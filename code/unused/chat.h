#ifndef CHAT_
#define CHAT_
#include"http.h"


#ifdef __cplusplus
extern "C"{
#endif

void chat_init();
void getp(http_para*a);
void getcon(http_para*a);
void sendmessage(http_para*b);

#ifdef __cplusplus
}
#endif


#endif