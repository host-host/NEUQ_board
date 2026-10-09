#ifndef GPTAPI4_H
#define GPTAPI4_H
#ifdef __cplusplus
extern "C"{
#endif

#include"http.h"
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

#ifdef __cplusplus
}
#endif
#endif
