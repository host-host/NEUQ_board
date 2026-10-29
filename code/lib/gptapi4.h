#ifndef GPTAPI4_H
#define GPTAPI4_H
#ifdef __cplusplus
extern "C"{
#endif

#include"http.h"
#include"ndb2.h"
void gpt4_init();
struct gpt6_ret;

typedef struct{
    long long tokens,useage;//总消耗token，总扣除额度
}gpt4_use;
typedef struct{
    gpt4_use all,today;
    long long time;//最后一次更新的时间戳
}gpt4_uses;
extern ndb2 useagedb;//apikey[3:20]+modelname -> gpt4_uses
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
extern ndb2 stable_db;//model_provider -> stablelog
void gpt4_addstable(const char* a,bool stable,gpt6_ret* b,double o_cost,double cost,long long sell);
void gpt4_askstable(http_para* a);
struct reslog{
    char model[48],provider[48];
    int used_tokens;
    int cachereturn;//计算出的应缓存token，当cachereturn>cache时执行补偿
    double multiply;//沿用历史单位：isimage=0时为额度/token，1时为额度/次；100万额度=0.3元
    long long time;
    long long input,output,cache,makecache;//makecache 包含 makecache_1h
    int websearch;
    int makecache_1h;
    double cost;//真实成本
    long long start,first,last,end;
    int isimage;//计费方式，0按总token(used_tokens*multiply)，1按次(used_tokens*multiply)，2按(max(input-cache-makecache,0)*inputm+output*outputm+cache*cachem+(makecache-makecache_1h)*makecachem+makecache_1h*makecache_1hm)/1000000+websearch*websearchm，3表示卡密充值(useage为增加的额度)
    int stable;//0不知道 1正常 2~999用户请求有问题 >=1000上游炸了
    char info[128];
    char isauto;
    char other1,other2;//保留
    char isswitch;//1表示本次请求为auto请求并且失败了被下一个渠道自动切换
    int cachereturn_refund;//真实退还金额，以这个为准
    long long useage;//事实扣费数额
    float inputm,outputm,cachem,makecachem,websearchm,makecache_1hm;
};
struct reslogs{
    int lock,n;
    reslog a[];
};
extern ndb2 log_db;//userid -> reslogs
void gpt4_asklog(http_para* a);
void gpt4_addlog(const char* userid,const reslog* rl);

#ifdef __cplusplus
}
#endif
#endif
