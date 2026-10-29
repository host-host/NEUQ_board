#ifndef GPTAPI4_H
#define GPTAPI4_H
#ifdef __cplusplus
extern "C"{
#endif

#include"http.h"
#include"ndb2.h"
typedef struct{
    long long tokens,useage;//总消耗token，总扣除额度
}gpt4_use;
typedef struct{
    gpt4_use all,today;
    long long time;//最后一次更新的时间戳
}gpt4_uses;
void gpt4_init();
void gpt4_adduseage(const char* apikey,const char* model,long long tokens,long long useage);
void gpt4_askuseage(http_para* a);
#define ST_D (3*24*4)
struct stablelog{
    int c[ST_D][2];
    int uptime,lock;
    long long s,input,output,cache,makecache,tokens;
    long long latency,latency_n,alltime,alltime_tokens;
    int allwebsearch;
    double o_cost,cost;//按原价成本,真实成本(¥)
    long long sell;//总扣费用户多少额度
};
extern ndb2 stable_db;
struct gpt6_ret;
void gpt4_addstable(const char* a,bool stable,gpt6_ret* b,double o_cost,double cost,long long sell);
void gpt4_askstable(http_para* a);

#ifdef __cplusplus
}
#endif
#endif
