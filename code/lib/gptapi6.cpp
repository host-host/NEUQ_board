#include "gptapi6.h"
#include <curl/curl.h>
#include <algorithm>
#include <cctype>
#include <cstdio>
#include <ctime>
#include <fcntl.h>
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
        string auth="Authorization: "+Authorization;
        headers=curl_slist_append(headers,auth.c_str());
        curl_easy_setopt(curl,CURLOPT_URL,url.c_str());
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
    cppJSON response;
    bool parsed=gpt6_extract_response(proxy.body,format,response);
    gpt6_log_exchange(message,proxy.body,format,result,proxy.status_line,parsed);
    if(result!=CURLE_OK||!parsed)return cppJSON();
    response_id=response["id"].valuestring();
    return response["output"].clone();
}
cppJSON my_format(const cppJSON& a){
    if(a.IsArray()){
        cppJSON result("[]");
        for(cppJSON item:a)result.push_back(my_format(item));
        return result;
    }
    if(a.IsObject()){
        std::vector<cppJSON> items;
        for(cppJSON item:a)items.push_back(item);
        std::stable_sort(items.begin(),items.end(),[](const cppJSON& x,const cppJSON& y){
            const char* x_key=x.a&&x.a->string?x.a->string:"";
            const char* y_key=y.a&&y.a->string?y.a->string:"";
            return std::string(x_key)<std::string(y_key);
        });
        cppJSON result("{}");
        for(cppJSON item:items)result.insert(item.a->string,my_format(item));
        return result;
    }
    return a.clone();
}
