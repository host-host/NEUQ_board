#include "gptapi5.h"
#include "gptapi4.h"
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
#include <vector>
using namespace std;
static_assert(sizeof(reslog)==376,"reslog size error");
#define CONFIG "/web/res/pri/gpt4.json"
#define GPT5_TOKEN_C 1
#define ll long long
ndb2 content_db;//con_id -> content
ndb2 index_db;//sha256(response_id) -> con_id
ndb2 history_db;//user_id -> history
ndb2 provider_db;//userid_model -> preferred provider
ndb2 stable_db;//model_provider -> [3*24*4][2]
ndb2 log_db;//userid -> reslogs
struct history{
    int n;
    char user_id[10];
    char con_id[][32];
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
struct pro_ret1{
    string provider;
    cppJSON price,cost;//卖价和成本价
};
struct pro_ret{
    vector<pro_ret1>providers;
    bool isauto;
};
static void mergeprice(cppJSON& price,const cppJSON& overrides){
    for(cppJSON i:overrides){
        string key=i.namestring();
        cppJSON current=price[key];
        if(current.IsObject()&&i.IsObject())mergeprice(current,i);
        else price.insert(key.c_str(),i);
    }
}
void initproret(pro_ret1*tmp,cppJSON a,cppJSON price){
    tmp->price=price.clone();
    if(!tmp->price.IsObject())tmp->price=cppJSON("{}");
    tmp->cost=price.clone();
    if(!tmp->cost.IsObject())tmp->cost=cppJSON("{}");
    mergeprice(tmp->price,a["price"]);
    mergeprice(tmp->cost,a["cost"]);
}
bool checkpro(pro_ret&a,user_*p,cppJSON& config,string& model,string saved){
    if(saved=="auto"){
        cppJSON jmodel=config["model"][model],aut=jmodel["auto"];
        if(!aut)return 0;
        pro_ret1 tmpre;
        initproret(&tmpre,jmodel["auto"],jmodel["price"]);
        for(cppJSON i:aut["provider"]){
            cppJSON tmp=config["provider"][i];
            if(tmp&&(p->admin||tmp["public"]==true)){
                tmpre.provider=i;
                pro_ret1 tmp2;
                initproret(&tmp2,tmp,jmodel["price"]);
                tmpre.cost=std::move(tmp2.cost);
                a.providers.push_back(tmpre);
            }
        }
        if(a.providers.size()){
            a.isauto=1;
            return 1;
        }
        return 0;
    }
    cppJSON tmp=config["provider"][saved];
    if(tmp&&(p->admin||tmp["public"]==true)){
        pro_ret1 tmp2;
        initproret(&tmp2,tmp,config["model"][model]["price"]);
        tmp2.provider=saved;
        a.providers.push_back(std::move(tmp2));
        a.isauto=0;
        return 1;
    }
    return 0;
}
pro_ret gotprovider(user_*p,cppJSON& config,string& model){
    pro_ret ans;
    string key=(string)p->userid+"_"+model;
    char* saved=(char*)ndb2_got(provider_db,key.c_str(),0);
    cppJSON pros=config["model"][model]["provider"];
    if(pros.has(saved)&&checkpro(ans,p,config,model,saved))return ans;
    for(cppJSON pro:pros)if(checkpro(ans,p,config,model,pro))return ans;
    return ans;
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
        string model=item.a->string;
        pro_ret sel=gotprovider(p,config,model);
        if(sel.providers.size()>0)selected.insert(model.c_str(),sel.isauto?(string)"auto":sel.providers[0].provider);
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
        item.insert("isimage",(double)logs->a[i].isimage);
        item.insert("used_tokens",(double)logs->a[i].used_tokens);
        item.insert("input",(double)logs->a[i].input);
        item.insert("output",(double)logs->a[i].output);
        item.insert("cache",(double)logs->a[i].cache);
        item.insert("makecache",(double)logs->a[i].makecache);
        item.insert("websearch",(double)logs->a[i].websearch);
        long long first=logs->a[i].start>0?logs->a[i].first-logs->a[i].start:-1;
        if(first>=0)item.insert("first",first/1000000000.0);
        else item.insert("first",(const char*)0);
        item.insert("total",(logs->a[i].end-logs->a[i].start)/1000000000.0);
        item.insert("multiply",logs->a[i].multiply);
        item.insert("inputm",(double)logs->a[i].inputm);
        item.insert("outputm",(double)logs->a[i].outputm);
        item.insert("cachem",(double)logs->a[i].cachem);
        item.insert("makecachem",(double)logs->a[i].makecachem);
        item.insert("websearchm",(double)logs->a[i].websearchm);
        item.insert("isswitch",logs->a[i].isswitch==1);
        item.insert("useage",(double)logs->a[i].useage);
        item.insert("time",(double)logs->a[i].time);
        item.insert("isauto",(double)logs->a[i].isauto);
        item.insert("info",logs->a[i].info);
        ans.push_back(std::move(item));
    }
    http_send(a,Hok Hjson Hc0,ans.stringify_Unformatted().c_str(),0);
}
static void gpt5_log(user_* p,const string& model,const string& provider,gpt6_ret&b,bool isauto,bool isswitch,reslog* rl,double cost) {
    reslogs* logs;
    retry:
    logs=(reslogs*)ndb2_got(log_db,p->userid,0);
    if(!logs)if(!(logs=(reslogs*)ndb2_got(log_db,p->userid,sizeof(reslogs)+sizeof(reslog))))return;//db error
    if(TRY(&logs->lock))goto retry;
    int n=++logs->n;
    logs=(reslogs*)ndb2_got(log_db,p->userid,sizeof(reslogs)+n*sizeof(reslog));// if(!logs)return;//db error 由于持有锁，直接崩溃吧
    reslog* item=&logs->a[n-1];
    memcpy(item,rl,sizeof(reslog));
    memcpy(item->model,model.data(),min(model.size(),sizeof(item->model)-1));
    memcpy(item->provider,provider.data(),min(provider.size(),sizeof(item->provider)-1));
    // item->used_tokens;在gpt5_cost中填充
    // item->bl;
    // item->multiply;在gpt5_cost中填充
    item->time=time(0);
    item->input=b.input;
    item->output=b.output;
    item->cache=b.cache;
    item->makecache=b.makecache;
    item->websearch=b.websearch;
    // item->makecache_1h
    item->cost=cost;
    item->start=b.start_ns;
    item->first=b.first_ns;
    item->last=b.last_ns;
    item->end=b.end_ns;
    // item->isimage;在gpt5_cost中填充
    item->stable=b.stable;
    memcpy(item->info,b.info.data(),min(b.info.size(),sizeof(item->info)-1));
    item->isauto=isauto;
    // item->other1,other2
    item->isswitch=isswitch;
    // item->bl2
    // item->useage=useage;在gpt5_cost中填充
    // item->inputm,outputm,cachem,makecachem,websearchm,makecache_1hm;在gpt5_cost中填充
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
    con->deleted=1;
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
double gpt5_cost(gpt6_ret* usage,const cppJSON& price,reslog* rl) {//填充倍率和useage和used_tokens
    memset(rl,0,sizeof(reslog));
    rl->used_tokens=usage->used_tokens;
    double mul=price["multiply"].valuedouble();
    if(mul==0)mul=1;
    if(price["dollar"]==true)mul*=7;
    string format=price["format"];
    if(format=="per"){
        mul*=price["input"].valuedouble();
        rl->isimage=1;
        rl->multiply=mul/0.3*1000000;
        rl->useage=ceil(rl->multiply*rl->used_tokens);
        return rl->useage*0.3/1000000;
    }
    if(format=="token"){
        mul*=price["input"].valuedouble();
        rl->isimage=0;
        rl->multiply=mul*10/3;
        rl->useage=ceil(rl->multiply*rl->used_tokens);
        return rl->useage*0.3/1000000;
    }
    auto f=[](gpt6_ret* usage,const cppJSON& a,double mu,reslog* rl){
        rl->isimage=2;
        double input=usage->input-usage->cache-usage->makecache;
        rl->inputm=a["input"].valuedouble()*mu;
        rl->outputm=a["output"].valuedouble()*mu;
        rl->cachem=a["cache"].valuedouble()*mu;
        rl->makecachem=a["makecache"].valuedouble()*mu;
        rl->websearchm=a["websearch"].valuedouble()*mu;
        double ans=rl->inputm*(input>0?input:0)/1000000+
            rl->outputm*usage->output/1000000+
            rl->cachem*usage->cache/1000000+
            rl->makecachem*usage->makecache/1000000+
            rl->websearchm*usage->websearch;
        rl->useage=ceil(ans/0.3*1000000);
        return ans;
    };
    if(format=="normal")return f(usage,price,mul,rl);
    if(format=="deepseek"){// 暂不处理节假日和调休。
        time_t bj=(usage->start_ns>0?usage->start_ns/1000000000LL:time(0))+8*3600;
        tm t{};
        gmtime_r(&bj,&t);
        int minutes=t.tm_hour*60+t.tm_min;
        bool d=t.tm_wday>=1&&t.tm_wday<=5&&((minutes>=9*60&&minutes<12*60)||(minutes>=14*60&&minutes<18*60));
        return f(usage,price,mul*(d?2:1),rl);
    }
    if(format=="lengthdouble")return usage->input>price["length"].valuedouble()?f(usage,price["newprice"],mul,rl):f(usage,price,mul,rl);
    if(format=="claude")return f(usage,price,mul,rl);
    return 0;
}
void gpt5_add(string a,bool stable,gpt6_ret* b,double o_cost=0,double cost=0,long long sell=0){
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
        c->allwebsearch+=b->websearch;
        if(b->start_ns>0&&b->first_ns>=b->start_ns){
            c->latency+=(b->first_ns-b->start_ns)/1000000000LL;
            c->latency_n++;
        }
        if(b->start_ns>0&&b->end_ns>=b->start_ns){
            c->alltime+=(b->end_ns-b->start_ns)/1000000000LL;
            c->alltime_tokens+=b->output;
        }
    }
    c->o_cost+=o_cost;
    c->cost+=cost;
    c->sell+=sell;
    UNLOCK(c->lock);
}
static bool gpt5_should_skip(const string& key) {
    stablelog* cc=(stablelog*)ndb2_got(stable_db,key.c_str(),0);
    if(!cc||(unsigned long long)ndb2_gotmaxlen(cc)<sizeof(stablelog))return false;
    LOCK(&cc->lock);
    int t=time(0)/(15*60);
    bool skip=true;
    for(int window=0;window<2&&skip;window++) {
        long long stable=0,total=0;
        for(int i=t-window*8-7;i<=t-window*8;i++) {
            if(i>cc->uptime||i<=cc->uptime-ST_D)continue;
            int index=(i+ST_D)%ST_D;
            stable+=cc->c[index][0];
            total+=cc->c[index][1];
        }
        if(total==0||stable*20>=total)skip=false;
    }
    UNLOCK(cc->lock);
    return skip;
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
    cppJSON request("{\"messages\":[{\"role\":\"user\",\"content\":\"hello\"}]}");
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
            if(item["suggest_format"]=="image")continue;
            string model=item.a->string;
            set<string>s;
            for(cppJSON value:item["provider"])s.insert((string)value);
            for(cppJSON value:item["auto"]["provider"])s.insert((string)value);
            for(const string& provider:s) {
                if(provider=="auto")continue;
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
        stats.insert("latency",(double)c1.latency);
        stats.insert("latency_n",(double)c1.latency_n);
        stats.insert("alltime",(double)c1.alltime);
        stats.insert("alltime_tokens",(double)c1.alltime_tokens);
        ans.insert(i.valuestring().c_str(),std::move(stats));
    }
    return http_send(a,Hok Hjson Hc0,ans.stringify_Unformatted().c_str(),0);
}
void makelog(gpt6_ret*ans,const char* model,const char* message,const char*name,string&provider,bool isauto){
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
    field("websearch",to_string(ans->websearch));
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
    field("stable",to_string(ans->stable));
    field("req",message);
    field("res",'\n'+ans->header+ans->body);
    FILE* fout=fopen(filename.c_str(),"a");
    if(fout){
        fwrite(content.data(),1,content.size(),fout);
        fclose(fout);
    }
}
#define key_find(str) do{char*t=strcasestr(a->get,str);if(t)tmp=t+strlen(str);}while(0)
user_* gpt5_api_user(http_para* a,string* apikey=nullptr) {
    char *tmp=0;
    key_find("Authorization: Bearer sk-");
    if(!tmp)key_find("Authorization: sk-");
    if(!tmp)key_find("x-api-key: sk-");
    user_* p=getuser_by_id(tmp);
    if(p&&tmp&&memcmp(tmp+8,p->gptapikey,19))p=0;
    if(p&&apikey)*apikey="sk-"+string(tmp,27);
    return p;
}
void gpt5_coreapi(http_para*a,const char* format,const char* array_name){
    string apikey;
    user_* p=gpt5_api_user(a,&apikey);
    if(!p)return ERROR(H400,"Invalid API key.");
    cppJSON req(a->get+a->n),config=cppJSON::from_file(CONFIG);
    string model=gpt6_request_model(a,req,format);
    if(!config["model"].has(model))return ERROR(H400,"Model not found.");
    pro_ret proret=gotprovider(p,config,model);
    if(proret.providers.empty())return ERROR(H400,"Model not found.");
    if(p->token_used>=p->token_limit)return ERROR(H400,"余额不足，访问 https://www.neuqboard.cn/token 获取更多信息");
    gpt6_ret b;
    long long startns=0;
    for(int k=0;k<(int)proret.providers.size();k++){
        string& provider=proret.providers[k].provider;
        bool isauto=proret.isauto,last=k+1==(int)proret.providers.size();
        if(isauto&&!last&&gpt5_should_skip(model+"_"+provider))continue;
        b=gpt6_work3(a,a->get+a->n,model.c_str(),config["provider"][provider],format,last);
        if(memcmp(a->get,"POST /api",9)!=0&&b.send){//网页端阻塞到标题创建完成
            close(a->cl);
            a->cl=0;
        }
        if(b.stable==0||config["file"]==true)makelog(&b,model.c_str(),a->get+a->n,p->name,provider,isauto);//写入日志文件
        if(!b.send){
            if(!startns)startns=b.start_ns;
            if(b.end_ns-startns>2*60*1000000000ll){//2分钟后就不再重试渠道了
                b.send=1;
                gpt6_flush(&b);
            }
        }
        reslog rl;
        double ori=gpt5_cost(&b,config["model"][model]["price"],&rl);//原价
        double cost=gpt5_cost(&b,proret.providers[k].cost,&rl);//成本
        gpt5_cost(&b,proret.providers[k].price,&rl);//售价
        if(b.send==0)rl.useage=0;//渠道炸了，不收费
        ADD(&p->token_used,rl.useage);//加入用量
        gpt4_adduseage(apikey.c_str(),model.c_str(),strcmp(format,"image")==0?0:b.used_tokens,rl.useage);
        if(b.stable<2||b.stable>=1000)gpt5_add(model+"_"+provider,b.stable==1,&b,ori,cost,rl.useage);//稳定性统计
        gpt5_log(p,model,provider,b,isauto,!b.send,&rl,cost);//写入个人日志
        if(b.send){
            if(isauto)if(b.stable<2||b.stable>=1000)gpt5_add(model+"_auto",b.stable==1,&b,ori,cost,rl.useage);//稳定性统计auto
            break;
        }
    }
    if(config["nohistory"]==true)return;//可以在配置文件里不让进历史记录（磁盘要满了的临时处理方法）
    if(!array_name)return;//image 不进历史记录功能
    cppJSON input=req[array_name].clone(),oldinput=my_format(input,format);
    for(auto i:b.append)input.push_back(i);
    string inp=input.stringify_Unformatted(),new_input=(string)"new_input_"+p->userid+my_format(input,format).stringify_Unformatted();
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
    if(b.used_tokens<=0||(b.append.IsArray()&&b.append.size()==0))return;
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
    gpt5_coreapi(a,"image",nullptr);
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
        provider.erase("cost");
    }
    http_send(a,Hok Hjson Hc0,config.stringify_Unformatted().c_str(),0);
}
