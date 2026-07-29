#include "gptapi6.h"
#include <bits/stdc++.h>
#include <curl/curl.h>
#include <fcntl.h>
#include <pthread.h>
#include <sys/stat.h>
#include <unistd.h>
using namespace std;
static cppJSON events(const string& body) {
    cppJSON out("[]");
    istringstream input(body);
    string line;
    while(getline(input,line)) {
        if(!line.empty()&&line.back()=='\r')line.pop_back();
        if(line.rfind("data:",0))continue;
        line.erase(0,5);
        line.erase(0,line.find_first_not_of(" \t"));
        if(line=="[DONE]")break;
        cppJSON event(line.c_str(),line.size());
        if(event.IsObject())out.push_back(move(event));
    }
    return out;
}
static unsigned long long usage_tokens(const cppJSON& value) {
    cppJSON usage=value["usage"];
    if(!usage.IsObject())usage=value["usageMetadata"];
    if(!usage.IsObject())return 0;
    if(usage["total_tokens"].IsNumber())return (unsigned long long)usage["total_tokens"].valuedouble();
    if(usage["totalTokenCount"].IsNumber())return (unsigned long long)usage["totalTokenCount"].valuedouble();
    unsigned long long input=usage["input_tokens"].IsNumber()?(unsigned long long)usage["input_tokens"].valuedouble():0;
    unsigned long long output=usage["output_tokens"].IsNumber()?(unsigned long long)usage["output_tokens"].valuedouble():0;
    return input+output;
}
static unsigned long long response_usage_tokens(const string& body,const cppJSON& response) {
    unsigned long long total=usage_tokens(response);
    if(total||response.IsObject())return total;
    cppJSON stream=events(body);
    unsigned long long largest=0,input=0,output=0;
    for(cppJSON event:stream) {
        unsigned long long value=usage_tokens(event);
        if(!value)value=usage_tokens(event["response"]);
        if(value>largest)largest=value;
        cppJSON usage=event["usage"];
        if(usage["input_tokens"].IsNumber())input=(unsigned long long)usage["input_tokens"].valuedouble();
        if(usage["output_tokens"].IsNumber())output=(unsigned long long)usage["output_tokens"].valuedouble();
    }
    return input||output?input+output:largest;
}
static void append(cppJSON& object,const char* key,const string& text) {
    if(object.IsObject()&&!text.empty())object.insert(key,object[key].valuestring()+text);
}
static cppJSON wrap(cppJSON item) {
    cppJSON out("[]");
    if(item)out.push_back(move(item));
    return out;
}
static cppJSON responses(const cppJSON& input,string& id) {
    cppJSON completed;
    map<int,cppJSON> items;
    map<int,string> texts;
    for(cppJSON event:input) {
        string type=event["type"].valuestring();
        if(type=="response.output_item.done"&&event["item"].IsObject()) {
            int index=event["output_index"].IsNumber()?event["output_index"].valuedouble():items.size();
            cppJSON item=event["item"].clone();
            item.erase("id");
            if(item["type"]=="message") {
                item.erase("status");
                for(cppJSON part:item["content"]) {
                    part.erase("annotations");
                    part.erase("logprobs");
                }
            }
            items[index]=move(item);
        }
        if(type=="response.output_text.delta"&&event["delta"].IsString()) {
            int index=event["output_index"].IsNumber()?event["output_index"].valuedouble():0;
            texts[index]+=event["delta"].valuestring();
        }
        if(type=="response.completed"&&event["response"].IsObject()&&!event["response"]["id"].valuestring().empty())completed=event["response"].clone();
    }
    if(!completed)return cppJSON("[]");
    id=completed["id"].valuestring();
    if(items.empty()&&completed["output"].IsArray()&&completed["output"].size())return completed["output"].clone();
    cppJSON out("[]");
    for(auto& item:items)out.push_back(item.second.clone());
    if(!items.empty())return out;
    for(auto& text:texts) {
        if(text.second.empty())continue;
        cppJSON message("{\"type\":\"message\",\"role\":\"assistant\",\"content\":[{\"type\":\"output_text\"}]}");
        message["content"][0].insert("text",text.second);
        out.push_back(move(message));
    }
    return out;
}
static cppJSON completions(const cppJSON& input,string& id) {
    string role="assistant",content;
    cppJSON calls("[]");
    for(cppJSON event:input) {
        if(!event["choices"].IsArray()||!event["choices"].size())continue;
        cppJSON delta=event["choices"][0]["delta"];
        if(!delta.IsObject())continue;
        if(id.empty())id=event["id"].valuestring();
        if(!delta["role"].valuestring().empty())role=delta["role"].valuestring();
        if(delta["content"].IsString())content+=delta["content"].valuestring();
        for(cppJSON item:delta["tool_calls"]) {
            int index=item["index"].valuedouble();
            while(calls.size()<=index)calls.push_back(cppJSON("{}"));
            cppJSON call=calls[index];
            if(!item["id"].valuestring().empty())call.insert("id",item["id"].valuestring());
            if(!item["type"].valuestring().empty())call.insert("type",item["type"].valuestring());
            cppJSON function=item["function"];
            if(!function.IsObject())continue;
            if(!call["function"].IsObject())call.insert("function",cppJSON("{}"));
            if(!function["name"].valuestring().empty())call["function"].insert("name",function["name"].valuestring());
            cppJSON target=call["function"];
            if(function["arguments"].IsString())append(target,"arguments",function["arguments"].valuestring());
        }
    }
    if(id.empty())return cppJSON("[]");
    cppJSON message("{}");
    message.insert("role",role);
    if(content.empty()&&calls.size())message.insert("content",(const char*)0);
    else message.insert("content",content);
    if(calls.size())message.insert("tool_calls",move(calls));
    return wrap(move(message));
}
static cppJSON claude(const cppJSON& input,string& id) {
    cppJSON message;
    map<int,cppJSON> blocks;
    map<int,string> json;
    for(cppJSON event:input) {
        string type=event["type"].valuestring();
        int index=event["index"].valuedouble();
        if(type=="message_start"&&event["message"].IsObject())message=event["message"].clone();
        if(type=="content_block_start"&&event["content_block"].IsObject())blocks[index]=event["content_block"].clone();
        if(type=="content_block_delta"&&event["delta"].IsObject()) {
            cppJSON delta=event["delta"];
            const char* key=delta["type"]=="text_delta"?"text":delta["type"]=="thinking_delta"?"thinking":delta["type"]=="signature_delta"?"signature":0;
            if(key)append(blocks[index],key,delta[key].valuestring());
            else json[index]+=delta["partial_json"].valuestring();
        }
    }
    if(!message.IsObject())return cppJSON("[]");
    id=message["id"].valuestring();
    if(!blocks.empty()) {
        cppJSON content("[]");
        for(auto& block:blocks) {
            cppJSON value(json[block.first].c_str());
            if(value)block.second.insert("input",move(value));
            content.push_back(block.second.clone());
        }
        message.insert("content",move(content));
    }
    cppJSON history("{}");
    string role=message["role"].valuestring();
    history.insert("role",role.empty()?"assistant":role);
    history.insert("content",message["content"].IsArray()||message["content"].IsString()?message["content"].clone():cppJSON("[]"));
    return wrap(move(history));
}
static bool same(const cppJSON& a,const cppJSON& b) {
    bool text=a["text"].IsString()&&b["text"].IsString();
    return a.IsObject()&&b.IsObject()&&(text?(a["thought"]==true)==(b["thought"]==true):(a.has("functionCall")&&b.has("functionCall"))||(a.has("thoughtSignature")&&b.has("thoughtSignature")));
}
static cppJSON gemini(const cppJSON& input,string& id) {
    cppJSON parts("[]"),content("{}");
    for(cppJSON event:input) {
        if(!event["responseId"].valuestring().empty())id=event["responseId"].valuestring();
        cppJSON chunk=event["candidates"][0]["content"];
        if(!chunk.IsObject())continue;
        if(!chunk["role"].valuestring().empty())content.insert("role",chunk["role"].valuestring());
        for(cppJSON part:chunk["parts"]) {
            cppJSON last=parts[parts.size()-1];
            if(!last||!same(last,part))parts.push_back(part.clone());
            else if(part["text"].IsString())append(last,"text",part["text"].valuestring());
            else last.replace(part);
        }
    }
    if(!parts.size())return cppJSON("[]");
    if(content["role"].valuestring().empty())content.insert("role","model");
    content.insert("parts",move(parts));
    return wrap(move(content));
}
cppJSON gpt7_sse_to_history(const string& body,const string& format,string& id) {
    id.clear();
    cppJSON input=events(body);
    cppJSON out("[]");
    if(format=="responses")out=responses(input,id);
    if(format=="completions")out=completions(input,id);
    if(format=="claude")out=claude(input,id);
    if(format=="gemini")out=gemini(input,id);
    if(!out.size())id.clear();
    return out;
}
__attribute((constructor)) static void gptapi6_init() {
    curl_global_init(CURL_GLOBAL_DEFAULT);
}
struct gpt6_proxy_data {
    http_para* client=0;
    string status_line,body;
    vector<string> headers;
    bool headers_sent=false,write_failed=false;
};
static bool gpt6_write_all(int fd,const char* data,size_t size) {
    for(size_t sent=0;sent<size;) {
        ssize_t n=write(fd,data+sent,size-sent);
        if(n<=0)return false;
        sent+=(size_t)n;
    }
    return true;
}
static pthread_mutex_t gpt6_error_log_mutex=PTHREAD_MUTEX_INITIALIZER;
static void gpt6_log_exchange(const string& request,const string& response,const string& format,CURLcode result,const string& status_line,bool parsed) {
    time_t now=time(0);
    struct tm local_time;
    if(!localtime_r(&now,&local_time))return;
    char filename[80];
    if(!strftime(filename,sizeof(filename),"/web/log/error/%Y-%m-%d_%H-%M-%S.txt",&local_time))return;
    string content="=== meta ===\nformat="+format+"\ncurl_code="+to_string((int)result)+"\ncurl_error="+curl_easy_strerror(result)+"\nparsed="+(parsed?"true":"false")+"\nstatus_line="+status_line+"\n=== request ===\n"+request+"\n\n=== response ===\n"+response+"\n\n";
    pthread_mutex_lock(&gpt6_error_log_mutex);
    mkdir("/web/log",0700);
    mkdir("/web/log/error",0700);
    int fd=open(filename,O_WRONLY|O_CREAT|O_APPEND,0600);
    if(fd>=0) {
        gpt6_write_all(fd,content.data(),content.size());
        close(fd);
    }
    pthread_mutex_unlock(&gpt6_error_log_mutex);
}
static bool gpt6_forward_header(string name) {
    size_t colon=name.find(':');
    if(colon==string::npos)return false;
    name.resize(colon);
    for(char& c:name)c=(char)tolower((unsigned char)c);
    static const set<string> blocked={"connection","keep-alive","proxy-authenticate","proxy-authorization","te","trailer","transfer-encoding","upgrade","content-length"};
    return !blocked.count(name);
}
static void gpt6_send_headers(gpt6_proxy_data* data) {
    if(data->headers_sent||!data->client)return;
    string head="HTTP/1.1 200 OK\r\n";
    size_t space=data->status_line.find(' ');
    if(space!=string::npos)head="HTTP/1.1"+data->status_line.substr(space);
    for(const string& line:data->headers)if(gpt6_forward_header(line))head+=line;
    head+="Connection: close\r\n\r\n";
    if(!gpt6_write_all(data->client->cl,head.data(),head.size()))data->write_failed=true;
    data->headers_sent=true;
}
static size_t gpt6_header_callback(char* ptr,size_t size,size_t count,void* userdata) {
    gpt6_proxy_data* data=(gpt6_proxy_data*)userdata;
    size_t total=size*count;
    string line(ptr,total);
    if(line.rfind("HTTP/",0)==0) {
        data->status_line=line;
        data->headers.clear();
    }
    else if(line=="\r\n"||line=="\n") {
        int status=0;
        if(sscanf(data->status_line.c_str(),"HTTP/%*s %d",&status)!=1)status=200;
        if(status>=100&&status<200) {
            data->status_line.clear();
            data->headers.clear();
        }
    }
    else data->headers.push_back(line);
    return total;
}
static size_t gpt6_body_callback(void* ptr,size_t size,size_t count,void* userdata) {
    gpt6_proxy_data* data=(gpt6_proxy_data*)userdata;
    size_t total=size*count;
    data->body.append((char*)ptr,total);
    if(!data->headers_sent)gpt6_send_headers(data);
    if(data->client&&!data->write_failed&&!gpt6_write_all(data->client->cl,(char*)ptr,total))data->write_failed=true;
    return total;
}
static void gpt6_options(CURL*) {
}
template<class T,class... A>static void gpt6_options(CURL* curl,CURLoption option,T value,A... rest) {
    curl_easy_setopt(curl,option,value);
    gpt6_options(curl,rest...);
}
static cppJSON gpt6_normal_to_history(const cppJSON& response,const string& format,string& response_id) {
    cppJSON output("[]"),history;
    if(format=="responses"&&response["output"].IsArray()&&response["output"].size()>0) {
        if(!(response_id=response["id"].valuestring()).empty())return response["output"].clone();
    }
    else if(format=="completions"&&response["choices"].IsArray()&&response["choices"].size()>0&&response["choices"][0]["message"].IsObject()) {
        if((response_id=response["id"].valuestring()).empty())return output;
        history=response["choices"][0]["message"].clone();
        history.erase("reasoning_content");
    }
    else if(format=="claude"&&response["type"]=="message") {
        response_id=response["id"].valuestring();
        history=cppJSON("{}");
        history.insert("role",response["role"].valuestring().empty()?"assistant":response["role"].valuestring());
        history.insert("content",response["content"].IsArray()||response["content"].IsString()?response["content"].clone():cppJSON("[]"));
    }
    else if(format=="gemini"&&response["candidates"][0]["content"].IsObject()) {
        response_id=response["responseId"].valuestring();
        history=response["candidates"][0]["content"].clone();
        if(history["role"].valuestring().empty())history.insert("role","model");
        if(!history["parts"].IsArray()||!history["parts"].size())history.clear();
    }
    if(history)output.push_back(std::move(history));
    return output;
}
static string gpt6_request_url(string url,http_para* a,const string& message,const string& format) {
    if(format!="gemini")return url;
    string model=gpt6_request_model(a,cppJSON(message.c_str()),format);
    size_t marker=url.find("{model}");
    if(marker!=string::npos)url.replace(marker,7,model);
    else if(url.find(":streamGenerateContent")==string::npos){
        if(!url.empty()&&url.back()!='/')url+='/';
        url+="models/"+model+":streamGenerateContent";
    }
    if(url.find("alt=sse")==string::npos)url+=(url.find('?')==string::npos?"?":"&")+string("alt=sse");
    return url;
}
cppJSON gpt6_work(http_para* a,const string& url,const string& Authorization,const string& message,const string& format,string& response_id,unsigned long long* used_tokens) {
    response_id.clear();
    gpt6_proxy_data proxy;
    proxy.client=a;
    CURL* curl=curl_easy_init();
    CURLcode result=CURLE_FAILED_INIT;
    if(curl) {
        struct curl_slist* headers=curl_slist_append(0,"Content-Type: application/json");
        string auth=(format=="claude"?"x-api-key: ":format=="gemini"?"x-goog-api-key: ":"Authorization: ")+Authorization;
        headers=curl_slist_append(headers,auth.c_str());
        if(format=="claude")headers=curl_slist_append(headers,"anthropic-version: 2023-06-01");
        string request_url=gpt6_request_url(url,a,message,format);
        gpt6_options(curl,CURLOPT_URL,request_url.c_str(),CURLOPT_HTTPHEADER,headers,CURLOPT_POSTFIELDS,message.data(),CURLOPT_POSTFIELDSIZE_LARGE,(curl_off_t)message.size(),CURLOPT_HEADERFUNCTION,gpt6_header_callback,CURLOPT_HEADERDATA,&proxy,CURLOPT_WRITEFUNCTION,gpt6_body_callback,CURLOPT_WRITEDATA,&proxy,CURLOPT_CONNECTTIMEOUT,60L,CURLOPT_TIMEOUT,1200L,CURLOPT_NOSIGNAL,1L);
        result=curl_easy_perform(curl);
        curl_slist_free_all(headers);
        curl_easy_cleanup(curl);
    }
    if(!proxy.headers_sent&&!proxy.status_line.empty())gpt6_send_headers(&proxy);
    if(!proxy.headers_sent&&a) {
        string body="{\"error\":\""+string(curl_easy_strerror(result))+"\"}";
        string head=H500 Hjson "Connection: close\r\nContent-Length: "+to_string(body.size())+"\r\n\r\n";
        gpt6_write_all(a->cl,head.data(),head.size());
        gpt6_write_all(a->cl,body.data(),body.size());
    }
    cppJSON response(proxy.body.c_str(),(int)proxy.body.size());
    if(used_tokens)*used_tokens=response_usage_tokens(proxy.body,response);
    cppJSON output=response.IsObject()?gpt6_normal_to_history(response,format,response_id):gpt7_sse_to_history(proxy.body,format,response_id);
    bool parsed=output.IsArray()&&output.size()>0;
    if(used_tokens&&!*used_tokens)gpt6_log_exchange(message,proxy.body,format,result,proxy.status_line,parsed);
    if(result!=CURLE_OK||!parsed)return cppJSON("[]");
    return output;
}
///////////
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
static const set<string>responses_allow={"type","call_id","output","name","input","role","tools","content","encrypted_content"};
cppJSON my_format(const cppJSON& a,const string& format,int k){
    if(a.IsArray()){
        cppJSON result("[]");
        for(cppJSON item:a)result.push_back(my_format(item,format,k-1));
        return result;
    }
    if(a.IsObject()){
        map<string,cppJSON> items;
        for(cppJSON item:a){
            if(format=="responses"&&k==1){
                if(!responses_allow.count(item.a->string))continue;
                if(item.IsArray()&&!item.size())continue;
                if(item.IsNull())continue;
            }
            items[item.a->string]=my_format(item,format,k-1);
        }
        cppJSON result("{}");
        for(auto& item:items)result.insert(item.first.c_str(),std::move(item.second));
        return result;
    }
    return a.clone();
}
