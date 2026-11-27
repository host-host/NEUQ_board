#include "completions2claude.h"
#include <cctype>
#include <cstring>
#include <ctime>
#include <map>
#include <set>
#include <string>
#include <vector>
using namespace std;
#define ll long long
//映射规则参考 cc-switch proxy/providers/transform.rs(anthropic_to_openai/openai_to_anthropic)、streaming.rs，这里取反方向
//请求侧的历史清理、thinking开关规则参考 transform_codex_anthropic.rs（Responses->Anthropic，与本文件同方向）
static const double C2C_DEFAULT_MAX_TOKENS=128000;//claude必填，用户未给时的默认值，可用渠道append覆盖
struct c2c_state{
    string id,model;
    ll created=0;
    ll input=0,cache_read=0,cache_creation=0,output=0;
    int ntool=0;
    map<int,int> tool_index;//claude content block index -> completions tool_calls index
};

// ===== 请求转换 completions -> claude =====
static bool c2c_meaningful(const string& s){//cc-switch is_meaningful_text
    return s.find_first_not_of(" \t\r\n")!=string::npos;
}
static void c2c_add_text(cppJSON& blocks,const string& text,const cppJSON& cc){
    if(!c2c_meaningful(text))return;//claude拒绝空或纯空白的text块
    cppJSON b("{\"type\":\"text\"}");
    b.insert("text",text);
    if(cc.IsObject())b.insert("cache_control",cc);//保留客户端的缓存标记
    blocks.push_back(std::move(b));
}
static void c2c_add_image(cppJSON& blocks,const string& url,const cppJSON& cc){
    if(url.empty())return;
    cppJSON b("{\"type\":\"image\",\"source\":{}}");
    cppJSON src=b["source"];
    if(url.compare(0,5,"data:")==0){//data:image/png;base64,xxxx
        size_t pos=url.find(";base64,");
        if(pos==string::npos)return;
        src.insert("type","base64");
        src.insert("media_type",url.substr(5,pos-5));
        src.insert("data",url.substr(pos+8));
    }else{
        src.insert("type","url");
        src.insert("url",url);
    }
    if(cc.IsObject())b.insert("cache_control",cc);
    blocks.push_back(std::move(b));
}
//completions的content(字符串或parts数组) -> claude content块数组
static cppJSON c2c_blocks(const cppJSON& content){
    cppJSON blocks("[]");
    if(content.IsString()){
        c2c_add_text(blocks,content.valuestring(),cppJSON());
        return blocks;
    }
    for(cppJSON p:content){
        if(p.IsString()){
            c2c_add_text(blocks,p.valuestring(),cppJSON());
            continue;
        }
        string type=p["type"];
        if(type=="text")c2c_add_text(blocks,p["text"].valuestring(),p["cache_control"]);
        else if(type=="image_url"){
            cppJSON iu=p["image_url"];
            c2c_add_image(blocks,iu.IsString()?iu.valuestring():iu["url"].valuestring(),p["cache_control"]);
        }
        //input_audio/file 等claude不支持，丢弃
    }
    return blocks;
}
//追加一个块，若上一条消息同角色则合并（并行工具的多个tool_result必须在同一条user消息里）
static void c2c_push_block(cppJSON& msgs,const string& role,cppJSON&& block){
    int n=msgs.size();
    if(n){
        cppJSON last=msgs[n-1];
        if(last["role"]==role){
            last["content"].push_back(std::move(block));
            return;
        }
    }
    cppJSON m("{}");
    m.insert("role",role);
    m.insert("content",cppJSON("[]"));
    m["content"].push_back(std::move(block));
    msgs.push_back(std::move(m));
}
// ----- 历史清理（cc-switch transform_codex_anthropic.rs 同名逻辑） -----
static vector<string> c2c_ids(const cppJSON& m,const char* type,const char* field){
    vector<string> r;
    for(cppJSON b:m["content"])if(b["type"]==type)r.push_back(b[field].valuestring());
    return r;
}
//mode 0:去掉tool_result块  mode 1:tool_result块排到最前（claude要求tool_result先于text/image）
static cppJSON c2c_filter_blocks(const cppJSON& m,int mode){
    cppJSON r=m.clone(),c("[]");
    if(mode==1)for(cppJSON b:m["content"])if(b["type"]=="tool_result")c.push_back(b.clone());
    for(cppJSON b:m["content"])if(!(b["type"]=="tool_result"))c.push_back(b.clone());
    r.insert("content",std::move(c));
    return r;
}
//assistant的tool_use必须在紧随其后的user消息里全部有对应tool_result，否则整轮丢弃；孤立的tool_result也丢弃
static void c2c_drop_incomplete_tool_turns(cppJSON& msgs){
    cppJSON out("[]");
    int n=msgs.size();
    for(int i=0;i<n;){
        cppJSON m=msgs[i];
        vector<string> uses=m["role"]=="assistant"?c2c_ids(m,"tool_use","id"):vector<string>();
        if(!uses.empty()){
            cppJSON next=i+1<n?msgs[i+1]:cppJSON();
            bool paired=next["role"]=="user";
            vector<string> res=paired?c2c_ids(next,"tool_result","tool_use_id"):vector<string>();
            set<string> su(uses.begin(),uses.end()),sr(res.begin(),res.end());
            bool complete=paired&&!su.count("")&&!sr.count("")&&su.size()==uses.size()&&sr.size()==res.size()&&su==sr;
            if(complete){
                out.push_back(m.clone());
                out.push_back(next.clone());
            }else if(paired){
                cppJSON u2=c2c_filter_blocks(next,0);
                if(u2["content"].size())out.push_back(std::move(u2));
            }
            i+=paired?2:1;
            continue;
        }
        if(m["role"]=="user"){
            cppJSON u2=c2c_filter_blocks(m,0);
            if(u2["content"].size())out.push_back(std::move(u2));
        }else if(m["content"].size())out.push_back(m.clone());
        i++;
    }
    msgs.replace(out);
}
static void c2c_drop_empty(cppJSON& msgs){
    cppJSON out("[]");
    for(cppJSON m:msgs)if(m["content"].size())out.push_back(m.clone());
    msgs.replace(out);
}
static void c2c_ensure_leading_user(cppJSON& msgs){//claude要求第一条是user
    if(!msgs.size()||msgs[0]["role"]=="user")return;
    cppJSON out("[{\"role\":\"user\",\"content\":[{\"type\":\"text\",\"text\":\"(continuing the conversation)\"}]}]");
    for(cppJSON m:msgs)out.push_back(m.clone());
    msgs.replace(out);
}
static void c2c_trim_trailing_assistant(cppJSON& msgs){//claude拒绝以空白结尾的assistant预填
    int n=msgs.size();
    if(!n)return;
    cppJSON last=msgs[n-1];
    if(!(last["role"]=="assistant"))return;
    cppJSON c=last["content"];
    int k=c.size();
    if(!k)return;
    cppJSON b=c[k-1];
    if(!(b["type"]=="text"))return;
    string t=b["text"];
    size_t e=t.find_last_not_of(" \t\r\n");
    if(e==string::npos)c.pop_back();
    else if(e+1!=t.size())b.insert("text",t.substr(0,e+1));
}
//最后一轮是新的user提问才可开thinking；若是tool_result续写，紧邻的assistant必须带签名thinking块
static bool c2c_trailing_supports_thinking(const cppJSON& msgs){
    int n=msgs.size();
    if(!n)return false;
    cppJSON last=msgs[n-1];
    if(!(last["role"]=="user"))return false;
    vector<string> res=c2c_ids(last,"tool_result","tool_use_id");
    if(res.empty())return true;
    for(auto& id:res)if(id.empty())return false;
    if(n<2)return false;
    cppJSON pa=msgs[n-2];
    if(!(pa["role"]=="assistant"))return false;
    bool has_signed=false;
    set<string> uses;
    for(cppJSON b:pa["content"]){
        string t=b["type"];
        if(t=="thinking"||t=="redacted_thinking")has_signed=true;
        if(t=="tool_use")uses.insert(b["id"].valuestring());
    }
    if(!has_signed)return false;
    for(auto& id:res)if(!uses.count(id))return false;
    return true;
}
static void c2c_normalize(cppJSON& msgs){
    cppJSON out("[]");
    for(cppJSON m:msgs)out.push_back(m["role"]=="user"?c2c_filter_blocks(m,1):m.clone());
    msgs.replace(out);
    c2c_drop_incomplete_tool_turns(msgs);
    c2c_drop_empty(msgs);
    c2c_ensure_leading_user(msgs);
    c2c_trim_trailing_assistant(msgs);
    c2c_drop_empty(msgs);
}
//effort -> claude档位（cc-switch codex_effort_to_anthropic），空串表示不认识
static string c2c_effort(string e){
    for(char& ch:e)ch=tolower((unsigned char)ch);
    if(e=="minimal"||e=="low")return "low";
    if(e=="medium")return "medium";
    if(e=="high")return "high";
    if(e=="xhigh"||e=="max"||e=="ultra")return "max";
    return "";
}

void completions2claude_request(cppJSON& u,void*& internal){
    completions2claude_clean(internal);
    c2c_state* st=new c2c_state();
    st->created=time(0);
    st->model=u["model"].valuestring();
    internal=st;
    cppJSON r("{}"),system("[]"),msgs("[]");
    if(u["model"].IsString())r.insert("model",u["model"].valuestring());
    for(cppJSON m:u["messages"]){
        string role=m["role"];
        cppJSON content=m["content"];
        if(role=="system"||role=="developer"){//claude的system在顶层
            cppJSON b=c2c_blocks(content);
            for(cppJSON i:b)system.push_back(i.clone());
        }else if(role=="tool"||role=="function"){//tool结果 -> user消息里的tool_result块
            cppJSON tr("{\"type\":\"tool_result\"}");
            tr.insert("tool_use_id",m[role=="tool"?"tool_call_id":"name"].valuestring());
            cppJSON b=c2c_blocks(content);
            if(b.size())tr.insert("content",std::move(b));
            c2c_push_block(msgs,"user",std::move(tr));
        }else if(role=="assistant"){
            //reasoning_content没有signature，claude无法回传thinking块，丢弃（cc-switch同样只在特定上游保留）
            cppJSON b=c2c_blocks(content);
            for(cppJSON i:b)c2c_push_block(msgs,"assistant",i.clone());
            auto add_tool=[&](const string& id,const cppJSON& fn){
                cppJSON tu("{\"type\":\"tool_use\"}");
                tu.insert("id",id);
                tu.insert("name",fn["name"].valuestring());
                cppJSON args=fn["arguments"];
                cppJSON input=args.IsObject()?args.clone():cppJSON(args.valuestring().c_str());
                if(!input.IsObject())input=cppJSON("{}");//解析失败给空对象
                tu.insert("input",std::move(input));
                c2c_push_block(msgs,"assistant",std::move(tu));
            };
            for(cppJSON tc:m["tool_calls"])add_tool(tc["id"].valuestring(),tc["function"]);
            if(!m["tool_calls"].size()&&m["function_call"].IsObject())add_tool(m["function_call"]["name"].valuestring(),m["function_call"]);//旧格式
        }else{
            cppJSON b=c2c_blocks(content);
            for(cppJSON i:b)c2c_push_block(msgs,"user",i.clone());
        }
    }
    c2c_normalize(msgs);//去掉不完整的工具轮次、空消息，保证首条是user
    bool thinking_ok=c2c_trailing_supports_thinking(msgs);
    if(system.size())r.insert("system",std::move(system));
    r.insert("messages",std::move(msgs));
    cppJSON mt=u["max_completion_tokens"].IsNumber()?u["max_completion_tokens"]:u["max_tokens"];
    double max_tokens=mt.IsNumber()?mt.valuedouble():C2C_DEFAULT_MAX_TOKENS;//传多少就是多少
    for(const char* k:{"temperature","top_p","top_k","stream"})if(u.has(k)&&!u[k].IsNull())r.insert(k,u[k]);
    if(u["stop"].IsString()&&!u["stop"].valuestring().empty()){
        cppJSON s("[]");
        s.push_back(u["stop"].valuestring());
        r.insert("stop_sequences",std::move(s));
    }else if(u["stop"].IsArray()&&u["stop"].size())r.insert("stop_sequences",u["stop"]);
    if(u["user"].IsString()&&!u["user"].valuestring().empty()){
        cppJSON md("{}");
        md.insert("user_id",u["user"].valuestring());
        r.insert("metadata",std::move(md));
    }
    //tools: {type:function,function:{name,description,parameters}} -> {name,description,input_schema}
    cppJSON tools("[]");
    auto add_def=[&](const cppJSON& fn){
        if(!fn.IsObject()||fn["name"].valuestring().empty())return;
        cppJSON ct("{}");
        ct.insert("name",fn["name"].valuestring());
        if(fn["description"].IsString())ct.insert("description",fn["description"].valuestring());
        cppJSON schema=fn["parameters"].IsObject()&&fn["parameters"].size()?fn["parameters"].clone():cppJSON("{\"type\":\"object\",\"properties\":{}}");
        if(!(schema["type"]=="object"))schema.insert("type","object");//claude要求根schema是object
        ct.insert("input_schema",std::move(schema));
        if(fn["strict"].IsBool())ct.insert("strict",fn["strict"]);
        tools.push_back(std::move(ct));
    };
    for(cppJSON t:u["tools"])add_def(t["function"]);
    for(cppJSON f:u["functions"])add_def(f);//旧格式
    //tool_choice: auto/required/none/{type:function,function:{name}} -> {type:auto/any/none/tool}
    cppJSON tc=u["tool_choice"].IsNull()?cppJSON():u["tool_choice"],choice;
    string tcs=tc.IsString()?tc.valuestring():tc["type"].valuestring();
    if(tcs=="auto")choice=cppJSON("{\"type\":\"auto\"}");
    else if(tcs=="required")choice=cppJSON("{\"type\":\"any\"}");
    else if(tcs=="none")choice=cppJSON("{\"type\":\"none\"}");
    else if(tcs=="function"){
        choice=cppJSON("{\"type\":\"tool\"}");
        choice.insert("name",tc["function"]["name"].valuestring());
    }else if(tc)choice=cppJSON("{\"type\":\"auto\"}");//不认识的形式降级为auto，避免400
    if(u["parallel_tool_calls"]==false&&tools.size()){
        if(!choice)choice=cppJSON("{\"type\":\"auto\"}");
        if(!(choice["type"]=="none"))choice.insert("disable_parallel_tool_use",true);
    }
    if(tools.size()){//没有tools时claude不接受tool_choice
        r.insert("tools",std::move(tools));
        if(choice)r.insert("tool_choice",std::move(choice));
    }
    //thinking：优先透传客户端的thinking/output_config；否则reasoning_effort(或reasoning.effort) -> adaptive + output_config.effort
    //claude-5.x 不能关thinking（发disabled会400），所有"关闭"一律改成 adaptive+low（cc-switch thinking_cannot_be_disabled 同）
    const char* LOW="{\"effort\":\"low\"}";
    cppJSON thinking,oc;
    if(u["thinking"].IsObject()){
        thinking=u["thinking"].clone();
        if(u["output_config"].IsObject())oc=u["output_config"].clone();
        if(thinking["type"]=="disabled"){
            thinking=cppJSON("{\"type\":\"adaptive\"}");
            oc=cppJSON(LOW);
        }
    }else{
        string raw=u["reasoning_effort"].IsString()?u["reasoning_effort"].valuestring():u["reasoning"]["effort"].valuestring();
        string effort=c2c_effort(raw);
        if(!effort.empty()){
            thinking=cppJSON("{\"type\":\"adaptive\"}");
            oc=cppJSON("{}");
            oc.insert("effort",effort);
        }else if(raw=="none"){
            thinking=cppJSON("{\"type\":\"adaptive\"}");
            oc=cppJSON(LOW);
        }
        //不认识的effort不开thinking，保留采样参数（cc-switch同）
    }
    bool on=thinking.IsObject()&&!(thinking["type"]=="disabled");
    //工具续写轮即使没有签名thinking块也保持adaptive（关不掉），能否被上游接受待实测
    (void)thinking_ok;
    //开着thinking时claude不接受强制tool_choice(any/tool)：降级为auto，保留thinking和disable_parallel_tool_use
    string ct=r["tool_choice"]["type"];
    if(on&&(ct=="any"||ct=="tool")){
        cppJSON tcj=r["tool_choice"];
        tcj.insert("type","auto");
        tcj.erase("name");
    }
    if(thinking)r.insert("thinking",std::move(thinking));
    if(oc)r.insert("output_config",std::move(oc));
    if(on){//开启thinking时claude不允许改采样参数（cc-switch同样去掉temperature/top_p）
        r.erase("temperature");
        r.erase("top_p");
        r.erase("top_k");
    }
    r.insert("max_tokens",max_tokens);
    u.replace(r);
}

// ===== 响应转换 claude -> completions =====
//stop_reason -> finish_reason（cc-switch map_stop_reason 的反方向）
static const char* c2c_finish(const string& r){
    if(r.empty())return 0;
    if(r=="max_tokens")return "length";
    if(r=="tool_use")return "tool_calls";
    if(r=="refusal")return "content_filter";
    return "stop";//end_turn/stop_sequence/pause_turn
}
static inline ll c2c_max(ll a,double b){return a<(ll)b?(ll)b:a;}
static void c2c_read_usage(c2c_state* st,const cppJSON& usage){
    if(!usage.IsObject())return;
    st->input=c2c_max(st->input,usage["input_tokens"].valuedouble());
    st->cache_read=c2c_max(st->cache_read,usage["cache_read_input_tokens"].valuedouble());
    st->cache_creation=c2c_max(st->cache_creation,usage["cache_creation_input_tokens"].valuedouble());
    st->output=c2c_max(st->output,usage["output_tokens"].valuedouble());
}
//claude的input不含缓存，completions的prompt_tokens含缓存（与cc-switch build_anthropic_usage_json相反）
static cppJSON c2c_usage(c2c_state* st){
    ll prompt=st->input+st->cache_read+st->cache_creation;
    cppJSON u("{\"prompt_tokens_details\":{}}");
    u.insert("prompt_tokens",(double)prompt);
    u.insert("completion_tokens",(double)st->output);
    u.insert("total_tokens",(double)(prompt+st->output));
    u["prompt_tokens_details"].insert("cached_tokens",(double)st->cache_read);
    u["prompt_tokens_details"].insert("cache_write_tokens",(double)st->cache_creation);
    return u;
}
static cppJSON c2c_chunk(c2c_state* st){
    cppJSON c("{}");
    c.insert("id",st->id);
    c.insert("object","chat.completion.chunk");
    c.insert("created",(double)st->created);
    c.insert("model",st->model);
    c.insert("choices",cppJSON("[{\"index\":0,\"delta\":{},\"finish_reason\":null}]"));
    return c;
}
static cppJSON c2c_error(const cppJSON& a){
    cppJSON e("{}");
    e.insert("error",a["error"].IsObject()?a["error"]:a);//保留error.type，下游据此判断rate_limit_error等
    return e;
}
//一行SSE（claude格式）-> 最多一行 data:（completions格式），下游默认一行只有一个JSON
static void c2c_sse_line(string& resp,c2c_state* st){
    if(resp.compare(0,6,"event:")==0){resp.clear();return;}//completions没有event行
    if(resp.compare(0,6,"data: ")!=0)return;//空行原样保留，作为事件分隔
    cppJSON a(resp.c_str()+6);
    if(!a.IsObject())return;
    string type=a["type"];
    cppJSON c;
    if(type=="message_start"){
        cppJSON m=a["message"];
        if(!m["id"].valuestring().empty())st->id=m["id"].valuestring();
        if(!m["model"].valuestring().empty())st->model=m["model"].valuestring();
        c2c_read_usage(st,m["usage"]);
        c=c2c_chunk(st);
        c["choices"][0]["delta"].insert("role","assistant");
        c["choices"][0]["delta"].insert("content","");
    }else if(type=="content_block_start"){
        cppJSON b=a["content_block"];
        string bt=b["type"];
        if(bt=="tool_use"){
            int idx=st->ntool++;
            st->tool_index[(int)a["index"].valuedouble()]=idx;
            cppJSON call("{\"type\":\"function\",\"function\":{\"arguments\":\"\"}}");
            call.insert("index",(double)idx);
            call.insert("id",b["id"].valuestring());
            call["function"].insert("name",b["name"].valuestring());
            c=c2c_chunk(st);
            c["choices"][0]["delta"].insert("tool_calls",cppJSON("[]"));
            c["choices"][0]["delta"]["tool_calls"].push_back(std::move(call));
        }else if(bt=="text"&&!b["text"].valuestring().empty()){
            c=c2c_chunk(st);
            c["choices"][0]["delta"].insert("content",b["text"].valuestring());
        }else if(bt=="thinking"&&!b["thinking"].valuestring().empty()){
            c=c2c_chunk(st);
            c["choices"][0]["delta"].insert("reasoning_content",b["thinking"].valuestring());
        }
    }else if(type=="content_block_delta"){
        cppJSON d=a["delta"];
        string dt=d["type"];
        if(dt=="text_delta"){
            c=c2c_chunk(st);
            c["choices"][0]["delta"].insert("content",d["text"].valuestring());
        }else if(dt=="thinking_delta"){
            c=c2c_chunk(st);
            c["choices"][0]["delta"].insert("reasoning_content",d["thinking"].valuestring());
        }else if(dt=="input_json_delta"){
            auto it=st->tool_index.find((int)a["index"].valuedouble());
            if(it!=st->tool_index.end()&&!d["partial_json"].valuestring().empty()){
                cppJSON call("{\"function\":{}}");
                call.insert("index",(double)it->second);
                call["function"].insert("arguments",d["partial_json"].valuestring());
                c=c2c_chunk(st);
                c["choices"][0]["delta"].insert("tool_calls",cppJSON("[]"));
                c["choices"][0]["delta"]["tool_calls"].push_back(std::move(call));
            }
        }
        //signature_delta 等completions无对应，丢弃
    }else if(type=="message_delta"){//结束原因+完整usage合并到同一个chunk
        c2c_read_usage(st,a["usage"]);
        c=c2c_chunk(st);
        const char* fr=c2c_finish(a["delta"]["stop_reason"].valuestring());
        if(fr)c["choices"][0].insert("finish_reason",fr);
        c.insert("usage",c2c_usage(st));
    }else if(type=="message_stop"){
        resp="data: [DONE]\n";
        return;
    }else if(type=="error")c=c2c_error(a);
    //ping/content_block_stop 不输出
    resp=c?"data: "+c.stringify_Unformatted()+"\n":string();
}
//非流式 claude message -> chat.completion（cc-switch openai_to_anthropic 的反方向）
static void c2c_whole(string& resp,c2c_state* st){
    cppJSON a(resp.c_str());
    if(!a.IsObject())return;
    if(a["type"]=="error"||a["error"].IsObject()){
        resp=c2c_error(a).stringify_Unformatted();
        return;
    }
    if(!(a["type"]=="message"))return;
    if(!a["id"].valuestring().empty())st->id=a["id"].valuestring();
    if(!a["model"].valuestring().empty())st->model=a["model"].valuestring();
    c2c_read_usage(st,a["usage"]);
    string text,thinking;
    cppJSON calls("[]");
    for(cppJSON b:a["content"]){
        string bt=b["type"];
        if(bt=="text")text+=b["text"].valuestring();
        else if(bt=="thinking")thinking+=b["thinking"].valuestring();
        else if(bt=="tool_use"){
            cppJSON call("{\"type\":\"function\",\"function\":{}}");
            call.insert("id",b["id"].valuestring());
            call["function"].insert("name",b["name"].valuestring());
            call["function"].insert("arguments",b["input"]?b["input"].stringify_Unformatted():string("{}"));
            calls.push_back(std::move(call));
        }
    }
    cppJSON msg("{\"role\":\"assistant\"}");
    if(text.empty()&&calls.size())msg.insert("content",(const char*)0);
    else msg.insert("content",text);
    if(!thinking.empty())msg.insert("reasoning_content",thinking);
    if(calls.size())msg.insert("tool_calls",std::move(calls));
    cppJSON r("{}");
    r.insert("id",st->id);
    r.insert("object","chat.completion");
    r.insert("created",(double)st->created);
    r.insert("model",st->model);
    cppJSON choice("{\"index\":0}");
    choice.insert("message",std::move(msg));
    const char* fr=c2c_finish(a["stop_reason"].valuestring());
    choice.insert("finish_reason",fr?fr:"stop");
    r.insert("choices",cppJSON("[]"));
    r["choices"].push_back(std::move(choice));
    r.insert("usage",c2c_usage(st));
    resp=r.stringify_Unformatted();
}

void completions2claude_response(string& resp,void* internal,bool issse){
    c2c_state local;
    c2c_state* st=internal?(c2c_state*)internal:&local;
    if(!st->created)st->created=time(0);
    if(issse)c2c_sse_line(resp,st);
    else c2c_whole(resp,st);
}

void completions2claude_clean(void* internal){
    delete (c2c_state*)internal;
}
