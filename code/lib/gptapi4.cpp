#include"gptapi4.h"
#include"gptapi6.h"
#include"cppJSON.h"
#include"ndb2.h"
#include"user.h"
#include"mylib.h"
#include<cstring>
#include<ctime>
#include<string>
ndb2 useagedb,stable_db,log_db;
void gpt4_init(){
    useagedb=ndb2_init("/web/res/pri/useage.ndb2");
    stable_db=ndb2_init("/web/res/pri/gpt5stable.ndb2");
    log_db=ndb2_init("/web/res/pri/gpt5log.ndb2");
}
static long long useage_day(long long time){
    return (time+8*3600)/86400;//北京时间自然日
}
void gpt4_adduseage(const char* apikey,const char* model,long long tokens,long long useage){
    if(!useagedb||!apikey||!*apikey||!model||!*model)return;
    std::string key=std::string(apikey+3,16)+model;
    if(key.size()>47)return;
    gpt4_uses* uses=(gpt4_uses*)ndb2_got(useagedb,key.c_str(),sizeof(gpt4_uses));
    if(!uses)return;
    long long now=time(0);
    if(useage_day(uses->time)!=useage_day(now))uses->today={};
    ADD(&uses->all.tokens,tokens);
    ADD(&uses->all.useage,useage);
    uses->today.tokens+=tokens;
    uses->today.useage+=useage;
    uses->time=now;
}
#define ERROR(H,message) http_send(a,H Hjson Hc0,"{\"error\":{\"message\":\"" message "\"}}",0)
void gpt4_askuseage(http_para* a){
    cppJSON req(a->get+a->n),ans("{}");
    std::string apikey=req["apikey"].valuestring();
    if(apikey.length()<3+8+19)return ERROR(H400,"Invalid API key.");
    user_* p=getuser_by_id(apikey.c_str()+3);
    if(p&&memcmp(apikey.c_str()+3+8,p->gptapikey,19))p=0;
    if(!p)return ERROR(H400,"Invalid API key.");
    char key[48]={0},pf[17];
    memcpy(key,apikey.data()+3,16);
    memcpy(pf,apikey.data()+3,16);
    long long today=useage_day(time(0));
    for(void* value=ndb2_next(useagedb,key);value;value=ndb2_next(useagedb,key)){
        if(strncmp(key,pf,16)!=0)break;
        if(ndb2_gotmaxlen(value)<(long long)sizeof(gpt4_uses))continue;
        gpt4_uses uses=*(gpt4_uses*)value;
        if(useage_day(uses.time)!=today)uses.today={};
        auto json=[](const gpt4_use& use){
            cppJSON item("{}");
            item.insert("tokens",(double)use.tokens);
            item.insert("useage",(double)use.useage);
            return item;
        };
        cppJSON item("{}");
        item.insert("all",json(uses.all));
        item.insert("today",json(uses.today));
        ans.insert(key+16,std::move(item));
    }
    http_send(a,Hok Hjson Hc0,ans.stringify_Unformatted().c_str(),0);
}
void gpt4_addstable(const char* a,bool stable,gpt6_ret* b,double o_cost,double cost,long long sell){
    stablelog* c=(stablelog*)ndb2_got(stable_db,a,sizeof(stablelog));
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
void gpt4_askstable(http_para* a) {
    cppJSON ask(a->get+a->n),ans("{}");
    for(cppJSON i:ask){
        std::string tp=i;
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
void gpt4_asklog(http_para* a) {
    user_* p=getuser(a->get);
    if(!p)return my_http_error(a,"Please log in first.");
    cppJSON req(a->get+a->n),ans(R"({"end":0,"item":[]})"),ite=ans["item"];
    int i=req["start"].valuedouble();
    reslogs* logs=(reslogs*)ndb2_got(log_db,p->userid,0);
    int n=logs?logs->n:0;
    if(i<0||i>=n)i=n-1;
    for(int start=i;i>=0&&i>start-100;i--){
        cppJSON item("{}");
        item.insert("model",logs->a[i].model);
        if(!logs->a[i].isauto)item.insert("provider",logs->a[i].provider);
        item.insert("isimage",(double)logs->a[i].isimage);
        item.insert("used_tokens",(double)logs->a[i].used_tokens);
        item.insert("input",(double)logs->a[i].input);
        item.insert("output",(double)logs->a[i].output);
        item.insert("cache",(double)logs->a[i].cache);
        item.insert("makecache",(double)logs->a[i].makecache);
        item.insert("makecache_1h",(double)logs->a[i].makecache_1h);
        item.insert("cachereturn_refund",(double)logs->a[i].cachereturn_refund);
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
        item.insert("makecache_1hm",(double)logs->a[i].makecache_1hm);
        item.insert("websearchm",(double)logs->a[i].websearchm);
        item.insert("isswitch",logs->a[i].isswitch==1);
        item.insert("useage",(double)logs->a[i].useage);
        item.insert("time",(double)logs->a[i].time);
        item.insert("isauto",(double)logs->a[i].isauto);
        item.insert("info",logs->a[i].info);
        ite.push_back(std::move(item));
    }
    ans["end"]=(double)i;
    http_send(a,Hok Hjson Hc0,ans.stringify_Unformatted().c_str(),0);
}
void gpt4_addlog(const char* userid,const reslog* rl) {
    reslogs* logs;
    retry:
    logs=(reslogs*)ndb2_got(log_db,userid,0);
    if(!logs)if(!(logs=(reslogs*)ndb2_got(log_db,userid,sizeof(reslogs)+sizeof(reslog))))return;//db error
    if(TRY(&logs->lock))goto retry;
    int n=++logs->n;
    logs=(reslogs*)ndb2_got(log_db,userid,sizeof(reslogs)+n*sizeof(reslog));// if(!logs)return;//db error 由于持有锁，直接崩溃吧
    memcpy(&logs->a[n-1],rl,sizeof(reslog));
    UNLOCK(logs->lock);
}
