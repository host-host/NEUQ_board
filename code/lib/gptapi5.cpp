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
#include <curl/curl.h>
using namespace std;
#define CONFIG "/web/res/pri/gpt4.json"
#define IMAGE_CONFIG "/web/res/pri/image.json"
#define GPT5_TOKEN_C 0.75
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
    long long input,output,cache,makecache;
    time_t first_deprecated,total_deprecated;
    long long start,first,last,end;
    int isimage;
    char other[256-4*sizeof(long long)-2*sizeof(time_t)-4*sizeof(ll)-sizeof(int)];//保留为未来增加功能
};
struct reslogs{
    int lock,n;
    reslog a[];
};
void gptapi5_init() {
    content_db=ndb2_init("/web/res/pri/gpt5content.ndb2");
    index_db=ndb2_init("/web/res/pri/gpt5sha256.ndb2");
    history_db=ndb2_init("/web/res/pri/gpt5userhistory.ndb2");
    provider_db=ndb2_init("/web/res/pri/gpt5provider.ndb2");
    stable_db=ndb2_init("/web/res/pri/gpt5stable.ndb2");
    log_db=ndb2_init("/web/res/pri/gpt5log.ndb2");
    curl_global_init(CURL_GLOBAL_DEFAULT);
    pthread_t thread_id;
    if(!pthread_create(&thread_id,0,gpt5_probe_loop,0))pthread_detach(thread_id);
    else exit(-98);
}
#define ERROR(H,message) http_send(a,H Hjson Hc0,"{\"error\":{\"message\":\"" message "\"}}",0)
string gotprovider(user_*p,cppJSON& config,string& model){
    string key=(string)p->userid+"_"+model;
    char* saved=(char*)ndb2_got(provider_db,key.c_str(),0);
    cppJSON pros=config["model"][model]["provider"],c_p=config["provider"];
    if(pros.has(saved)&&c_p[saved]&&(p->admin||c_p[saved]["public"]==true))return saved;
    for(cppJSON pro:pros)if(c_p[pro]&&(p->admin||c_p[pro]["public"]==true))return pro;
    return "";
}
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
    if(!p->token_limit&&p->admin)p->token_limit=10000000ULL;
    cppJSON ans("{}");
    ans.insert("api_key",(string)"sk-"+p->userid+p->gptapikey);
    ans.insert("token_limit",(double)p->token_limit);
    ans.insert("token_used",(double)p->token_used);
    ans.insert("admin",p->admin!=0);
    cppJSON selected("{}");
    for(cppJSON item:config["model"]) {
        string model=item.a->string,sel=gotprovider(p,config,model);
        if(!sel.empty())selected.insert(model.c_str(),sel);
    }
    ans.insert("selected_provider",std::move(selected));
    http_send(a,Hok Hjson Hc0,ans.stringify_Unformatted().c_str(),0);
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
        item.insert("isimage",logs->a[i].isimage==1);
        item.insert("used_tokens",(double)logs->a[i].used_tokens);
        item.insert("input",(double)logs->a[i].input);
        item.insert("output",(double)logs->a[i].output);
        item.insert("cache",(double)logs->a[i].cache);
        item.insert("makecache",(double)logs->a[i].makecache);
        long long first,total;
        if(logs->a[i].start>0){
            first=(logs->a[i].first-logs->a[i].start)/1000000000LL;
            total=(logs->a[i].end-logs->a[i].start)/1000000000LL;
        }else{
            first=logs->a[i].first_deprecated;
            total=logs->a[i].total_deprecated;
        }
        item.insert("first",(double)(first>0?first:0));
        item.insert("total",(double)(total>0?total:0));
        item.insert("multiply",logs->a[i].multiply);
        item.insert("time",(double)logs->a[i].time);
        ans.push_back(std::move(item));
    }
    http_send(a,Hok Hjson Hc0,ans.stringify_Unformatted().c_str(),0);
}
static void gpt5_log(user_* p,const string& model,const string& provider,gpt6_ret&b,double multiply,bool isimage=false) {
    reslogs* logs;
    retry:
    logs=(reslogs*)ndb2_got(log_db,p->userid,0);
    if(!logs)if(!(logs=(reslogs*)ndb2_got(log_db,p->userid,sizeof(reslogs)+sizeof(reslog))))return;//db error
    if(TRY(&logs->lock))goto retry;
    int n=++logs->n;
    logs=(reslogs*)ndb2_got(log_db,p->userid,sizeof(reslogs)+n*sizeof(reslog));// if(!logs)return;//db error 由于持有锁，直接崩溃吧
    reslog& item=logs->a[n-1];
    memset(&item,0,sizeof(item));
    memcpy(item.model,model.data(),min(model.size(),sizeof(item.model)-1));
    memcpy(item.provider,provider.data(),min(provider.size(),sizeof(item.provider)-1));
    item.used_tokens=b.used_tokens;
    item.input=b.input;
    item.output=b.output;
    item.cache=b.cache;
    item.makecache=b.makecache;
    item.start=b.start_ns;
    item.first=b.first_ns;
    item.last=b.last_ns;
    item.end=b.end_ns;
    item.multiply=multiply;
    item.time=time(0);
    item.isimage=isimage?1:0;
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
    http_send(a,Hok Hjson Hc0,response.stringify_Unformatted().c_str(),0);
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
void insert2index_db(const string&a,const string&b){
    char tmp[48]={0};
    mylib_sha256(a.c_str(),a.length(),tmp);
    char* p=(char*)ndb2_got(index_db,tmp,b.length()+10);
    if(p)memcpy(p,b.c_str(),b.length());
}
void maketitle(char*name,string b,const cppJSON& config){
    if(!name[0]){
        cppJSON req(R"({"model":0,"messages":[{"role":"user","content":0}]})");
        req["model"]=config["model"].valuestring();
        req["messages"][0]["content"]="将以下回复的内容取一个简短的标题：\n"+b;
        gpt6_ret a=gpt6_work3(0,req.stringify_Unformatted().c_str(),config["model"].valuestring().c_str(),config,"completions");
        string title=a.append[0]["content"];
        if(title.size()>4&&title.substr(0,2)=="**"&&title.substr(title.size()-2)=="**")title=title.substr(2,title.size()-4);
        if(title.size()>60)title=title.substr(0,utf8_substr(title.c_str(),56))+"...";
        if(!title.empty())strcpy(name,title.c_str());
    }
}
#define ST_D (3*24*4)
struct stablelog{
    int c[ST_D][2];
    int uptime,lock;
    long long s,input,output,cache,makecache,tokens;
    long long latency,latency_n,alltime,alltime_tokens;
};
void gpt5_add(string a,bool stable,gpt6_ret* b){
    stablelog* c=(stablelog*)ndb2_got(stable_db,a.c_str(),sizeof(stablelog));
    if(!c)return;//DB ERROR
    LOCK(&c->lock);
    int t=time(0)/(15*60),l=c->uptime;
    for(int i=l+1;i<=t&&i<=l+ST_D;i++)c->c[i%ST_D][0]=c->c[i%ST_D][1]=0;
    c->c[t%ST_D][0]+=stable;
    c->c[t%ST_D][1]++;
    c->uptime=t;
    if(b&&b->used_tokens>0){
        c->s++;
        c->input+=b->input;
        c->output+=b->output;
        c->cache+=b->cache;
        c->makecache+=b->makecache;
        c->tokens+=b->used_tokens;
        if(b->start_ns>0&&b->first_ns>=b->start_ns){
            c->latency+=(b->first_ns-b->start_ns)/1000000000LL;
            c->latency_n++;
        }
        if(b->start_ns>0&&b->end_ns>=b->start_ns){
            c->alltime+=(b->end_ns-b->start_ns)/1000000000LL;
            c->alltime_tokens+=b->output;
        }
    }
    UNLOCK(c->lock);
}
static bool gpt5_should_probe(const string& key) {
    stablelog* cc=(stablelog*)ndb2_got(stable_db,key.c_str(),sizeof(stablelog));
    if(!cc)return false;
    LOCK(&cc->lock);
    int (*c)[2]=cc->c;
    int t=time(0)/(15*60),l=cc->uptime;
    for(int i=l+1;i<=t&&i<=l+ST_D;i++)c[i%ST_D][0]=c[i%ST_D][1]=0;
    cc->uptime=t;
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
    UNLOCK(cc->lock);
    return (total2>0&&stable2*5<=total2)||(total4>0&&stable4*2<=total4);
}
static void gpt5_probe(const string& model,const string& provider,const cppJSON& conf) {
    cppJSON request("{\"messages\":[{\"role\":\"user\",\"content\":\"你好\"}]}");
    request.insert("model",model);
    gpt6_ret a=gpt6_work3(0,request.stringify_Unformatted().c_str(),model.c_str(),conf,"completions");
    string key=model+"_"+provider;
    gpt5_add(key,a.used_tokens>0,0);
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
        stablelog* cc=(stablelog*)ndb2_got(stable_db,tp.c_str(),0);
        if(!cc)continue;
        if((unsigned long long)ndb2_gotmaxlen(cc)<sizeof(stablelog))cc=(stablelog*)ndb2_got(stable_db,tp.c_str(),sizeof(stablelog));
        if(!cc)continue;//db error
        stablelog c1=*cc;
        int (*c)[2]=c1.c;
        int t=time(0)/(15*60),l=c1.uptime;
        for(int i=l+1;i<=t&&i<=l+ST_D;i++)c[i%ST_D][0]=c[i%ST_D][1]=0;
        cppJSON tmp("[]");
        for(int i=0;i<ST_D;i++){
            tmp.push_back((double)c[i][0]);
            tmp.push_back((double)c[i][1]);
        }
        cppJSON stats("{}");
        stats.insert("buckets",std::move(tmp));
        stats.insert("s",(double)c1.s);
        stats.insert("input",(double)c1.input);
        stats.insert("output",(double)c1.output);
        stats.insert("cache",(double)c1.cache);
        stats.insert("makecache",(double)c1.makecache);
        stats.insert("tokens",(double)c1.tokens);
        stats.insert("latency",(double)c1.latency);
        stats.insert("latency_n",(double)c1.latency_n);
        stats.insert("alltime",(double)c1.alltime);
        stats.insert("alltime_tokens",(double)c1.alltime_tokens);
        ans.insert(i.valuestring().c_str(),std::move(stats));
    }
    return http_send(a,Hok Hjson Hc0,ans.stringify_Unformatted().c_str(),0);
}
void makelog(gpt6_ret*ans,const char* model,const char* message,const char*name,string&provider){
    if(!ans||!ans->format||!ans->format[0])return;
    time_t now=time(0);
    struct tm local_time;
    if(!localtime_r(&now,&local_time))return;
    char timestamp[32];
    if(!strftime(timestamp,sizeof(timestamp),"%Y-%m-%d_%H-%M-%S.txt",&local_time))return;
    string directory="/web/log/"+string(ans->format);
    string filename=directory+"/"+timestamp;
    mkdir(directory.c_str(),0700);
    string content;
    content.reserve(256+ans->header.size()+ans->body.size()+ans->bodydelta.size());
    auto field=[&content](const char* name,const string& value){
        content+=name;
        content+='=';
        content+=value;
        content+='\n';
    };
    field("format",ans->format);
    field("append",ans->append.stringify_Unformatted());
    field("used_tokens",to_string(ans->used_tokens));
    field("input",to_string(ans->input));
    field("output",to_string(ans->output));
    field("cache",to_string(ans->cache));
    field("makecache",to_string(ans->makecache));
    field("start",to_string(ans->start_ns/1000000000LL));
    field("first",to_string(ans->first_ns/1000000000LL));
    field("last",to_string(ans->last_ns/1000000000LL));
    field("end",to_string(ans->end_ns/1000000000LL));
    field("curlcode",to_string(ans->curlcode));
    field("httpcode",to_string(ans->httpcode));
    field("issse",to_string(ans->issse));
    field("response_id",ans->response_id);
    field("model",model);
    field("user",name);
    field("provider",provider);
    field("req",message);
    field("res",'\n'+ans->header+ans->body);
    FILE* fout=fopen(filename.c_str(),"a");
    if(fout){
        fwrite(content.data(),1,content.size(),fout);
        fclose(fout);
    }
}
#define key_find(str) do{char*t=strcasestr(a->get,str);if(t)tmp=t+strlen(str);}while(0)
user_* gpt5_api_user(http_para* a) {
    char *tmp=0;
    key_find("Authorization: Bearer sk-");
    if(!tmp)key_find("Authorization: sk-");
    if(!tmp)key_find("x-api-key: sk-");
    user_* p=getuser_by_id(tmp);
    if(p&&tmp&&memcmp(tmp+8,p->gptapikey,19))p=0;
    return p;
}
void gpt5_coreapi(http_para*a,const char* format,const char* array_name){
    user_* p=gpt5_api_user(a);
    if(!p)return ERROR(H400,"Invalid API key.");
    cppJSON req(a->get+a->n),config=cppJSON::from_file(CONFIG);
    string model=gpt6_request_model(a,req,format);
    if(!config["model"].has(model))return ERROR(H400,"Model not found.");
    string provider=gotprovider(p,config,model);
    if(provider.empty())return ERROR(H500,"provider not found.");
    if(!p->token_limit&&p->admin)p->token_limit=10000000ULL;
    if(p->token_used>=p->token_limit)return ERROR(H400,"余额不足，访问 https://www.neuqboard.cn/token 获取更多信息");
    gpt6_ret b=gpt6_work3(a,a->get+a->n,model.c_str(),config["provider"][provider],format);
    if(memcmp(a->get,"POST /api",9)!=0){//网页端阻塞到标题创建完成
        close(a->cl);
        a->cl=0;
    }
    if(b.used_tokens<=0)makelog(&b,model.c_str(),a->get+a->n,p->name,provider);//写入日志文件
    if(b.httpcode!=404)gpt5_add(model+"_"+provider,b.used_tokens>0,&b);//稳定性统计
    double mul=config["model"][model]["price"][0].valuedouble()*GPT5_TOKEN_C*config["provider"][provider]["multiply"].valuedouble()/0.3;
    ADD(&p->token_used,(long long)ceil(b.used_tokens*mul));//加入用量
    gpt5_log(p,model,provider,b,mul);//写入个人日志
    cppJSON input=req[array_name].clone(),oldinput=my_format(input,format,2);
    for(auto i:b.append)input.push_back(i);
    string inp=input.stringify_Unformatted(),new_input=(string)"new_input_"+p->userid+my_format(input,format,2).stringify_Unformatted();
    char con_id[32]={0},newhash[44],hash[44];
    mylib_sha256(new_input.c_str(),new_input.length(),newhash);
    for(int i=1;i<=50&&oldinput.a&&oldinput.a->child;i++){
        string s=(string)"new_input_"+p->userid+oldinput.stringify_Unformatted();
        mylib_sha256(s.c_str(),s.length(),hash);
        content* cont=(content*)ndb2_got(content_db,(char*)ndb2_got(index_db,hash,0),0);
        if(cont&&!strcmp(cont->format,format)&&!strcmp(cont->hash,hash)){
            if(TRY(&cont->isusing))continue;
            if(strcmp(cont->hash,hash)){
                cont->isusing=0;
                continue;
            }
            cont=(content*)ndb2_got(content_db,cont->con_id,sizeof(content)+10+inp.length());
            if(!cont)exit(282);//DB error!
            memcpy(cont->hash,newhash,44);
            cont->updatetime=time(0);
            memcpy(cont->content,inp.c_str(),inp.length()+1);
            if(!b.response_id.empty())insert2index_db("response_id_"+b.response_id,cont->con_id);
            insert2index_db(new_input,cont->con_id);
            UNLOCK(cont->isusing);
            return;
        }
        oldinput.pop_back();
    }
    mylib_random_string(con_id,20);
    content*con=(content*)ndb2_got(content_db,con_id,sizeof(content)+inp.length()+10);
    if(!con)exit(283);//DB error!
    memset(con,0,sizeof(content));
    memcpy(con->ownerid,p->userid,8);
    memcpy(con->ownername,p->name,min(strlen(p->name),sizeof(con->ownername)-1));
    con->createtime=con->updatetime=time(0);
    memcpy(con->format,format,min(strlen(format),sizeof(con->format)-1));
    memcpy(con->con_id,con_id,strlen(con_id));
    memcpy(con->hash,newhash,44);
    memcpy(con->content,inp.c_str(),inp.length()+1);
    history* h=(history*)ndb2_got(history_db,p->userid,0);
    int n=h?h->n+1:1;
    h=(history*)ndb2_got(history_db,p->userid,sizeof(history)+32*n);
    if(h){
        h->n=n;
        if(n==1)memcpy(h->user_id,p->userid,8);
        memcpy(h->con_id[n-1],con_id,32);
    }
    if(!b.response_id.empty())insert2index_db("response_id_"+b.response_id,con_id);
    insert2index_db(new_input,con_id);
    if(b.used_tokens<=0)return;
    char title[64]={0};
    maketitle(title,b.append.stringify_Unformatted(),config["title"]);
    con=(content*)ndb2_got(content_db,con_id,0);
    if(con&&!con->name[0]&&title[0])memcpy(con->name,title,strlen(title)+1);
}
void gpt5_responses(http_para* a) {
    // LOG("%.*s\n",a->n,a->get);
    gpt5_coreapi(a,"responses","input");
}
void gpt5_chat_completions(http_para* a) {
    gpt5_coreapi(a,"completions","messages");
}
void gpt5_claude_messages(http_para* a) {
    gpt5_coreapi(a,"claude","messages");
}
void gpt5_gemini_generate_content(http_para* a) {
    gpt5_coreapi(a,"gemini","contents");
}
void gpt5_image_generations(http_para* a) {
    user_* p=gpt5_api_user(a);
    if(!p)return ERROR(H400,"Invalid API key.");
    if(!p->admin)return ERROR(H400,"Permission denied.");
    cppJSON request(a->get+a->n),config=cppJSON::from_file(IMAGE_CONFIG);
    string model=request["model"];
    if(!config["model"].has(model))return ERROR(H400,"Model not found.");
    string provider=gotprovider(p,config,model);
    if(provider.empty())return ERROR(H400,"provider not found.");
    if(!p->token_limit&&p->admin)p->token_limit=10000000ULL;
    if(p->token_used>=p->token_limit)return ERROR(H400,"余额不足，访问 https://www.neuqboard.cn/token 获取更多信息");
    gpt6_ret result=gpt6_work3(a,a->get+a->n,model.c_str(),config["provider"][provider],"image");
    bool success=result.curlcode==CURLE_OK&&result.httpcode>=200&&result.httpcode<300;
    double price=config["model"][model]["price"].valuedouble();
    double provider_multiply=config["provider"][provider]["multiply"].valuedouble();
    double mul=price/0.3*1000000.0*provider_multiply;
    result.used_tokens=success?1:0;
    if(success)ADD(&p->token_used,(long long)ceil(result.used_tokens*mul));
    gpt5_log(p,model,provider,result,mul,true);
    if(!success)LOG("image request failed: curl=%d http=%lld model=%s provider=%s",
        result.curlcode,result.httpcode,model.c_str(),provider.c_str());
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
    http_send(a,Hok Hjson Hc0,response.stringify_Unformatted().c_str(),0);
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
    http_send(a,Hok Hjson Hc0,config.stringify_Unformatted().c_str(),0);
}
