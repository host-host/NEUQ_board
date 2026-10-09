#include"gptapi4.h"
#include"cppJSON.h"
#include"ndb2.h"
#include"user.h"
#include<cstring>
#include<ctime>
#include<string>
ndb2 useagedb;//apikey[3:20]+modelname -> gpt4_uses
void gpt4_init(){
    useagedb=ndb2_init("/web/res/pri/useage.ndb2");
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
    uses->all.tokens+=tokens;
    uses->all.useage+=useage;
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
