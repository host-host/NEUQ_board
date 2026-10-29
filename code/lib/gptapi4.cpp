#include"gptapi4.h"
#include"gptapi6.h"
#include"cppJSON.h"
#include"ndb2.h"
#include"user.h"
#include"mylib.h"
#include<cstring>
#include<ctime>
#include<string>
ndb2 useagedb;//apikey[3:20]+modelname -> gpt4_uses
ndb2 stable_db;//model_provider -> [3*24*4][2]
void gpt4_init(){
    useagedb=ndb2_init("/web/res/pri/useage.ndb2");
    stable_db=ndb2_init("/web/res/pri/gpt5stable.ndb2");
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
