#include "gptapi5.h"
#include "cppJSON.h"
#include "gptapi6.h"
#include "mylib.h"
#include "ndb2.h"
#include "user.h"
#include "http.h"
#include <pthread.h>
#include <sys/stat.h>
#include <time.h>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <set>
#include <string>
using namespace std;
#define CONFIG "/web/res/pri/gpt3.json"
#define GPT5_TOKEN_C 0.5
#define ll long long
ndb2 content_db;//con_id -> content
ndb2 index_db;//sha256(response_id) -> con_id
ndb2 history_db;//user_id -> history
ndb2 provider_db;//userid_model -> preferred provider
struct content{
    bool publish;
    bool deleted;
    bool isusing;
    char ownerid[10];
    char ownername[24];
    ll createtime;
    ll updatetime;
    char name[64];
    char format[20];
    char con_id[32];
    char hash[44];
    char other[1024-44];
    char content[0];
};
struct history{
    int n;
    char user_id[10];
    char con_id[][32];
};
__attribute((constructor)) void gptapi5_init() {
    content_db=ndb2_init("/web/res/pri/gpt5content.ndb2");
    index_db=ndb2_init("/web/res/pri/gpt5sha256.ndb2");
    history_db=ndb2_init("/web/res/pri/gpt5userhistory.ndb2");
    provider_db=ndb2_init("/web/res/pri/gpt5provider.ndb2");
}
void gpt5_apikey(http_para* a) {
    user_* p=getuser(a->get);
    if(!p)return my_http_error(a,"Please log in first.");
    cppJSON request(a->get+a->n),config=cppJSON::from_file(CONFIG);
    if(!config)return my_http_error(a,"can not read gpt3.json.");
    bool rotate=request["rotate"]==true;
    string model=request["model"],provider=request["provider"];
    if(!model.empty()&&!provider.empty()) {
        if(!config["model_available_provider"][model].has(provider.c_str()))return my_http_error(a,"Invalid provider.");
        string key=(string)p->userid+"_"+model;
        char* saved=(char*)ndb2_got(provider_db,key.c_str(),provider.size()+1);
        if(saved)memcpy(saved,provider.c_str(),provider.size()+1);
    }
    if(!p->gptapikey[0]||rotate)mylib_random_string(p->gptapikey,19);
    if(!p->token_limit)p->token_limit=10000000ULL;
    cppJSON ans("{}");
    ans.insert("api_key",(string)"sk-"+p->userid+p->gptapikey);
    ans.insert("token_limit",(double)p->token_limit);
    ans.insert("token_used",(double)p->token_used);
    ans.insert("admin",p->admin!=0);
    cppJSON selected("{}");
    for(cppJSON item:config["model_available_provider"]) {
        string key=(string)p->userid+"_"+item.a->string;
        char* saved=(char*)ndb2_got(provider_db,key.c_str(),0);
        if(saved)selected.insert(item.a->string,saved);
    }
    ans.insert("selected_provider",std::move(selected));
    http_send(a,Hok Hjson Hc0,ans.stringify_Unformatted().c_str(),0);
}
void gpt5_resolve(http_para* a) {
    cppJSON re(a->get+a->n);
    string id=re["response_id"];
    if(id.empty())return my_http_error(a,"response_id is required.");
    id="response_id_"+id;
    char c[48];
    mylib_sha256(id.c_str(),id.size(),c);
    char* con_id=(char*)ndb2_got(index_db,c,0);
    if(!con_id)return my_http_error(a,"Response not found.");
    cppJSON response("{}");
    response.insert("con_id",con_id);
    http_send(a,Hok Hjson Hc0,response.stringify_Unformatted().c_str(),0);
}
void gpt5_history_list(http_para* a) {
    user_* p=getuser(a->get);
    if(!p)return my_http_error(a,"Please log in first.");
    history*h=(history*)ndb2_got(history_db,p->userid,0);
    if(!h)return http_send(a,Hok Hjson Hc0,"[]",0);
    cppJSON ans("[]");
    for(int i=0;i<h->n;i++){
        content* con=(content*)ndb2_got(content_db,h->con_id[i],0);
        if(!con||con->deleted)continue;
        cppJSON item("{}");
        item.insert("con_id",con->con_id);
        item.insert("name",con->name);
        item.insert("updatetime",(double)con->updatetime);
        ans.push_back(std::move(item));
    }
    return http_send(a,Hok Hjson Hc0,ans.stringify_Unformatted().c_str(),0);
}
void gpt5_history_get(http_para* a) {
    user_* p=getuser(a->get);
    string con_id=cppJSON(a->get+a->n)["con_id"];
    if(con_id.empty())return my_http_error(a,"con_id is required.");
    content* con=(content*)ndb2_got(content_db,con_id.c_str(),0);
    if(!con)return my_http_error(a,"conversation not found.");
    if(!con->publish&&(!p||strcmp(con->ownerid,p->userid)!=0))return my_http_error(a,"Permission denied.");
    cppJSON ans("{}");
    ans.insert("ownername",con->ownername);
    ans.insert("isusing",con->isusing);
    ans.insert("name",con->name);
    ans.insert("format",con->format);
    ans.insert("con_id",con->con_id);
    ans.insert("content",cppJSON(con->content));
    return http_send(a,Hok Hjson Hc0,ans.stringify_Unformatted().c_str(),0);
}
void gpt5_history_rename(http_para* a) {
    user_* p=getuser(a->get);
    if(!p)return my_http_error(a,"Please log in first.");
    cppJSON request(a->get+a->n);
    string con_id=request["con_id"],title=request["title"];
    if(con_id.empty())return my_http_error(a,"con_id is required.");
    if(title.empty())return my_http_error(a,"title is required.");
    if(title.size()>=sizeof(((content*)0)->name))return my_http_error(a,"title is too long.");
    content* con=(content*)ndb2_got(content_db,con_id.c_str(),0);
    if(!con)return my_http_error(a,"conversation not found.");
    if(strcmp(con->ownerid,p->userid)!=0)return my_http_error(a,"Permission denied.");
    memset(con->name,0,sizeof(con->name));
    memcpy(con->name,title.data(),title.size());
    http_send(a,Hok Hjson Hc0,"{\"ok\":true}",0);
}
void gpt5_history_delete(http_para* a) {
    user_* p=getuser(a->get);
    if(!p)return my_http_error(a,"Please log in first.");
    string con_id=cppJSON(a->get+a->n)["con_id"];
    if(con_id.empty())return my_http_error(a,"con_id is required.");
    content* con=(content*)ndb2_got(content_db,con_id.c_str(),0);
    if(!con)return my_http_error(a,"conversation not found.");
    if(strcmp(con->ownerid,p->userid)!=0)return my_http_error(a,"Permission denied.");
    con->deleted=true;
    history* h=(history*)ndb2_got(history_db,p->userid,0);
    if(h){
        int l=0;
        for(int i=0;i<h->n;i++){
            if(strncmp(h->con_id[i],con_id.c_str(),sizeof(h->con_id[i]))==0)continue;
            if(l!=i)memcpy(h->con_id[l],h->con_id[i],sizeof(h->con_id[l]));
            l++;
        }
        h->n=l;
    }
    http_send(a,Hok Hjson Hc0,"{\"ok\":true}",0);
}
void gpt5_share(http_para* a) {
    user_* p=getuser(a->get);
    if(!p)return my_http_error(a,"Please log in first.");
    cppJSON request(a->get+a->n);
    string con_id=request["con_id"];
    if(con_id.empty())return my_http_error(a,"con_id is required.");
    if(!(request["publish"]==true))return my_http_error(a,"publish must be true.");
    content* con=(content*)ndb2_got(content_db,con_id.c_str(),0);
    if(!con)return my_http_error(a,"conversation not found.");
    if(strcmp(con->ownerid,p->userid)!=0)return my_http_error(a,"Permission denied.");
    con->publish=true;
    http_send(a,Hok Hjson Hc0,"{\"ok\":true}",0);
}
static std::string utf8_substr(const std::string& str, unsigned int max_bytes) {
    if (str.length() <= max_bytes) return str;
    unsigned int len = max_bytes;
    while(len > 0 && (str[len] & 0xC0) == 0x80) len--;
    return str.substr(0, len) + "...";
}
#define LOCK(a) while(__sync_val_compare_and_swap(a,0,1))usleep(0)
void insert2index_db(const string&a,const string&b){
    char tmp[48]={0};
    mylib_sha256(a.c_str(),a.length(),tmp);
    char* p=(char*)ndb2_got(index_db,tmp,b.length()+10);
    if(p)memcpy(p,b.c_str(),b.length());
}
void maketitle(char*name,string b,const cppJSON& config){
    if(!name[0]){
        cppJSON title_request("{\"stream\":false}");
        cppJSON title_config=config["title"];
        title_request.insert("model",title_config["model"].valuestring());
        cppJSON title_messages("[]");
        cppJSON title_message("{\"role\":\"user\"}");
        string prompt="将以下回复的内容取一个简短的标题：\n"+b;
        title_message.insert("content",prompt.c_str());
        title_messages.push_back(std::move(title_message));
        title_request.insert("messages",std::move(title_messages));
        string _;
        cppJSON title_reply=gpt6_work(0,title_config["url"],title_config["Authorization"],title_request.stringify_Unformatted(),"completions",_);
        string title=title_reply[0]["content"];
        if(title.size()>4&&title.substr(0,2)=="**"&&title.substr(title.size()-2)=="**")title=title.substr(2,title.size()-4);
        if(title.size()>60)title=utf8_substr(title,56)+"...";
        if(!title.empty())strcpy(name,title.c_str());
    }
}
#define key_find(str) do{char*t=strcasestr(a->get,str);if(t)tmp=t+strlen(str);}while(0)
static void gpt5_completion_request(http_para* a,const string& format,const char* array_name) {
    char *tmp=0;
    key_find("Authorization: Bearer sk-");
    if(!tmp)key_find("Authorization: sk-");
    if(!tmp)key_find("x-api-key: sk-");
    user_* p=getuser_by_id(tmp);
    if(p&&tmp&&memcmp(tmp+8,p->gptapikey,19))p=0;
    if(!p)return my_http_error(a,"Invalid API key.");
    cppJSON request(a->get+a->n),config=cppJSON::from_file(CONFIG),conf;
    string model=gpt6_request_model(a,request,format);
    if(!config)return my_http_error(a,"can not read gpt3.json.");
    if(!config["model_available_provider"].has(model.c_str()))return my_http_error(a,"Model not found.");
    cppJSON pros=config["model_available_provider"][model];
    string key=(string)p->userid+"_"+model;
    char* saved=(char*)ndb2_got(provider_db,key.c_str(),0);
    if(pros.has(saved)&&(p->admin||config["provider"][saved]["public"]==true))conf=config["provider"][saved];
    else for(cppJSON pro:pros)if(p->admin||config["provider"][pro]["public"]==true){
                conf=config["provider"][pro];
                break;
            }
    if(!conf)return my_http_error(a,"Permission denied.");
    if(!p->token_limit)p->token_limit=10000000ULL;
    if(p->token_used>=p->token_limit)return my_http_error(a,"余额不足，访问 https://www.neuqboard.cn/token 获取更多信息");
    string auth=conf["Authorization"];
    if(auth.empty())return my_http_error(a,"gpt3.json error: No Authorization");
    cppJSON previous_new_input_format=my_format(request[array_name],format,2);
    string hash;
    char hash_[48],*con_id=0;
    content*con=0;
    bool isnew=false;
    time_t maxtime=time(0)+5;
    retry:
    for(int i=previous_new_input_format.size()-1,j=0;i&&(++j)<30;i--){
        hash=(string)"new_input_"+p->userid+previous_new_input_format.stringify_Unformatted();
        mylib_sha256(hash.c_str(),hash.length(),hash_);
        char* candidate_id=(char*)ndb2_got(index_db,hash_,0);
        content* candidate=(content*)ndb2_got(content_db,candidate_id,0);
        if(candidate&&!strcmp(candidate->format,format.c_str())&&!strcmp(candidate->hash,hash_)){
            con_id=candidate_id;
            con=candidate;
            break;
        }
        previous_new_input_format.erase(i);
    }
    if(!con||strcmp(con->format,format.c_str())||memcmp(con->ownerid,p->userid,8)!=0)isnew=true;
    else{
        int o1=__sync_val_compare_and_swap(&con->isusing,0,1);
        if(o1){
            if(time(0)<maxtime){
                previous_new_input_format=my_format(request[array_name],format,2);
                con=0;
                con_id=0;
                usleep(1000000);
                goto retry;
            } else isnew=true;
        } else if(strcmp(con->hash,hash_)){
            con->isusing=0;
            isnew=true;
        }
    }
    if(isnew){
        con_id=(char*)malloc(32);
        memset(con_id,0,32);
        mylib_random_string(con_id,20);
        con=(content*)ndb2_got(content_db,con_id,sizeof(content)+10);
        if(!con)return my_http_error(a,"content_db error");
        memset(con,0,sizeof(content)+3);
        memcpy(con->ownerid,p->userid,8);
        memcpy(con->ownername,p->name,min(strlen(p->name),sizeof(con->ownername)-1));
        con->createtime=con->updatetime=time(0);
        memcpy(con->format,format.data(),min(format.size(),sizeof(con->format)-1));
        memcpy(con->con_id,con_id,strlen(con_id));
        memcpy(con->content,"[]",3);
        con->isusing=true;
        history* h=(history*)ndb2_got(history_db,p->userid,0);
        int n=h?h->n+1:1;
        h=(history*)ndb2_got(history_db,p->userid,sizeof(history)+32*n);
        if(h){
            h->n=n;
            if(n==1)memcpy(h->user_id,p->userid,8);
            memcpy(h->con_id[n-1],con_id,32);
        }
    }
    string response_id;
    unsigned long long used_tokens=0;
    cppJSON output=gpt6_work(a,conf["url"],auth,(string)(a->get+a->n),format,response_id,&used_tokens);
    p->token_used+=(unsigned long long)ceil(used_tokens*config["model_multiply"][model][0].valuedouble()
        *GPT5_TOKEN_C*conf["multiply"].valuedouble()/0.3);
    if(!response_id.empty())insert2index_db("response_id_"+response_id,con_id);
    cppJSON input=request[array_name].clone();
    for(cppJSON i:output)input.push_back(i);
    string new_input=input.stringify_Unformatted();
    cppJSON new_input_format=my_format(input,format,2);
    string new_input_key=(string)"new_input_"+p->userid+new_input_format.stringify_Unformatted();
    char new_hash[44];
    mylib_sha256(new_input_key.c_str(),new_input_key.length(),new_hash);
    insert2index_db(new_input_key,con_id);
    con=(content*)ndb2_got(content_db,con_id,sizeof(content)+new_input.length()+10);
    if(con){
        memcpy(con->content,new_input.c_str(),new_input.length()+1);
        memcpy(con->hash,new_hash,44);
        con->isusing=0;
        con->updatetime=time(0);
        if(!con->name[0])maketitle(con->name,output.stringify_Unformatted(),config);
    }
    if(isnew)free(con_id);
}
void gpt5_responses(http_para* a) {
    gpt5_completion_request(a,"responses","input");
}
void gpt5_chat_completions(http_para* a) {
    gpt5_completion_request(a,"completions","messages");
}
void gpt5_claude_messages(http_para* a) {
    gpt5_completion_request(a,"claude","messages");
}
void gpt5_gemini_generate_content(http_para* a) {
    gpt5_completion_request(a,"gemini","contents");
}
void gpt5_gpts2(http_para* a) {
    cppJSON config=cppJSON::from_file("/web/res/pri/gpt2.json");
    if(!config)return http_send(a,Hok Hc0 Htxt,"can not read gpt2.json.",0);
    for(cppJSON p:config) {
        p.erase("url");
        p.erase("Authorization");
    }
    http_send(a,Hok Hjson Hc0,config.stringify_Unformatted().c_str(),0);
}
void gpt5_model_list(http_para* a) {
    cppJSON config=cppJSON::from_file(CONFIG);
    if(!config)return http_send(a,Hok Hc0 Htxt,"can not read gpt3.json.",0);
    config.erase("title");
    for(cppJSON provider:config["provider"]) {
        provider.erase("name");
        provider.erase("url");
        provider.erase("Authorization");
    }
    http_send(a,Hok Hjson Hc0,config.stringify_Unformatted().c_str(),0);
}
