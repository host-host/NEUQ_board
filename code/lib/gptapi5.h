#ifndef GPTAPI5_H
#define GPTAPI5_H
#ifdef __cplusplus
extern "C"{
#endif

#include "http.h"
#include "ndb2.h"
#include <time.h>
struct content{
    bool publish;
    char deleted;//0没删，1软删除但保留，2即将硬删除
    bool isusing;
    char ownerid[10];
    char ownername[24];
    long long createtime;
    long long updatetime;
    char name[64];
    char format[20];
    char con_id[32];
    char hash[44];
    char other[1024-44];
    char content[0];
};
struct reslog{
    char model[48],provider[48];
    int used_tokens;
    double multiply;
    long long time;
    long long input,output,cache,makecache;
    int websearch;
    int deprecated1;
    long long deprecated2;
    long long start,first,last,end;
    int isimage;//其实含义已经变成了计费方式，0按总token 1按次
    int stable;//0不知道 1正常 2~999用户请求有问题 >=1000上游炸了
    char info[128];
    char isauto;
    char other1,other2;//保留
    char isswitch;//1表示本次请求为auto请求并且失败了被下一个渠道自动切换
    long long useage;//事实扣费数额
    char other[256-4*sizeof(long long)-2*sizeof(long long)-4*sizeof(long long)-2*sizeof(int)-128-4-sizeof(long long)-4];//保留为未来增加功能，末尾减4计入useage前的对齐填充
};
struct reslogs{
    int lock,n;
    reslog a[];
};
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
extern ndb2 log_db;
void gptapi5_init();
void* gpt5_probe_loop(void*);
void gpt5_apikey(http_para* a);
void gpt5_log_list(http_para* a);
void gpt5_resolve(http_para* a);
void gpt5_history_list(http_para* a);
void gpt5_history_get(http_para* a);
void gpt5_history_rename(http_para* a);
void gpt5_history_delete(http_para* a);
void gpt5_share(http_para* a);
void gpt5_models(http_para* a);
void gpt5_askstable(http_para* a);
void gpt5_responses(http_para* a);
void gpt5_chat_completions(http_para* a);
void gpt5_claude_messages(http_para* a);
void gpt5_gemini_generate_content(http_para* a);
void gpt5_image_generations(http_para* a);
void gpt5_model_list(http_para *a);

#ifdef __cplusplus
}
#endif
#endif
