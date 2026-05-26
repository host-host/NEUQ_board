#ifndef GPTAPI2_
#define GPTAPI2_
#include"http.h"
#ifdef __cplusplus
extern "C"{
#endif

typedef struct{
    char a[32];
}content_id;
typedef struct{//一个对话
    bool publish;
    bool isusing;
    char owner[24];
    int createtime;
    char name[64];
    char other[1024-64];
    char content[0];//对话内容的历史content
}gpt_content;
typedef struct{
    char user[24];
    int n;//有几条历史记录
    content_id content[0];
}gpt_userhistory;


void gptapi2_init();
void gpt_gotid(http_para *a);
void gpt_askid(http_para *a);
void gpt_idname(http_para *a);
void gpt_changename(http_para *a);
void gpt_idcontent(http_para *a);
void gpt_getuserhistory(http_para *a);
void gpt_deletehistory(http_para *a);
void gpt_share(http_para *a);

#ifdef __cplusplus
}
#endif
#endif