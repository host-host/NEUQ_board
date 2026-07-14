#ifndef GPTAPI3_
#define GPTAPI3_

#include "http.h"

typedef struct {
    char a[32];
}content_id;
typedef struct {
    bool publish;
    bool isusing;
    char owner[24];
    int createtime;
    char name[64];
    char other[1024-64];
    char content[0];
}gpt_content;
typedef struct {
    char user[24];
    int n;
    content_id content[0];
}gpt_userhistory;

void gpt_chat(http_para *a);
void gptapis2(http_para *a);
void gpt_idname(http_para *a);
void gpt_changename(http_para *a);
void gpt_idcontent(http_para *a);
void gpt_getuserhistory(http_para *a);
void gpt_deletehistory(http_para *a);
void gpt_share(http_para *a);

#endif
