#include"gptapi7.h"
#include"mylib.h"
#include"user.h"
#include<stdio.h>
#include<string.h>
#define ll long long
#define CONFIG "/web/res/pri/gpt4.json"
#define GPT5_TOKEN_C 0.75
#define ERROR(H,message) http_send(a,H Hjson Hc0,"{\"error\":{\"message\":\"" message "\"}}",0)
void gpt7_coreapi(http_para*a,const char* format,const char* arrname){
    char *tmp=0;
    #define key_find(str) do{char*t=strstr(a->get,str);if(t)tmp=t+strlen(str);}while(0)
    key_find("Authorization: Bearer sk-");
    if(!tmp)key_find("Authorization: sk-");
    if(!tmp)key_find("x-api-key: sk-");
    user_* p=getuser_by_id(tmp);
    if(p==0||memcmp(tmp+8,p->gptapikey,19))return ERROR(H400,"Invalid API key.");
    cJSON* req=cJSON_Parse(a->get+a->n),*config=file2j(CONFIG);
    // const char*model=
}