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
#include <cctype>
#include <climits>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <set>
#include <string>
using namespace std;
#define CONFIG "/web/res/pri/gpt4.json"
#define GPT5_TOKEN_C 0.5
#define ll long long
ndb2 content_db;//con_id -> content
ndb2 index_db;//sha256(response_id) -> con_id
ndb2 history_db;//user_id -> history
ndb2 provider_db;//userid_model -> preferred provider
ndb2 stable_db;//model_provider -> [3*24*4][2]
ndb2 log_db;//userid -> reslogs
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
struct reslog{
    char model[48],provider[48];
    int used_tokens;
    double multiply;
    long long time;
    char other[256];//保留为未来增加功能
};
struct reslogs{
    int lock,n;
    reslog a[];
};
__attribute((constructor)) void gptapi5_init() {
    content_db=ndb2_init("/web/res/pri/gpt5content.ndb2");
    index_db=ndb2_init("/web/res/pri/gpt5sha256.ndb2");
    history_db=ndb2_init("/web/res/pri/gpt5userhistory.ndb2");
    provider_db=ndb2_init("/web/res/pri/gpt5provider.ndb2");
    stable_db=ndb2_init("/web/res/pri/gpt5stable.ndb2");
    log_db=ndb2_init("/web/res/pri/gpt5log.ndb2");
}
#define ERROR(H,message) http_send(a,H Hjson Hc0,"{\"error\":{\"message\":\"" message "\"}}",0)
#define RET_JSON(j) do{char* tmp=j.PrintUnformatted();if(tmp){http_send(a,Hok Hjson Hc0,tmp,0);cppJSON::free_str(tmp);}return;}while(0)
void gpt5_apikey(http_para* a) {
    user_* p=getuser(a->get);
    if(!p)return my_http_error(a,"Please log in first.");
    cppJSON request(a->get+a->n),config=cppJSON::from_file(CONFIG);
    if(!config)return my_http_error(a,"can not read gpt4.json.");
    bool rotate=request["rotate"]==true;
    string model=request["model"],provider=request["provider"];
    if(!model.empty()&&!provider.empty()) {
        if(!config["model"][model]["provider"].has(provider.c_str()))return my_http_error(a,"Invalid provider.");
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
    for(cppJSON item:config["model"]) {
        string key=(string)p->userid+"_"+item.a->string;
        char* saved=(char*)ndb2_got(provider_db,key.c_str(),0);
        if(saved)selected.insert(item.a->string,saved);
    }
    ans.insert("selected_provider",std::move(selected));
    RET_JSON(ans);
}
void gpt5_log_list(http_para* a) {
    user_* p=getuser(a->get);
    if(!p)return my_http_error(a,"Please log in first.");
    cppJSON req(a->get+a->n),ans("[]");
    int start=req["start"].valuedouble(),end=req["end"].valuedouble();
    if(start<1)return my_http_error(a,"Bad request");
    reslogs* logs=(reslogs*)ndb2_got(log_db,p->userid,0);
    int n=logs?logs->n:0;
    for(int i=n-start;i>=0&&i>=n-end;i--){
        cppJSON item("{}");
        item.insert("model",logs->a[i].model);
        item.insert("provider",logs->a[i].provider);
        item.insert("used_tokens",(double)logs->a[i].used_tokens);
        item.insert("multiply",logs->a[i].multiply);
        item.insert("time",(double)logs->a[i].time);
        ans.push_back(std::move(item));
    }
    RET_JSON(ans);
}
static void gpt5_log(user_* p,const string& model,const string& provider,unsigned long long used_tokens,double multiply) {
    reslogs* logs;
    retry:
    logs=(reslogs*)ndb2_got(log_db,p->userid,0);
    if(!logs)if(!(logs=(reslogs*)ndb2_got(log_db,p->userid,sizeof(reslogs)+sizeof(reslog))))return;//db error
    if(TRY(&logs->lock))goto retry;
    int n=++logs->n;
    logs=(reslogs*)ndb2_got(log_db,p->userid,sizeof(reslogs)+n*sizeof(reslog));// if(!logs)return;//db error 由于持有锁，直接崩溃吧
    reslog& item=logs->a[n-1];
    memcpy(item.model,model.data(),min(model.size(),sizeof(item.model)-1));
    memcpy(item.provider,provider.data(),min(provider.size(),sizeof(item.provider)-1));
    item.used_tokens=used_tokens;
    item.multiply=multiply;
    item.time=time(0);
    UNLOCK(logs->lock);
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
    RET_JSON(response);
}
void gpt5_history_list(http_para* a) {
    user_* p=getuser(a->get);
    if(!p)return my_http_error(a,"Please log in first.");
    history*h=(history*)ndb2_got(history_db,p->userid,0);
    if(!h)return http_send(a,Hok Hjson Hc0,"[]",0);
    cppJSON ans("[]");
    for(int i=h->n>50?h->n-50:0;i<h->n;i++){
        content* con=(content*)ndb2_got(content_db,h->con_id[i],0);
        if(!con||con->deleted)continue;
        cppJSON item("{}");
        item.insert("con_id",con->con_id);
        item.insert("name",con->name);
        item.insert("updatetime",(double)con->updatetime);
        ans.push_back(std::move(item));
    }
    RET_JSON(ans);
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
    RET_JSON(ans);
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
void insert2index_db(const string&a,const string&b){
    char tmp[48]={0};
    mylib_sha256(a.c_str(),a.length(),tmp);
    char* p=(char*)ndb2_got(index_db,tmp,b.length()+10);
    if(p)memcpy(p,b.c_str(),b.length());
}
void maketitle(char*name,string b,const cppJSON& config){
    if(!name[0]){
        cppJSON req("{\"stream\":false}"),conf=config["title"];
        req.insert("model",conf["model"].valuestring());
        cppJSON tmp("[{\"role\":\"user\"}]");
        b="将以下回复的内容取一个简短的标题：\n"+b;
        tmp[0].insert("content",b.c_str());
        req.insert("messages",std::move(tmp));
        string _;
        char* tp=req.PrintUnformatted();
        if(!tp)return;
        cppJSON title_reply=gpt6_work(0,conf["url"],conf["Authorization"],tp,"completions",_);
        cppJSON::free_str(tp);
        string title=title_reply[0]["content"];
        if(title.size()>4&&title.substr(0,2)=="**"&&title.substr(title.size()-2)=="**")title=title.substr(2,title.size()-4);
        if(title.size()>60)title=title.substr(0,utf8_substr(title.c_str(),56))+"...";
        if(!title.empty())strcpy(name,title.c_str());
    }
}
#define ST_D (3*24*4)
void gpt5_add(string a,bool stable){
    int (*c)[2] = (int (*)[2])ndb2_got(stable_db,a.c_str(),(ST_D*2+2)*4);
    if(!c)return;
    LOCK(&c[ST_D][1]);
    int t=time(0)/(15*60),l=c[ST_D][0];
    for(int i=l+1;i<=t&&i<=l+ST_D;i++)c[i%ST_D][0]=c[i%ST_D][1]=0;
    c[t%ST_D][0]+=stable;
    c[t%ST_D][1]++;
    c[ST_D][0]=t;
    UNLOCK(c[ST_D][1]);
}
static bool gpt5_should_probe(const string& key) {
    int (*c)[2]=(int (*)[2])ndb2_got(stable_db,key.c_str(),0);
    if(!c)return false;
    LOCK(&c[ST_D][1]);
    int t=time(0)/(15*60),l=c[ST_D][0];
    for(int i=l+1;i<=t&&i<=l+ST_D;i++)c[i%ST_D][0]=c[i%ST_D][1]=0;
    c[ST_D][0]=t;
    int stable2=0,total2=0,stable4=0,total4=0;
    for(int i=t-15;i<=t;i++) {
        int index=(i+ST_D)%ST_D;
        stable4+=c[index][0];
        total4+=c[index][1];
        if(i>=t-7) {
            stable2+=c[index][0];
            total2+=c[index][1];
        }
    }
    UNLOCK(c[ST_D][1]);
    return (total2>0&&stable2*5<=total2)||(total4>0&&stable4*2<=total4);
}
static void gpt5_probe(const string& model,const string& provider,const cppJSON& conf) {
    string url=conf["url"],auth=conf["Authorization"];
    if(url.empty()||auth.empty())return;
    cppJSON request("{\"stream\":false,\"messages\":[{\"role\":\"user\",\"content\":\"你好\"}]}");
    request.insert("model",model);
    string response_id;
    int returncode=0;
    unsigned long long used_tokens=0;
    char* message=request.PrintUnformatted();
    if(!message)return;
    cppJSON output=gpt6_work(0,url,auth,message,"completions",response_id,&used_tokens,&returncode);
    cppJSON::free_str(message);
    string key=model+"_"+provider;
    gpt5_add(key,used_tokens>0);
}
void* gpt5_probe_loop(void*) {
    const time_t interval=2*60*60;
    for(;;) {
        time_t next=(time(0)/interval+1)*interval;
        while(time(0)<next)sleep(next-time(0));
        cppJSON config=cppJSON::from_file(CONFIG);
        if(!config)continue;
        for(cppJSON item:config["model"]) {
            string model=item.a->string;
            for(cppJSON value:item["provider"]) {
                string provider=value;
                cppJSON conf=config["provider"][provider.c_str()];
                if(!conf)continue;
                if(gpt5_should_probe(model+"_"+provider))gpt5_probe(model,provider,conf);
            }
        }
    }
    return 0;
}
void gpt5_askstable(http_para* a) {
    cppJSON ask(a->get+a->n),ans("{}");
    for(cppJSON i:ask){
        string tp=i;
        int (*c)[2] = (int (*)[2])ndb2_got(stable_db,tp.c_str(),0);
        if(!c)continue;
        int t=time(0)/(15*60),l=c[ST_D][0];
        if(l<t)LOCK(&c[ST_D][1]);
        for(int i=l+1;i<=t&&i<=l+ST_D;i++)c[i%ST_D][0]=c[i%ST_D][1]=0;
        c[ST_D][0]=t;
        cppJSON tmp("[]");
        for(int i=0;i<ST_D;i++){
            tmp.push_back((double)c[i][0]);
            tmp.push_back((double)c[i][1]);
        }
        if(l<t)UNLOCK(c[ST_D][1]);
        ans.insert(i.valuestring().c_str(),std::move(tmp));
    }
    RET_JSON(ans);
}
#define key_find(str) do{char*t=strcasestr(a->get,str);if(t)tmp=t+strlen(str);}while(0)
static user_* gpt5_api_user(http_para* a) {
    char *tmp=0;
    key_find("Authorization: Bearer sk-");
    if(!tmp)key_find("Authorization: sk-");
    if(!tmp)key_find("x-api-key: sk-");
    user_* p=getuser_by_id(tmp);
    if(p&&tmp&&memcmp(tmp+8,p->gptapikey,19))p=0;
    return p;
}
static void gpt5_completion_request(http_para* a,const string& format,const char* array_name) {
    user_* p=gpt5_api_user(a);
    if(!p)return my_http_error(a,"Invalid API key.");
    cppJSON request(a->get+a->n),config=cppJSON::from_file(CONFIG),conf;
    string model=gpt6_request_model(a,request,format);
    if(!config)return my_http_error(a,"can not read gpt4.json.");
    if(!config["model"].has(model.c_str()))return my_http_error(a,"Model not found.");
    cppJSON pros=config["model"][model]["provider"];
    string key=(string)p->userid+"_"+model;
    char* saved=(char*)ndb2_got(provider_db,key.c_str(),0);
    string provider;
    if(pros.has(saved)&&(p->admin||config["provider"][saved]["public"]==true)){
        conf=config["provider"][saved];
        provider=saved;
    } else for(cppJSON pro:pros){
        cppJSON tmp=config["provider"][pro];
        if(tmp&&(p->admin||tmp["public"]==true)){
            conf=tmp;
            provider=pro;
            break;
        }
    }
    if(!conf)return my_http_error(a,"Permission denied.");
    if(!p->token_limit)p->token_limit=10000000ULL;
    if(p->token_used>=p->token_limit)return my_http_error(a,"余额不足，访问 https://www.neuqboard.cn/token 获取更多信息");
    string auth=conf["Authorization"];
    if(auth.empty())return my_http_error(a,"gpt4.json error: No Authorization");
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
    int returncode=0;
    cppJSON output=gpt6_work(a,conf["url"],auth,a->get+a->n,format,response_id,&used_tokens,&returncode);
    // if(returncode<400||returncode>499||returncode==429)
    gpt5_add(model+"_"+provider,used_tokens>0);
    double mul=config["model"][model]["price"][0].valuedouble()*GPT5_TOKEN_C*conf["multiply"].valuedouble()/0.3;
    ADD(&p->token_used,(long long)ceil(used_tokens*mul));
    gpt5_log(p,model,provider,used_tokens,mul);
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
void gpt5_models(http_para* a) {
    user_* p=gpt5_api_user(a);
    if(!p)return http_send(a,H401 Hjson Hc0,"{\"error\":{\"message\":\"Invalid API key.\"}}",0);
    cppJSON config=cppJSON::from_file(CONFIG);
    if(!config)return http_send(a,H500 Hjson Hc0,"{\"error\":{\"message\":\"can not read gpt4.json.\"}}",0);
    cppJSON data("[]");
    for(cppJSON model:config["model"]) {
        bool available=false;
        for(cppJSON provider:model["provider"]) {
            string name=provider;
            cppJSON conf=config["provider"][name.c_str()];
            if(conf&&(p->admin||conf["public"]==true)) {
                available=true;
                break;
            }
        }
        if(!available)continue;
        cppJSON item("{}");
        item.insert("id",model.a->string);
        item.insert("object","model");
        item.insert("created",0.0);
        item.insert("owned_by","neuqboard");
        data.push_back(std::move(item));
    }
    cppJSON response("{}");
    response.insert("object","list");
    response.insert("data",std::move(data));
    RET_JSON(response);
}
void gpt5_model_list(http_para* a) {
    cppJSON config=cppJSON::from_file(CONFIG);
    if(!config)return http_send(a,Hok Hc0 Htxt,"can not read gpt4.json.",0);
    config.erase("title");
    for(cppJSON provider:config["provider"]) {
        provider.erase("name");
        provider.erase("url");
        provider.erase("Authorization");
    }
    RET_JSON(config);
}
