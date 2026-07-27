#include "gptapi6.h"
#include <curl/curl.h>
#include <algorithm>
#include <cctype>
#include <cstdio>
#include <ctime>
#include <fcntl.h>
#include <set>
#include <map>
#include <pthread.h>
#include <string>
#include <sys/stat.h>
#include <vector>
#include <unistd.h>
using namespace std;
__attribute((constructor)) static void gptapi6_init() {
    curl_global_init(CURL_GLOBAL_DEFAULT);
}

struct gpt6_proxy_data {
    http_para* client;
    string status_line;
    vector<string> headers;
    string body;
    bool headers_sent;
    bool write_failed;
    gpt6_proxy_data():client(0),headers_sent(false),write_failed(false){}
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

static void gpt6_log_exchange(const string& request,const string& response,const string& format,
                              CURLcode result,const string& status_line,bool parsed) {
    time_t now=time(0);
    struct tm local_time;
    if(!localtime_r(&now,&local_time))return;
    char filename[80];
    if(!strftime(filename,sizeof(filename),"/web/log/error/%Y-%m-%d_%H-%M-%S.txt",&local_time))return;
    string content="=== meta ===\nformat="+format+
        "\ncurl_code="+to_string((int)result)+
        "\ncurl_error="+curl_easy_strerror(result)+
        "\nparsed="+(parsed?"true":"false")+
        "\nstatus_line="+status_line+
        "\n=== request ===\n"+request+
        "\n\n=== response ===\n"+response+"\n\n";
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

static string gpt6_header_name(const string& line) {
    size_t colon=line.find(':');
    if(colon==string::npos)return string();
    string name=line.substr(0,colon);
    for(char& c:name)c=(char)tolower((unsigned char)c);
    return name;
}

static bool gpt6_hop_by_hop(const string& name) {
    return name=="connection"||name=="keep-alive"||name=="proxy-authenticate"||
           name=="proxy-authorization"||name=="te"||name=="trailer"||
           name=="transfer-encoding"||name=="upgrade"||name=="content-length";
}

static void gpt6_send_headers(gpt6_proxy_data* data) {
    if(data->headers_sent||!data->client)return;
    string head="HTTP/1.1 200 OK\r\n";
    size_t space=data->status_line.find(' ');
    if(space!=string::npos)head="HTTP/1.1"+data->status_line.substr(space);
    for(const string& line:data->headers) {
        string name=gpt6_header_name(line);
        if(!name.empty()&&!gpt6_hop_by_hop(name))head+=line;
    }
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
    } else if(line=="\r\n"||line=="\n") {
        int status=0;
        if(sscanf(data->status_line.c_str(),"HTTP/%*s %d",&status)!=1)status=200;
        if(status>=100&&status<200) {
            data->status_line.clear();
            data->headers.clear();
        }
    } else data->headers.push_back(line);
    return total;
}

static size_t gpt6_body_callback(void* ptr,size_t size,size_t count,void* userdata) {
    gpt6_proxy_data* data=(gpt6_proxy_data*)userdata;
    size_t total=size*count;
    data->body.append((char*)ptr,total);
    if(!data->headers_sent)gpt6_send_headers(data);
    if(data->client&&!data->write_failed&&!gpt6_write_all(data->client->cl,(char*)ptr,total))
        data->write_failed=true;
    return total;
}

static cppJSON gpt6_responses_history_item(const cppJSON& source) {
    cppJSON item=source.clone();
    item.erase("id");
    if(item["type"]=="message") {
        item.erase("status");
        cppJSON content=item["content"];
        if(content.IsArray())for(cppJSON part:content)if(part.IsObject()) {
            part.erase("annotations");
            part.erase("logprobs");
        }
    }
    return item;
}

static bool gpt6_extract_responses(const string& body,cppJSON& response) {
    cppJSON normal(body.c_str(),(int)body.size());
    if(normal.IsObject()&&normal["output"].IsArray()&&normal["output"].size()>0&&
       !normal["id"].valuestring().empty()) {
        response=normal.clone();
        return true;
    }
    cppJSON completed;
    map<int,cppJSON> done_items;
    map<int,string> output_text;
    size_t pos=0;
    while(pos<body.size()) {
        size_t end=body.find('\n',pos);
        string line=end==string::npos?body.substr(pos):body.substr(pos,end-pos);
        pos=end==string::npos?body.size():end+1;
        if(!line.empty()&&line.back()=='\r')line.pop_back();
        if(line.rfind("data:",0)!=0)continue;
        string payload=line.substr(5);
        while(!payload.empty()&&(payload[0]==' '||payload[0]=='\t'))payload.erase(0,1);
        cppJSON event(payload.c_str(),(int)payload.size());
        string type=event["type"].valuestring();
        if(type=="response.output_item.done"&&event["item"].IsObject()) {
            int index=event["output_index"].IsNumber()?(int)event["output_index"].valuedouble():(int)done_items.size();
            done_items[index]=gpt6_responses_history_item(event["item"]);
        } else if(type=="response.output_text.delta"&&event["delta"].IsString()) {
            int index=event["output_index"].IsNumber()?(int)event["output_index"].valuedouble():0;
            output_text[index]+=event["delta"].valuestring();
        } else if(type=="response.completed"&&event["response"].IsObject()&&
                  !event["response"]["id"].valuestring().empty()) {
            completed=event["response"].clone();
        }
    }
    if(!completed)return false;
    if(!done_items.empty()) {
        cppJSON output("[]");
        for(auto& entry:done_items)output.push_back(entry.second.clone());
        completed.insert("output",std::move(output));
        response=std::move(completed);
        return true;
    }
    if(completed["output"].IsArray()&&completed["output"].size()>0) {
        response=std::move(completed);
        return true;
    }
    cppJSON output("[]");
    for(auto& entry:output_text)if(!entry.second.empty()) {
        cppJSON part("{}");
        part.insert("type","output_text");
        part.insert("text",entry.second);
        cppJSON content("[]");
        content.push_back(std::move(part));
        cppJSON message("{}");
        message.insert("type","message");
        message.insert("role","assistant");
        message.insert("content",std::move(content));
        output.push_back(std::move(message));
    }
    if(output.size()==0)return false;
    completed.insert("output",std::move(output));
    response=std::move(completed);
    return true;
}

static bool gpt6_extract_completions(const string& body,cppJSON& response) {
    cppJSON normal(body.c_str(),(int)body.size());
    if(normal.IsObject()&&!normal["id"].valuestring().empty()&&normal["choices"].IsArray()&&
       normal["choices"].size()>0&&normal["choices"][0]["message"].IsObject()) {
        cppJSON output("[]");
        cppJSON message=normal["choices"][0]["message"].clone();
        message.erase("reasoning_content");
        output.push_back(std::move(message));
        response=cppJSON("{}");
        response.insert("id",normal["id"].valuestring());
        response.insert("output",std::move(output));
        return true;
    }

    string id,role="assistant",content;
    cppJSON tool_calls("[]");
    bool saw_choice=false;
    size_t pos=0;
    while(pos<body.size()) {
        size_t end=body.find('\n',pos);
        string line=end==string::npos?body.substr(pos):body.substr(pos,end-pos);
        pos=end==string::npos?body.size():end+1;
        if(!line.empty()&&line.back()=='\r')line.pop_back();
        if(line.rfind("data:",0)!=0)continue;
        string payload=line.substr(5);
        while(!payload.empty()&&(payload[0]==' '||payload[0]=='\t'))payload.erase(0,1);
        if(payload=="[DONE]")break;
        cppJSON event(payload.c_str(),(int)payload.size());
        if(!event.IsObject()||!event["choices"].IsArray()||event["choices"].size()<1)continue;
        if(id.empty())id=event["id"].valuestring();
        cppJSON delta=event["choices"][0]["delta"];
        if(!delta.IsObject())continue;
        saw_choice=true;
        string delta_role=delta["role"].valuestring();
        if(!delta_role.empty())role=delta_role;
        if(delta["content"].IsString())content+=delta["content"].valuestring();
        cppJSON delta_tool_calls=delta["tool_calls"];
        if(delta_tool_calls.IsArray()) {
            int fallback_index=0;
            for(cppJSON item=delta_tool_calls.child();item;item=item.next(),fallback_index++) {
                int index=item["index"].IsNumber()?(int)item["index"].valuedouble():fallback_index;
                if(index<0)continue;
                while(tool_calls.size()<=index)tool_calls.push_back(cppJSON("{}"));
                cppJSON target=tool_calls[index];
                string value=item["id"].valuestring();
                if(!value.empty()) {
                    if(target.has("id"))target["id"]=value;
                    else target.insert("id",value);
                }
                value=item["type"].valuestring();
                if(!value.empty()) {
                    if(target.has("type"))target["type"]=value;
                    else target.insert("type",value);
                }
                cppJSON function=item["function"];
                if(function.IsObject()) {
                    if(!target["function"].IsObject())target.insert("function",cppJSON("{}"));
                    cppJSON target_function=target["function"];
                    value=function["name"].valuestring();
                    if(!value.empty()) {
                        if(target_function.has("name"))target_function["name"]=value;
                        else target_function.insert("name",value);
                    }
                    if(function["arguments"].IsString()) {
                        value=target_function["arguments"].valuestring()+function["arguments"].valuestring();
                        if(target_function.has("arguments"))target_function["arguments"]=value;
                        else target_function.insert("arguments",value);
                    }
                }
            }
        }
    }
    if(id.empty()||!saw_choice)return false;
    cppJSON message("{}");
    message.insert("role",role);
    if(content.empty()&&tool_calls.size()>0)message.insert("content",(const char*)nullptr);
    else message.insert("content",content);
    if(tool_calls.size()>0)message.insert("tool_calls",std::move(tool_calls));
    cppJSON output("[]");
    output.push_back(std::move(message));
    response=cppJSON("{}");
    response.insert("id",id);
    response.insert("output",std::move(output));
    return true;
}

static bool gpt6_extract_response(const string& body,const string& format,cppJSON& response) {
    if(format=="responses")return gpt6_extract_responses(body,response);
    if(format=="completions")return gpt6_extract_completions(body,response);
    return false;
}

static cppJSON claude_history(const string& body,string& response_id);
static cppJSON gemini_history(const string& body,string& response_id);

string gpt6_request_model(http_para* a,const cppJSON& request,const string& format) {
    if(format!="gemini")return request["model"].valuestring();
    if(!a||!a->get)return string();
    string request_line(a->get,(size_t)a->n);
    size_t begin=request_line.find("/models/");
    size_t end=begin==string::npos?string::npos:request_line.find(":streamGenerateContent",begin);
    if(end==string::npos)return string();
    string model=request_line.substr(begin+8,end-begin-8);
    for(char c:model)if(!isalnum((unsigned char)c)&&c!='-'&&c!='_'&&c!='.')return string();
    return model;
}

bool gpt6_is_assistant(const cppJSON& item,const string& format) {
    return item["role"]==(format=="gemini"?"model":"assistant");
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

cppJSON gpt6_work(http_para* a,const string& url,const string& Authorization,
                       const string& message,const string& format,string& response_id) {
    response_id.clear();
    gpt6_proxy_data proxy;
    proxy.client=a;
    CURL* curl=curl_easy_init();
    CURLcode result=CURLE_FAILED_INIT;
    if(curl) {
        struct curl_slist* headers=0;
        headers=curl_slist_append(headers,"Content-Type: application/json");
        string auth;
        if(format=="claude"){
            auth="x-api-key: "+Authorization;
            headers=curl_slist_append(headers,auth.c_str());
            headers=curl_slist_append(headers,"anthropic-version: 2023-06-01");
        }else if(format=="gemini"){
            auth="x-goog-api-key: "+Authorization;
            headers=curl_slist_append(headers,auth.c_str());
        }else{
            auth="Authorization: "+Authorization;
            headers=curl_slist_append(headers,auth.c_str());
        }
        string request_url=gpt6_request_url(url,a,message,format);
        curl_easy_setopt(curl,CURLOPT_URL,request_url.c_str());
        curl_easy_setopt(curl,CURLOPT_HTTPHEADER,headers);
        curl_easy_setopt(curl,CURLOPT_POSTFIELDS,message.data());
        curl_easy_setopt(curl,CURLOPT_POSTFIELDSIZE_LARGE,(curl_off_t)message.size());
        curl_easy_setopt(curl,CURLOPT_HEADERFUNCTION,gpt6_header_callback);
        curl_easy_setopt(curl,CURLOPT_HEADERDATA,&proxy);
        curl_easy_setopt(curl,CURLOPT_WRITEFUNCTION,gpt6_body_callback);
        curl_easy_setopt(curl,CURLOPT_WRITEDATA,&proxy);
        curl_easy_setopt(curl,CURLOPT_CONNECTTIMEOUT,60L);
        curl_easy_setopt(curl,CURLOPT_TIMEOUT,1200L);
        curl_easy_setopt(curl,CURLOPT_NOSIGNAL,1L);
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
    cppJSON response,item;
    bool native=format=="claude"||format=="gemini";
    if(native)item=format=="claude"?claude_history(proxy.body,response_id)
                                    :gemini_history(proxy.body,response_id);
    bool parsed=native?item.IsObject():gpt6_extract_response(proxy.body,format,response);
    gpt6_log_exchange(message,proxy.body,format,result,proxy.status_line,parsed);
    if(result!=CURLE_OK||!parsed)return cppJSON("[]");
    if(native){
        cppJSON output("[]");
        output.push_back(std::move(item));
        return output;
    }
    response_id=response["id"].valuestring();
    cppJSON output=response["output"].clone();
    return output.IsArray()?std::move(output):cppJSON("[]");
}

static vector<cppJSON> native_sse_payloads(const string& body) {
    vector<cppJSON> result;
    size_t pos=0;
    while(pos<body.size()) {
        size_t end=body.find('\n',pos);
        string line=end==string::npos?body.substr(pos):body.substr(pos,end-pos);
        pos=end==string::npos?body.size():end+1;
        if(!line.empty()&&line.back()=='\r')line.pop_back();
        if(line.rfind("data:",0)!=0)continue;
        string payload=line.substr(5);
        while(!payload.empty()&&(payload.front()==' '||payload.front()=='\t'))payload.erase(0,1);
        if(payload.empty()||payload=="[DONE]")continue;
        cppJSON event(payload.c_str(),(int)payload.size());
        if(event.IsObject())result.push_back(std::move(event));
    }
    return result;
}

static void native_append_string(cppJSON& object,const char* key,const string& delta) {
    if(!object.IsObject()||delta.empty())return;
    object.insert(key,object[key].valuestring()+delta);
}

static cppJSON claude_history(const string& body,string& response_id) {
    response_id.clear();
    cppJSON normal(body.c_str(),(int)body.size()),message;
    if(normal.IsObject()&&normal["type"]=="message")message=normal.clone();
    map<int,cppJSON> blocks;
    map<int,string> partial_json;
    if(!message)for(cppJSON event:native_sse_payloads(body)) {
        string type=event["type"].valuestring();
        if(type=="message_start"&&event["message"].IsObject())message=event["message"].clone();
        else if(type=="content_block_start"&&event["content_block"].IsObject())
            blocks[(int)event["index"].valuedouble()]=event["content_block"].clone();
        else if(type=="content_block_delta"&&event["delta"].IsObject()) {
            int index=(int)event["index"].valuedouble();
            if(!blocks[index].IsObject())blocks[index]=cppJSON("{}");
            cppJSON delta=event["delta"];
            string delta_type=delta["type"].valuestring();
            if(delta_type=="text_delta")native_append_string(blocks[index],"text",delta["text"].valuestring());
            else if(delta_type=="thinking_delta")native_append_string(blocks[index],"thinking",delta["thinking"].valuestring());
            else if(delta_type=="signature_delta")native_append_string(blocks[index],"signature",delta["signature"].valuestring());
            else if(delta_type=="input_json_delta")partial_json[index]+=delta["partial_json"].valuestring();
        } else if(type=="message_delta"&&event["delta"].IsObject()) {
            if(!message.IsObject())message=cppJSON("{}");
            for(cppJSON field:event["delta"])message.insert(field.a->string,field.clone());
            if(event["usage"].IsObject()) {
                if(!message["usage"].IsObject())message.insert("usage",cppJSON("{}"));
                for(cppJSON field:event["usage"])
                    message["usage"].insert(field.a->string,field.clone());
            }
        }
    }
    if(!message.IsObject())return cppJSON();
    response_id=message["id"].valuestring();
    if(!blocks.empty()) {
        cppJSON content("[]");
        for(auto& entry:blocks) {
            if(!partial_json[entry.first].empty()) {
                cppJSON input(partial_json[entry.first].c_str());
                if(input)entry.second.insert("input",std::move(input));
            }
            content.push_back(entry.second.clone());
        }
        message.insert("content",std::move(content));
    }
    cppJSON history("{}");
    history.insert("role",message["role"].valuestring().empty()?"assistant":message["role"].valuestring());
    history.insert("content",message["content"].IsArray()||message["content"].IsString()
                             ?message["content"].clone():cppJSON("[]"));
    return history;
}

static bool same_gemini_part(const cppJSON& left,const cppJSON& right) {
    if(!left.IsObject()||!right.IsObject())return false;
    if(left["text"].IsString()&&right["text"].IsString())
        return (left["thought"]==true)==(right["thought"]==true);
    return (left.has("functionCall")&&right.has("functionCall"))||
           (left.has("thoughtSignature")&&right.has("thoughtSignature"));
}

static cppJSON gemini_history(const string& body,string& response_id) {
    response_id.clear();
    cppJSON normal(body.c_str(),(int)body.size());
    vector<cppJSON> payloads,parts;
    if(normal.IsObject())payloads.push_back(normal.clone());
    else payloads=native_sse_payloads(body);
    cppJSON content("{}");
    for(cppJSON response:payloads) {
        string id=response["responseId"].valuestring();
        if(!id.empty())response_id=id;
        cppJSON chunk=response["candidates"][0]["content"];
        if(!chunk.IsObject())continue;
        if(!chunk["role"].valuestring().empty())content.insert("role",chunk["role"].valuestring());
        if(!chunk["parts"].IsArray())continue;
        for(cppJSON part:chunk["parts"]) {
            if(!parts.empty()&&same_gemini_part(parts.back(),part)) {
                if(part["text"].IsString())native_append_string(parts.back(),"text",part["text"].valuestring());
                else parts.back()=part.clone();
            } else parts.push_back(part.clone());
        }
    }
    if(parts.empty())return cppJSON();
    cppJSON output_parts("[]");
    for(cppJSON& part:parts)output_parts.push_back(part.clone());
    if(content["role"].valuestring().empty())content.insert("role","model");
    content.insert("parts",std::move(output_parts));
    return content;
}

set<string>responses_allow={"type","call_id","output","name","input","role","tools","content","encrypted_content"};
cppJSON my_format(const cppJSON& a,const string& format,int k){
    if(a.IsArray()){
        cppJSON result("[]");
        for(cppJSON item:a)result.push_back(my_format(item,format,k-1));
        return result;
    }
    if(a.IsObject()){
        std::vector<cppJSON> items;
        for(cppJSON item:a){
            const char* key=item.a&&item.a->string?item.a->string:"";
            if(format=="responses"&&k==1){
                if(responses_allow.find(key)==responses_allow.end())continue;
                if(item.IsArray()&&item.size()==0)continue;
            }
            items.push_back(item);
        }
        std::stable_sort(items.begin(),items.end(),[](const cppJSON& x,const cppJSON& y){
            const char* x_key=x.a&&x.a->string?x.a->string:"";
            const char* y_key=y.a&&y.a->string?y.a->string:"";
            return std::string(x_key)<std::string(y_key);
        });
        cppJSON result("{}");
        for(cppJSON item:items)result.insert(item.a->string,my_format(item,format,k-1));
        return result;
    }
    return a.clone();
}
