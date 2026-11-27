#include "gptapi6.h"
#include "completions2claude.h"
#include <bits/stdc++.h>
#include <curl/curl.h>
#include <fcntl.h>
#include <pthread.h>
#include <sys/stat.h>
#include <unistd.h>
#include <zlib.h>
using namespace std;
#define ll long long
static const ll NANOSECONDS_PER_SECOND=1000000000LL;
ll gpt6_now_ns(){
    timespec now;
    if(clock_gettime(CLOCK_REALTIME,&now))return (ll)time(0)*NANOSECONDS_PER_SECOND;
    return (ll)now.tv_sec*NANOSECONDS_PER_SECOND+now.tv_nsec;
}
static inline ll max(ll a,ll b){
    return a<b?b:a;
}
static void gpt6_mark_first(gpt6_ret* ans){
    if(!ans->first_ns)ans->first_ns=gpt6_now_ns();
}
static void append(cppJSON& object,const char* key,const string& text) {
    if(object.IsObject()&&!text.empty())object.insert(key,object[key].valuestring()+text);
}
static void gpt6_set_info(gpt6_ret* ans,const cppJSON& info) {
    if(info)ans->info=info.stringify_Unformatted();
}
static bool same(const cppJSON& a,const cppJSON& b) {
    if(!a.IsObject()||!b.IsObject())return 0;
    if(a.has("functionCall")||b.has("functionCall"))return 0;//每个 functionCall 都是独立调用，不能合并
    if(a["text"].IsString()&&b["text"].IsString())return (a["thought"]==true)==(b["thought"]==true);
    return a.has("thoughtSignature")&&b.has("thoughtSignature");
}
void fakecache(gpt6_ret* ans){
    ll m=min(ans->fakewant,ans->input-14)-ans->cache;
    if(m<0)m=0;
    ans->cache+=m;
    ll mm=std::min(m,ans->makecache);
    ans->makecache-=mm;
    ans->makecache_1h=std::min(ans->makecache_1h,ans->makecache);
    if(ans->hasmakecache&&ans->makecache==0&&ans->input-26>=1024)ans->makecache=std::max(ans->input-26-ans->cache,0ll);
}
void gpt6_parse_responses(gpt6_ret* ans,string& resp,bool issse){
    /**
        "usage": {
            "input_tokens": 17,
            "input_tokens_details": {
                "cache_write_tokens": 0,
                "cache_write_tokens_details": {
                    "ttl_12h_tokens": 0,
                    "ttl_30m_tokens": 0
                },
                "cached_tokens": 0
            },
            "output_tokens": 23,
            "output_tokens_details": {
                "reasoning_tokens": 10
            },
            "total_tokens": 40
        },
     */
    auto f=[](gpt6_ret* ans,cppJSON t){
        if(!t)return;
        ans->used_tokens=t["total_tokens"].valuedouble();
        ans->input=t["input_tokens"].valuedouble();//input包含cache
        ans->costcache=ans->cache=t["input_tokens_details"]["cached_tokens"].valuedouble();
        ans->costmakecache=ans->makecache=t["input_tokens_details"]["cache_write_tokens"].valuedouble();//makecache==0
        ans->output=t["output_tokens"].valuedouble();
        if(ans->isauto){
            fakecache(ans);
            char ttt[1024];
            sprintf(ttt,R"({"input_tokens": %lld,"input_tokens_details": {"cache_write_tokens": %lld,"cache_write_tokens_details": 
                {"ttl_12h_tokens": %lld,"ttl_30m_tokens": %lld},"cached_tokens": %lld},"output_tokens": %lld,"output_tokens_details": {"reasoning_tokens": %lld},"total_tokens": %lld})",
                ans->input,ans->makecache,0ll,ans->makecache,ans->cache,ans->output,(ll)t["output_tokens_details"]["reasoning_tokens"].valuedouble(),ans->used_tokens);
            t.replace(cppJSON(ttt));
        }
    };
    if(issse&&resp.find("data: ")==0){
        cppJSON a(&resp[6]);
        if(!a)return;
        string type=a["type"];
        if(type=="response.output_item.added")gpt6_mark_first(ans);
        if(type=="response.output_item.done")gpt6_mark_first(ans);
        if(type=="response.output_text.delta")gpt6_mark_first(ans);
        if(type=="response.output_text.done")gpt6_mark_first(ans);
        if(type=="response.output_item.done")ans->append.push_back(a["item"].clone());
        if(type=="response.completed"){
            ans->response_id=a["response"]["id"];
            f(ans,a["response"]["usage"]);
            if(ans->isauto)resp="data: "+a.stringify_Unformatted()+"\n";
        }
        if(type=="response.failed"){
            ans->stable=1001;
            cppJSON info=a["response"]["error"];
            gpt6_set_info(ans,info?info:a);
        }
        if(type=="error"){
            ans->stable=1002;
            cppJSON info=a["error"];
            gpt6_set_info(ans,info?info:a);
        }
    }else {
        cppJSON a(resp.c_str());
        if(!a)return;
        if(!issse){
            ans->append=a["output"].clone();
            ans->response_id=a["id"].valuestring();
            f(ans,a["usage"]);
            if(ans->isauto)resp=a.stringify_Unformatted();
        }
        gpt6_set_info(ans,a["error"]);
        if(a["code"]=="GROUP_DISABLED")ans->stable=1007;
        string p=a["error"]["type"];
        if(p=="rate_limit_error")ans->stable=1004;
        if(p=="model_not_found"||a["error"]["code"]=="model_not_found")ans->stable=1005;
        if(p=="upstream_error")ans->stable=1006;
        if(p=="invalid_request_error")ans->stable=2;
    }
}
void gpt6_parse_completions(gpt6_ret* ans,string& resp,bool issse){
    /**
    "usage": {
        "prompt_tokens": 73,
        "completion_tokens": 72,
        "total_tokens": 145,
        "prompt_tokens_details": {
            "cached_tokens": 0,
            "cache_write_tokens": 0,
            "audio_tokens": 0,
            "video_tokens": 0
        },
        "completion_tokens_details": {
            "reasoning_tokens": 28,
            "image_tokens": 0,
            "audio_tokens": 0,
            "accepted_prediction_tokens": 0,
            "rejected_prediction_tokens": 0
        }
    }
     */
    auto f=[](gpt6_ret* ans,cppJSON usage){
        if(!usage.IsObject())return;
        ans->input=max(ans->input,usage["prompt_tokens"].valuedouble());
        ans->output=max(ans->output,usage["completion_tokens"].valuedouble());
        ans->costcache=ans->cache=max(ans->costcache,usage["prompt_tokens_details"]["cached_tokens"].valuedouble());
        ans->costmakecache=ans->makecache=max(ans->costmakecache,usage["prompt_tokens_details"]["cache_write_tokens"].valuedouble());
        ans->used_tokens=max(ans->used_tokens,max(usage["total_tokens"].valuedouble(),ans->input+ans->output));
        if(ans->isauto){
            fakecache(ans);
            char ttt[1024];
            sprintf(ttt,R"({"prompt_tokens": %lld,"completion_tokens": %lld,"total_tokens": %lld,"prompt_tokens_details": {"cached_tokens": %lld,"cache_write_tokens": %lld,"audio_tokens": %lld,
                "video_tokens": %lld},"completion_tokens_details": {"reasoning_tokens": %lld,"image_tokens": %lld,"audio_tokens": %lld,"accepted_prediction_tokens": %lld,"rejected_prediction_tokens": %lld}})",
                ans->input,ans->output,ans->used_tokens,ans->cache,ans->makecache,0ll,0ll,(ll)usage["completion_tokens_details"]["reasoning_tokens"].valuedouble(),0ll,0ll,0ll,0ll);
            usage.replace(cppJSON(ttt));
        }
    };
    if(issse&&resp.find("data: ")==0){
        cppJSON res(resp.data()+6);
        if(!res.IsObject())return;
        gpt6_set_info(ans,res["error"]);
        f(ans,res["usage"]);
        if(ans->isauto)resp="data: "+res.stringify_Unformatted()+"\n";
        if(ans->response_id.empty())ans->response_id=res["id"].valuestring();
        cppJSON delta=res["choices"][0]["delta"];
        if(!delta.IsObject())return;
        if(!delta["content"].valuestring().empty()||!delta["reasoning_content"].valuestring().empty()||!delta["reasoning"].valuestring().empty())gpt6_mark_first(ans);
        if(!ans->append[0].IsObject())ans->append=cppJSON("[{\"role\":\"assistant\",\"content\":\"\"}]");
        if(delta["reasoning_content"].IsString())ans->append[0].insert("reasoning_content",ans->append[0]["reasoning_content"].valuestring()+delta["reasoning_content"].valuestring());
        string tmp=delta["content"];
        if(!tmp.empty())ans->append[0]["content"]=ans->append[0]["content"].valuestring()+tmp;
        cppJSON tool_calls=delta["tool_calls"];
        if(tool_calls.IsArray()){//此处未验证过
            gpt6_mark_first(ans);
            if(!ans->append[0]["tool_calls"].IsArray())ans->append[0].insert("tool_calls",cppJSON("[]"));
            cppJSON calls=ans->append[0]["tool_calls"];
            for(cppJSON item:tool_calls){
                int index=item["index"].IsNumber()?(int)item["index"].valuedouble():calls.size();
                if(index<0)continue;
                while(calls.size()<=index)calls.push_back(cppJSON("{}"));
                cppJSON call=calls[index];
                if(!item["id"].valuestring().empty())call.insert("id",item["id"].valuestring());
                if(!item["type"].valuestring().empty())call.insert("type",item["type"].valuestring());
                cppJSON function=item["function"];
                if(!function.IsObject())continue;
                if(!call["function"].IsObject())call.insert("function",cppJSON("{}"));
                cppJSON target=call["function"];
                if(!function["name"].valuestring().empty())target.insert("name",function["name"].valuestring());
                if(function["arguments"].IsString())append(target,"arguments",function["arguments"].valuestring());
            }
            if(ans->append[0]["content"].valuestring().empty())ans->append[0].insert("content",(const char*)0);
        }
    }else{
        cppJSON a(resp.c_str());
        if(!a)return;
        if(!issse){
            gpt6_set_info(ans,a["error"]);
            f(ans,a["usage"]);
            if(ans->isauto)resp=a.stringify_Unformatted();
            cppJSON message=a["choices"][0]["message"].clone();
            if(message)ans->append.push_back(std::move(message));
            ans->response_id=a["id"].valuestring();
        }
        gpt6_set_info(ans,a["error"]);
        string p=a["error"]["type"];
        if(p=="rate_limit_error")ans->stable=1001;
        if(p=="model_not_found")ans->stable=1002;
    }
}
void gpt6_parse_claude(gpt6_ret* ans,string& resp,bool issse){
    /*
    "usage": {
        "input_tokens": 2,
        "output_tokens": 1730,
        "output_tokens_details": {
            "thinking_tokens": 78
        },
        "cache_creation_input_tokens": 37636,
        "cache_read_input_tokens": 0,
        "cache_creation": {
            "ephemeral_5m_input_tokens": 0,
            "ephemeral_1h_input_tokens": 37636
        }
    }
    */
    auto f=[](gpt6_ret* ans,cppJSON usage){
        if(!usage.IsObject())return;
        ll input=usage["input_tokens"].valuedouble();
        ans->output=max(ans->output,usage["output_tokens"].valuedouble());
        ans->costcache=ans->cache=usage["cache_read_input_tokens"].valuedouble();
        ans->costmakecache_1h=ans->makecache_1h=usage["cache_creation"]["ephemeral_1h_input_tokens"].valuedouble();
        ans->costmakecache=ans->makecache=max((ll)usage["cache_creation_input_tokens"].valuedouble(),ans->makecache_1h);
        ans->input=input+ans->cache+ans->makecache;
        ans->used_tokens=ans->input+ans->output;
        if(ans->isauto){
            fakecache(ans);
            char ttt[1024];
            sprintf(ttt,R"({"input_tokens": %lld,"output_tokens": %lld,"output_tokens_details": {"thinking_tokens": %lld},"cache_creation_input_tokens": %lld,
                "cache_read_input_tokens": %lld,"cache_creation": {"ephemeral_5m_input_tokens": %lld,"ephemeral_1h_input_tokens": %lld}})",
                ans->input-ans->cache-ans->makecache,ans->output,(ll)usage["output_tokens_details"]["thinking_tokens"].valuedouble(),
                ans->makecache,ans->cache,ans->makecache-ans->makecache_1h,ans->makecache_1h);
            usage.replace(cppJSON(ttt));
        }
    };
    if(issse&&resp.find("data: ")==0){
        cppJSON res(resp.data()+6),append,tmp;
        if(!res)return;
        gpt6_set_info(ans,res["error"]);
        f(ans,res["usage"]);
        f(ans,res["message"]["usage"]);
        if(ans->isauto)resp="data: "+res.stringify_Unformatted()+"\n";
        if(ans->response_id.empty())ans->response_id=res["message"]["id"].valuestring();
        string type=res["type"];
        if(!ans->append.size())ans->append=cppJSON(R"([{"role":"assistant","content":[]}])");
        append=ans->append[0]["content"];
        if(type=="content_block_start")append.push_back(res["content_block"].clone());
        tmp=append[int(res["index"].valuedouble())];
        if(type=="content_block_start")gpt6_mark_first(ans);
        if(type=="content_block_delta"){
            gpt6_mark_first(ans);
            for(cppJSON i:res["delta"]){
                string name=i.namestring();
                if(name=="type")continue;
                tmp.insert(name.c_str(),tmp[name].valuestring()+i.valuestring());
            }
        }
        if(type=="content_block_stop"&&tmp["partial_json"].IsString()){
            cppJSON input(tmp["partial_json"].valuestring().c_str());
            if(input)tmp.insert("input",std::move(input));
            tmp.erase("partial_json");
        }
        if(type=="error")ans->stable=1001;
    }else{
        cppJSON res(resp.c_str());
        if(!res)return;
        if(!issse){//未验证
            gpt6_set_info(ans,res["error"]);
            f(ans,res["usage"]);
            if(ans->isauto)resp=res.stringify_Unformatted();
            ans->response_id=res["id"].valuestring();
            if(!(res["type"]=="message"))return;
            cppJSON history("{}");
            string role=res["role"].valuestring();
            history.insert("role",role.empty()?"assistant":role);
            history.insert("content",res["content"].IsArray()||res["content"].IsString()?res["content"].clone():cppJSON("[]"));
            ans->append=cppJSON("[]");
            ans->append.push_back(std::move(history));
        }
        // cppJSON a(&tmp[0]);
        // string p=a["error"]["type"];
        // if(p=="error")ans->stable=3;
    }
}
void gpt6_parse_gemini(gpt6_ret* ans,string& tmp,bool issse){//未验证
    //暂不做faketoken
    auto f=[](gpt6_ret* ans,cppJSON usage){
        if(!usage.IsObject())return;
        ans->input=max(ans->input,usage["promptTokenCount"].valuedouble());
        ans->output=max(ans->output,usage["candidatesTokenCount"].valuedouble()+usage["thoughtsTokenCount"].valuedouble());
        ans->costcache=ans->cache=max(ans->cache,usage["cachedContentTokenCount"].valuedouble());
        ans->used_tokens=max(ans->used_tokens,usage["totalTokenCount"].valuedouble());
    };
    if(issse&&tmp.find("data: ")==0){
        cppJSON res(tmp.data()+6),parts,last;
        gpt6_set_info(ans,res["error"]);
        f(ans,res["usageMetadata"]);
        if(!res["responseId"].valuestring().empty())ans->response_id=res["responseId"].valuestring();
        cppJSON chunk=res["candidates"][0]["content"];
        if(!chunk["parts"].size())return;
        gpt6_mark_first(ans);
        if(!ans->append.size())ans->append=cppJSON(R"([{"role":"model","parts":[]}])");
        if(!chunk["role"].valuestring().empty())ans->append[0].insert("role",chunk["role"].valuestring());
        parts=ans->append[0]["parts"];
        for(cppJSON part:chunk["parts"]){//gemini 每个 chunk 都是新片段，同类型的合并到上一个 part
            last=parts[parts.size()-1];
            if(!same(last,part))parts.push_back(part.clone());
            else if(part["text"].IsString())last.insert("text",last["text"].valuestring()+part["text"].valuestring());
            else last.replace(part);
        }
        return;
    }else{
        if(!issse){
            cppJSON res(tmp.c_str(),(int)tmp.size());
            gpt6_set_info(ans,res["error"]);
            f(ans,res["usageMetadata"]);
            ans->response_id=res["responseId"].valuestring();
            cppJSON chunk=res["candidates"][0]["content"];
            if(!chunk["parts"].size())return;
            cppJSON history=chunk.clone();
            if(history["role"].valuestring().empty())history.insert("role","model");
            ans->append=cppJSON("[]");
            ans->append.push_back(std::move(history));
        }
        cppJSON a(&tmp[0]);
        cppJSON p=a["error"];
        if(p["status"]=="INVALID_ARGUMENT")ans->stable=3;
    }
}
void gpt6_parse_image(gpt6_ret* ans,string& tmp,bool issse) {
    if(issse)return;//图像好像都是非流
    cppJSON res(tmp.c_str());
    gpt6_set_info(ans,res["error"]);
    if(res["data"]){
        ans->used_tokens=1;
        ans->stable=1;
        gpt6_mark_first(ans);
    }
}
void gpt6_parse2(gpt6_ret* ans,string &tmp,bool issse){
    if(strcmp(ans->format,"responses")==0)gpt6_parse_responses(ans,tmp,issse);
    if(strcmp(ans->format,"completions")==0)gpt6_parse_completions(ans,tmp,issse);
    if(strcmp(ans->format,"claude")==0)gpt6_parse_claude(ans,tmp,issse);
    if(strcmp(ans->format,"gemini")==0)gpt6_parse_gemini(ans,tmp,issse);
    if(strcmp(ans->format,"image")==0)gpt6_parse_image(ans,tmp,issse);
    if(ans->a)ans->ca+=tmp;
}
void gpt6_parse(gpt6_ret* ans,string tmp,bool issse){//处理 append used_tokens input output cache makecache first response_id
    while(!tmp.empty()&&tmp.back()=='\r')tmp.pop_back();
    if(ans->change_response)ans->change_response(tmp,ans->internal,issse);
    if(ans->change_response&&issse){
        size_t u;
        while((u=tmp.find('\n'))!=string::npos){
            string line=tmp.substr(0,u+1);
            gpt6_parse2(ans,line,issse);
            tmp.erase(0,u+1);
        }
        if(!tmp.empty())gpt6_parse2(ans,tmp,issse);
    }else gpt6_parse2(ans,tmp,issse);
}
static size_t gpt6header(char* ptr,size_t size,size_t count,void* userdata) {
    gpt6_ret* p=(gpt6_ret*)userdata;
    size_t n=size*count;
    p->header+=string((char*)ptr,n);
    return n;
}
void gpt6_flush(gpt6_ret* ans){
    if(!ans->send)return;
    if(ans->a)http_send(ans->a,0,ans->ca.data(),ans->ca.size());
    ans->ca.clear();
}
static size_t gpt6body(void* ptr,size_t size,size_t count,void* userdata) {
    gpt6_ret* ans=(gpt6_ret*)userdata;
    size_t n=size*count;
    if(!n)return 0;
    if(!ans->issse) {
        size_t pos=ans->header.rfind("HTTP/");
        int status=500;
        if(pos!=string::npos)sscanf(ans->header.c_str()+pos,"HTTP/%*s %d",&status);
        ans->issse=strcasestr(ans->header.c_str(),"text/event-stream")?2:1;
        string head=httpcode(status);
        head+=ans->issse==2?Hsse Hc0 Hclo "\r\n":Hjson Hc0 Hclo "\r\n";
        if(ans->a)ans->ca+=head;
    }
    string tmp((char*)ptr,n);
    if(ans->issse==2)ans->bodydelta+=tmp;
    ans->body+=tmp;
    if(ans->issse==2&&tmp.find('\n')!=string::npos){
        size_t u=0;
        while((u=ans->bodydelta.find('\n'))!=string::npos){
            gpt6_parse(ans,ans->bodydelta.substr(0,u+1),true);
            ans->bodydelta=ans->bodydelta.substr(u+1);
        }
    }
    for(char i:tmp)if(i!='\r'&&i!='\n'&&i!=' '){
        ans->last_ns=gpt6_now_ns();
        break;
    }
    if(ans->first_ns&&ans->stable<1000)ans->send=1;
    if(ans->send)gpt6_flush(ans);
    else if(ans->stable>=1000)return 0;//渠道炸了，立刻切下一个渠道
    return n;
}
static bool gpt6_gzip(const string& input,string& output){
    if(input.size()<1024||input.size()>UINT_MAX)return false;
    z_stream stream{};
    if(deflateInit2(&stream,Z_BEST_SPEED,Z_DEFLATED,MAX_WBITS+16,8,Z_DEFAULT_STRATEGY)!=Z_OK)return false;
    stream.next_in=(Bytef*)input.data();
    stream.avail_in=(uInt)input.size();
    int result;
    do{
        char buffer[16384];
        stream.next_out=(Bytef*)buffer;
        stream.avail_out=sizeof(buffer);
        result=deflate(&stream,Z_FINISH);
        output.append(buffer,sizeof(buffer)-stream.avail_out);
    }while(result==Z_OK&&output.size()<input.size());
    deflateEnd(&stream);
    if(result!=Z_STREAM_END||output.size()+sizeof("Content-Encoding: gzip\r\n")-1>=input.size()){
        output.clear();
        return false;
    }
    return true;
}
static void gpt6_chat_res2recon(cppJSON& request){
    if(!request["messages"].IsArray())return;
    for(cppJSON item:request["messages"]){
        if(item.has("tool_calls")&&!item.has("reasoning_content"))item.insert("reasoning_content","");
    }
}
static void gpt6_responses_sum2rea(cppJSON& request){//这个函数ai写的！
    if(!request["input"].IsArray())return;
    for(cppJSON item:request["input"]){
        if(!(item["type"]=="reasoning")||!item["summary"].IsArray())continue;
        cppJSON content=item["content"];
        if(content&&!content.IsNull()&&!content.IsArray())continue;
        bool has_reasoning=false;
        for(cppJSON part:content)if(part["type"]=="reasoning_text"&&!part["text"].valuestring().empty())has_reasoning=true;
        if(has_reasoning)continue;//已有原生推理正文，不用摘要覆盖或重复追加
        cppJSON converted=content.IsArray()?content.clone():cppJSON("[]"),remaining("[]");
        bool changed=false;
        for(cppJSON part:item["summary"]){
            if(part["type"]=="summary_text"&&!part["text"].valuestring().empty()){
                cppJSON text("{\"type\":\"reasoning_text\"}");
                text.insert("text",part["text"].valuestring());
                converted.push_back(std::move(text));
                changed=true;
            }else remaining.push_back(part.clone());
        }
        if(!changed)continue;//缺少摘要时不伪造推理内容
        item.insert("content",std::move(converted));
        item.insert("summary",std::move(remaining));
    }
}
gpt6_ret gpt6_work3(http_para* a,const char* message,const char* model,cppJSON conf,const char* format,bool send_before_first,ll fakewant,bool hasmakecache,bool isauto){
    if(conf["map"][model].IsString())model=conf["map"][model].a->valuestring;
    gpt6_ret ans{};
    ans.fakewant=fakewant;
    ans.isauto=isauto;
    ans.hasmakecache=hasmakecache;
    ans.send=send_before_first;
    ans.a=a;
    ans.format=format;
    ans.append=cppJSON("[]");
    // ans.start_ns=gpt6_now_ns();
    CURL* curl=curl_easy_init();
    if(!curl) {
        ans.curlcode=CURLE_FAILED_INIT;
        ans.info=curl_easy_strerror(CURLE_FAILED_INIT);
        return ans;
    }
    char curl_error[CURL_ERROR_SIZE]={0};
    string Authorization=conf["Authorization"],url=conf["url"],mm,proxy=conf["proxy"],realf=conf["fmap"][format];
    if(realf.empty())realf=format;
    if(conf.has("url_claude")&&realf=="claude")url=conf["url_claude"];//单独适配deepseek的两种格式url不同
    struct curl_slist* headers=curl_slist_append(0,"Content-Type: application/json");
    if(realf=="responses"||realf=="completions"||realf=="image")Authorization="Authorization: Bearer "+Authorization;
    if(realf=="claude")Authorization="x-api-key: "+Authorization;
    if(realf=="gemini")Authorization="x-goog-api-key: "+Authorization;
    headers=curl_slist_append(headers,Authorization.c_str());
    if(realf=="claude")headers=curl_slist_append(headers,"anthropic-version: 2023-06-01");
    if(!url.empty()&&url.back()=='/')url.pop_back();
    if(realf=="responses")url+="/v1/responses";
    if(realf=="completions")url+="/v1/chat/completions";
    if(realf=="claude")url+="/v1/messages";
    if(realf=="image")url+="/v1/images/generations";
    if(realf=="gemini")url=url+"/v1beta/models/"+model+":streamGenerateContent?alt=sse";
    curl_easy_setopt(curl,CURLOPT_URL,url.c_str());
    cppJSON u(message);
    u.erase("nit_web_provider");//自定义字段
    if(conf["fmap"][format].IsString()){
        if(strcmp(format,"completions")==0&&realf=="claude"){
            completions2claude_request(u,ans.internal);
            ans.change_response=completions2claude_response;
            ans.change_clean=completions2claude_clean;
        }
    }
    for(auto i:conf["append"])u.insert(i.a->string,i);//执行消息内容追加
    if(realf=="completions"&&conf["responses_sum2rea"]==true)gpt6_chat_res2recon(u);
    if(realf=="responses"&&conf["responses_sum2rea"]==true)gpt6_responses_sum2rea(u);//deepseek特例，需要 return the reasoning_content
    if(realf=="completions"&&u["stream"]==true){
        if(!u["stream_options"].IsObject())u.insert("stream_options",cppJSON("{}"));
        u["stream_options"].insert("include_usage",true);
    }
    if(realf!="gemini")u["model"]=model;//执行名字map映射
    mm=u.stringify_Unformatted();
    u.clear();//提前释放，防止内存爆
    if(conf["gzip"]==true){
        string compressed;
        if(gpt6_gzip(mm,compressed)){
            headers=curl_slist_append(headers,"Content-Encoding: gzip");
            mm.swap(compressed);
        }
    }
    curl_easy_setopt(curl,CURLOPT_HTTPHEADER,headers);
    curl_easy_setopt(curl,CURLOPT_POSTFIELDSIZE_LARGE,(curl_off_t)mm.size());
    curl_easy_setopt(curl,CURLOPT_POSTFIELDS,mm.c_str());
    curl_easy_setopt(curl,CURLOPT_HEADERFUNCTION,gpt6header);
    curl_easy_setopt(curl,CURLOPT_HEADERDATA,&ans);
    curl_easy_setopt(curl,CURLOPT_WRITEFUNCTION,gpt6body);
    curl_easy_setopt(curl,CURLOPT_WRITEDATA,&ans);
    curl_easy_setopt(curl,CURLOPT_ERRORBUFFER,curl_error);
    curl_easy_setopt(curl,CURLOPT_CONNECTTIMEOUT,60L);
    curl_easy_setopt(curl,CURLOPT_TIMEOUT,3600L);
    curl_easy_setopt(curl,CURLOPT_NOSIGNAL,1L);
    if(!proxy.empty()){
        curl_easy_setopt(curl,CURLOPT_PROXY,proxy.c_str());
        curl_easy_setopt(curl,CURLOPT_NOPROXY,"");
    }
    ans.curlcode=curl_easy_perform(curl);
    ans.end_ns=gpt6_now_ns();
    if(!ans.issse)gpt6body(0,0,0,&ans);
    curl_easy_getinfo(curl,CURLINFO_RESPONSE_CODE,&ans.httpcode);
    curl_off_t upload_bytes=0;
    curl_easy_getinfo(curl,CURLINFO_SIZE_UPLOAD_T,&upload_bytes);
    ans.upload_bytes=upload_bytes;
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);
    if(ans.issse==1)gpt6_parse(&ans,ans.body,false);
    if(!ans.bodydelta.empty())gpt6_parse(&ans,ans.bodydelta,true);
    if(ans.httpcode/100==5||ans.httpcode==429)ans.stable=ans.httpcode+1000;
    if(ans.curlcode!=0)ans.stable=1000;
    if(ans.last_ns==0)ans.stable=3000;//没有响应体
    if(ans.info.empty()&&ans.curlcode!=0)ans.info=curl_error[0]?curl_error:curl_easy_strerror((CURLcode)ans.curlcode);
    if(ans.info.empty()&&ans.httpcode>=400)ans.info=ans.body;
    if(ans.used_tokens)ans.stable=ans.stable<=1?1/*正常*/:0/*有花费并且有问题，需要写日志人工看看*/;
    if(ans.send==0&&ans.stable==1)ans.send=1;
    gpt6_flush(&ans);
    if(ans.change_clean)ans.change_clean(ans.internal);
    return ans;
}
string gpt6_request_model(http_para* a,const cppJSON& request,const string& format) {
    if(format!="gemini")return request["model"].valuestring();
    if(!a||!a->get)return string();
    string request_line(a->get,(size_t)a->n);
    size_t begin=request_line.find("/models/"),end=begin==string::npos?string::npos:request_line.find(":streamGenerateContent",begin);
    if(end==string::npos)return string();
    string model=request_line.substr(begin+8,end-begin-8);
    for(char c:model)if(!isalnum((unsigned char)c)&&c!='-'&&c!='_'&&c!='.')return string();
    return model;
}
static const set<string>completions_allow={"role","tool_calls","tool_call_id","content","name","function_call","refusal","audio","reasoning_content"};
static const set<string>responses_allow={"type","call_id","output","name","input","role","content","encrypted_content"};
static const set<string>claude_allow={"role","content"};
cppJSON my_format(const cppJSON& a,const string& format,int k){
    if(a.IsArray()){
        cppJSON result("[]");
        for(cppJSON item:a)result.push_back(my_format(item,format,k-1));
        return result;
    }
    if(a.IsObject()){
        map<string,cppJSON> items;
        for(cppJSON item:a){
            if(item.IsNull()||((item.IsArray()||item.IsObject())&&!item.size()))continue;
            if(format=="completions"&&k==1){
                if(!completions_allow.count(item.a->string))continue;
            }
            if(format=="responses"&&k==1){
                if(!responses_allow.count(item.a->string))continue;
            }
            if(format=="claude"&&k==1){
                if(!claude_allow.count(item.a->string))continue;
                if(item.IsArray()&&!item.size())continue;
                if(item.IsNull())continue;
            }
            if(format=="claude" && k==-1) {
                if(strcmp(item.a->string,"cache_control")==0)continue;
                if(strcmp(item.a->string,"caller")==0)continue;
                if(a["type"]=="tool_use" &&!a["id"].valuestring().empty() &&strcmp(item.a->string,"input")==0)continue;
            }
            if((k==1||k==-1)&&format=="claude"){
                if(strcmp(item.a->string,"content")==0&&item.IsString()){
                    cppJSON tmp("[{}]");
                    tmp[0].insert("text",item.valuestring());
                    tmp[0].insert("type","text");
                    items[item.a->string]=std::move(tmp);
                    continue;
                }
            }
            cppJSON tmp=my_format(item,format,k-1);
            if(tmp.IsNull()||((tmp.IsArray()||tmp.IsObject())&&!tmp.size())||(tmp.IsString()&&tmp.valuestring()==""))continue;
            items[item.a->string]=std::move(tmp);
        }
        cppJSON result("{}");
        for(auto& item:items)result.insert(item.first.c_str(),std::move(item.second));
        return result;
    }
    return a.clone();
}
