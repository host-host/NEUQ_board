#include "pay.h"
#include "cppJSON.h"
#include "gptapi5.h"
#include "mylib.h"
#include "ndb2.h"
#include "user.h"
#include <cctype>
#include <cstring>
#include <string>
#include <time.h>
using namespace std;
struct card_{
    int state;//0空节点 1可兑换 2已兑换
    long long value;
    long long createtime,usetime;
    char userid[10];
};
ndb2 card_db;//卡密(40B字符串) -> card_
void card_init(){
    card_db=ndb2_init("/web/res/pri/card.ndb2");
}
void admin_card_create(http_para* a){
    user_* p=getuser(a->get);
    if(!p||!(p->admin&2))return http_send(a,H403 Hjson Hc0,"{\"error\":{\"message\":\"Permission denied.\"}}",0);
    cppJSON req(a->get+a->n);
    int amount=req["amount"].valuedouble();
    int value=req["value"].valuedouble();
    if(amount>1000||value<0)return http_send(a,H403 Hjson Hc0,"{\"error\":{\"message\":\"amount or value error.\"}}",0);
    cppJSON ans("[]");
    for(int i=1;i<=amount;i++){
        char key[41]={0};
        card_* c;
        int p=mylib_random_string(key,40);
        if(p!=40)return http_send(a,H500 Hjson Hc0,"{\"error\":{\"message\":\"rand error.\"}}",0);
        if(!(c=(card_*)ndb2_got(card_db,key,sizeof(card_))))return http_send(a,H500 Hjson Hc0,"{\"error\":{\"message\":\"db error.\"}}",0);
        c->value=value;
        c->createtime=time(0);
        asm volatile("":::"memory");
        c->state=1;
        ans.push_back(key);
    }
    http_send(a,Hok Hjson Hc0,ans.stringify_Unformatted().c_str(),0);
}
void card_redeem(http_para* a){
    user_* p=getuser(a->get);
    if(!p)return http_send(a,H401 Hjson Hc0,"{\"error\":{\"message\":\"Please log in first.\"}}",0);
    cppJSON req(a->get+a->n);
    string key=req["card"].valuestring();
    card_* c=(card_*)ndb2_got(card_db,key.c_str(),0);
    if(!c||c->state==0)return http_send(a,H404 Hjson Hc0,"{\"error\":{\"message\":\"卡密不存在\"}}",0);
    if(!__sync_bool_compare_and_swap(&c->state,1,2))return http_send(a,H409 Hjson Hc0,"{\"error\":{\"message\":\"卡密已被使用\"}}",0);
    c->usetime=time(0);
    memcpy(c->userid,p->userid,8);
    ADD(&p->token_limit,c->value);
    reslog rl;
    memset(&rl,0,sizeof(rl));
    memcpy(rl.model,"card_redeem",11);
    rl.time=c->usetime;
    rl.isimage=3;
    rl.stable=1;
    rl.useage=c->value;
    string masked=key.substr(0,4)+"****"+key.substr(36);
    memcpy(rl.info,masked.data(),masked.size());
    gpt5_log_append(p->userid,&rl);
    http_send(a,Hok Hjson Hc0,"ok",0);
}
