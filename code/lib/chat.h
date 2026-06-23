#ifndef CHAT_
#define CHAT_
#include"http.h"

#ifdef __cplusplus
extern "C"{
#endif

typedef struct{
    char prev[16];
    char next[16];
    int time;
    int deep;
    int other; // 保留为未来功能
    char name[48];
    char content[];
}chat;

void chat_init();
void chat_list(http_para* a);
void chat_content(http_para* a);
void chat_send(http_para* b);

#ifdef __cplusplus
}
#endif

#endif
