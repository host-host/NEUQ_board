#include "gptapi6.h"
#include <bits/stdc++.h>
#include <curl/curl.h>
#include <fcntl.h>
#include <pthread.h>
#include <sys/stat.h>
#include <unistd.h>
using namespace std;
#define ll long long
static void append(cppJSON& object,const char* key,const string& text) {
    if(object.IsObject()&&!text.empty())object.insert(key,object[key].valuestring()+text);
}
static bool same(const cppJSON& a,const cppJSON& b) {
    if(!a.IsObject()||!b.IsObject())return 0;
    if(a.has("functionCall")||b.has("functionCall"))return 0;//每个 functionCall 都是独立调用，不能合并
    if(a["text"].IsString()&&b["text"].IsString())return (a["thought"]==true)==(b["thought"]==true);
    return a.has("thoughtSignature")&&b.has("thoughtSignature");
}
void gpt6_parse_responses(gpt6_ret* ans,string& tmp,bool issse){
    auto f=[](gpt6_ret* ans,cppJSON t){
        if(!t)return;
        ans->used_tokens=t["total_tokens"].valuedouble();
        ans->input=t["input_tokens"].valuedouble();//input包含cache
        ans->cache=t["input_tokens_details"]["cached_tokens"].valuedouble();
        ans->makecache=t["input_tokens_details"]["cache_write_tokens"].valuedouble();//makecache==0
        ans->output=t["output_tokens"].valuedouble();
    };
    if(issse&&tmp.find("data: ")==0){
        cppJSON a(&tmp[6]);
        string type=a["type"];
        if(type=="response.output_item.added")if(!ans->first)ans->first=time(0);
        if(type=="response.output_item.done")if(!ans->first)ans->first=time(0);
        if(type=="response.output_text.delta")if(!ans->first)ans->first=time(0);
        if(type=="response.output_text.done")if(!ans->first)ans->first=time(0);
        if(type=="response.output_item.done")ans->append.push_back(a["item"].clone());
        if(type=="response.completed"){
            ans->response_id=a["response"]["id"];
            f(ans,a["response"]["usage"]);
        }
    }
    if(!issse){
        cppJSON r(tmp.c_str());
        ans->append=r["output"].clone();
        ans->response_id=r["id"].valuestring();
        f(ans,r["usage"]);
    }
}
static inline ll max(ll a,ll b){
    return a<b?b:a;
}
void gpt6_parse_completions(gpt6_ret* ans,string& tmp,bool issse){
    auto f=[](gpt6_ret* ans,cppJSON usage){
        if(!usage.IsObject())return;
        ans->input=max(ans->input,usage["prompt_tokens"].valuedouble());
        ans->output=max(ans->output,usage["completion_tokens"].valuedouble());
        ans->cache=max(ans->cache,usage["prompt_tokens_details"]["cached_tokens"].valuedouble());
        ans->makecache=max(ans->makecache,usage["prompt_tokens_details"]["cache_write_tokens"].valuedouble());
        ans->used_tokens=max(ans->used_tokens,max(usage["total_tokens"].valuedouble(),ans->input+ans->output));
    };
    if(issse&&tmp.find("data: ")==0){
        cppJSON res(tmp.data()+6);
        if(!res.IsObject())return;
        f(ans,res["usage"]);
        if(ans->response_id.empty())ans->response_id=res["id"].valuestring();
        cppJSON delta=res["choices"][0]["delta"];
        if(!delta.IsObject())return;
        if(!ans->first&&(!delta["content"].valuestring().empty()||!delta["reasoning_content"].valuestring().empty()||!delta["reasoning"].valuestring().empty()))ans->first=time(0);
        if(!ans->append[0].IsObject())ans->append=cppJSON("[{\"role\":\"assistant\",\"content\":\"\"}]");
        string tmp=delta["content"];
        if(!tmp.empty())ans->append[0]["content"]=ans->append[0]["content"].valuestring()+tmp;
        cppJSON tool_calls=delta["tool_calls"];
        if(tool_calls.IsArray()){//此处未验证过
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
        return;
    }
    if(!issse){
        cppJSON res=cppJSON(tmp.c_str());
        f(ans,res["usage"]);
        cppJSON message=res["choices"][0]["message"].clone();
        message.erase("reasoning_content");//我不知道应不应该删
        if(message)ans->append.push_back(std::move(message));
        ans->response_id=res["id"].valuestring();
        return;
    }
}
void gpt6_parse_claude(gpt6_ret* ans,string& tmp,bool issse){
    auto f=[](gpt6_ret* ans,cppJSON usage){
        if(!usage.IsObject())return;
        ans->input=max(ans->input,usage["input_tokens"].valuedouble());
        ans->output=max(ans->output,usage["output_tokens"].valuedouble());
        ans->cache=max(ans->cache,usage["cache_read_input_tokens"].valuedouble());
        ans->makecache=max(ans->makecache,usage["cache_creation_input_tokens"].valuedouble());
        ans->used_tokens=ans->input+ans->output+ans->cache;
    };
    if(issse&&tmp.find("data: ")==0){
        cppJSON res(tmp.data()+6),append,tmp;
        f(ans,res["usage"]);
        f(ans,res["message"]["usage"]);
        if(ans->response_id.empty())ans->response_id=res["message"]["id"].valuestring();
        string type=res["type"];
        if(!ans->append.size())ans->append=cppJSON(R"([{"role":"assistant","content":[]}])");
        append=ans->append[0]["content"];
        if(type=="content_block_start")append.push_back(res["content_block"].clone());
        tmp=append[int(res["index"].valuedouble())];
        if(type=="content_block_delta"){
            if(!ans->first)ans->first=time(0);
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
        return;
    }
    if(!issse){//未验证
        cppJSON res(tmp.c_str(),(int)tmp.size());
        f(ans,res["usage"]);
        ans->response_id=res["id"].valuestring();
        if(!(res["type"]=="message"))return;
        cppJSON history("{}");
        string role=res["role"].valuestring();
        history.insert("role",role.empty()?"assistant":role);
        history.insert("content",res["content"].IsArray()||res["content"].IsString()?res["content"].clone():cppJSON("[]"));
        ans->append=cppJSON("[]");
        ans->append.push_back(std::move(history));
        return;
    }
}
void gpt6_parse_gemini(gpt6_ret* ans,string& tmp,bool issse){//未验证
    auto f=[](gpt6_ret* ans,cppJSON usage){
        if(!usage.IsObject())return;
        ans->input=max(ans->input,usage["promptTokenCount"].valuedouble());
        ans->output=max(ans->output,usage["candidatesTokenCount"].valuedouble()+usage["thoughtsTokenCount"].valuedouble());
        ans->cache=max(ans->cache,usage["cachedContentTokenCount"].valuedouble());
        ans->used_tokens=max(ans->used_tokens,usage["totalTokenCount"].valuedouble());
    };
    if(issse&&tmp.find("data: ")==0){
        cppJSON res(tmp.data()+6),parts,last;
        f(ans,res["usageMetadata"]);
        if(!res["responseId"].valuestring().empty())ans->response_id=res["responseId"].valuestring();
        cppJSON chunk=res["candidates"][0]["content"];
        if(!chunk["parts"].size())return;
        if(!ans->first)ans->first=time(0);
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
    }
    if(!issse){
        cppJSON res(tmp.c_str(),(int)tmp.size());
        f(ans,res["usageMetadata"]);
        ans->response_id=res["responseId"].valuestring();
        cppJSON chunk=res["candidates"][0]["content"];
        if(!chunk["parts"].size())return;
        cppJSON history=chunk.clone();
        if(history["role"].valuestring().empty())history.insert("role","model");
        ans->append=cppJSON("[]");
        ans->append.push_back(std::move(history));
        return;
    }
}
void gpt6_parse(gpt6_ret* ans,string tmp,bool issse){//处理 append used_tokens input output cache makecache first response_id
    while(!tmp.empty()&&tmp.back()=='\r')tmp.pop_back();
    if(strcmp(ans->format,"responses")==0)gpt6_parse_responses(ans,tmp,issse);
    if(strcmp(ans->format,"completions")==0)gpt6_parse_completions(ans,tmp,issse);
    if(strcmp(ans->format,"claude")==0)gpt6_parse_claude(ans,tmp,issse);
    if(strcmp(ans->format,"gemini")==0)gpt6_parse_gemini(ans,tmp,issse);
}
static size_t gpt6header(char* ptr,size_t size,size_t count,void* userdata) {
    gpt6_ret* p=(gpt6_ret*)userdata;
    size_t n=size*count;
    p->header+=string((char*)ptr,n);
    return n;
}
static size_t gpt6body(void* ptr,size_t size,size_t count,void* userdata) {
    gpt6_ret* ans=(gpt6_ret*)userdata;
    if(!ans->issse) {
        size_t pos=ans->header.rfind("HTTP/");
        int status=500;
        if(pos!=string::npos)sscanf(ans->header.c_str()+pos,"HTTP/%*s %d",&status);
        ans->issse=strcasestr(ans->header.c_str(),"text/event-stream")?2:1;
        string head=httpcode(status);
        head+=ans->issse==2?Hsse Hc0 Hclo "\r\n":Hjson Hc0 Hclo "\r\n";
        if(ans->a)http_send(ans->a,0,head.data(),head.size());
    }
    size_t n=size*count;
    if(!n)return 0;
    if(ans->a)http_send(ans->a,0,(char*)ptr,n);
    string tmp((char*)ptr,n);
    if(ans->issse==2)ans->bodydelta+=tmp;
    ans->body+=tmp;
    if(ans->issse==2&&tmp.find('\n')!=string::npos){
        size_t u=0;
        while((u=ans->bodydelta.find('\n'))!=string::npos){
            gpt6_parse(ans,ans->bodydelta.substr(0,u),true);
            ans->bodydelta=ans->bodydelta.substr(u+1);
        }
    }
    ans->last=time(0);
    return n;
}
gpt6_ret gpt6_work3(http_para* a,const char* message,const char* model,cppJSON conf,const char* format){
    if(conf["map"][model].IsString())model=conf["map"][model].a->valuestring;
    gpt6_ret ans{};
    ans.a=a;
    ans.format=format;
    ans.append=cppJSON("[]");
    ans.start=time(0);
    CURL* curl=curl_easy_init();
    if(!curl) {
        ans.curlcode=CURLE_FAILED_INIT;
        return ans;
    }
    string Authorization=conf["Authorization"],url=conf["url"],mm;
    struct curl_slist* headers=curl_slist_append(0,"Content-Type: application/json");
    if(strcmp(format,"responses")==0||strcmp(format,"completions")==0)Authorization="Authorization: Bearer "+Authorization;
    if(strcmp(format,"claude")==0)Authorization="x-api-key: "+Authorization;
    if(strcmp(format,"gemini")==0)Authorization="x-goog-api-key: "+Authorization;
    headers=curl_slist_append(headers,Authorization.c_str());
    if(strcmp(format,"claude")==0)headers=curl_slist_append(headers,"anthropic-version: 2023-06-01");
    if(!url.empty()&&url.back()=='/')url.pop_back();
    if(strcmp(format,"responses")==0)url+="/v1/responses";
    if(strcmp(format,"completions")==0)url+="/v1/chat/completions";
    if(strcmp(format,"claude")==0)url+="/v1/messages";
    if(strcmp(format,"gemini")==0)url=url+"/v1beta/models/"+model+":streamGenerateContent?alt=sse";
    curl_easy_setopt(curl,CURLOPT_URL,url.c_str());
    curl_easy_setopt(curl,CURLOPT_HTTPHEADER,headers);
    cppJSON u(message);
    for(auto i:conf["append"])u.insert(i.a->string,i);//执行消息内容追加
    if(strcmp(format,"gemini"))u["model"]=model;//执行名字map映射
    mm=u.stringify_Unformatted();
    curl_easy_setopt(curl,CURLOPT_POSTFIELDS,mm.c_str());
    curl_easy_setopt(curl,CURLOPT_HEADERFUNCTION,gpt6header);
    curl_easy_setopt(curl,CURLOPT_HEADERDATA,&ans);
    curl_easy_setopt(curl,CURLOPT_WRITEFUNCTION,gpt6body);
    curl_easy_setopt(curl,CURLOPT_WRITEDATA,&ans);
    curl_easy_setopt(curl,CURLOPT_CONNECTTIMEOUT,60L);
    curl_easy_setopt(curl,CURLOPT_TIMEOUT,3600L);
    curl_easy_setopt(curl,CURLOPT_NOSIGNAL,1L);
    ans.curlcode=curl_easy_perform(curl);
    ans.end=time(0);
    if(!ans.issse)gpt6body(0,0,0,&ans);
    curl_easy_getinfo(curl,CURLINFO_RESPONSE_CODE,&ans.httpcode);
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);
    if(ans.issse==1)gpt6_parse(&ans,ans.body,false);
    if(!ans.bodydelta.empty())gpt6_parse(&ans,ans.bodydelta,true);
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
static const set<string>responses_allow={"type","call_id","output","name","input","role","content","encrypted_content"};
static const set<string>claude_allow={"role","content"};
static const set<string>claude_allow2={"cache_control"};
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
                if(a["type"]=="tool_use" &&!a["id"].valuestring().empty() &&strcmp(item.a->string,"input")==0)continue;
            }
            if((k==1||k==-1)&&format=="claude"){
                if(strcmp(item.a->string,"content")==0&&item.IsString()){
                    cppJSON tmp("[{}]");
                    tmp[0].insert("text",item.valuestring());
                    tmp[0].insert("type","text");
                    items[item.a->string]=tmp;
                    continue;
                }
            }
            cppJSON tmp=my_format(item,format,k-1);
            if(tmp.IsNull()||((tmp.IsArray()||tmp.IsObject())&&!tmp.size()))continue;
            items[item.a->string]=tmp;
        }
        cppJSON result("{}");
        for(auto& item:items)result.insert(item.first.c_str(),std::move(item.second));
        return result;
    }
    return a.clone();
}
