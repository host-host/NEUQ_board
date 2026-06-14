#ifndef HTTP_H
#define HTTP_H

#include<openssl/ssl.h>
#include<signal.h>
#include<netinet/in.h>
#include"http.h"
#ifdef __cplusplus
extern "C"{
#endif

typedef struct {
    int plen,ctxlen;
    void* p;//char*,fun
    void* ctx;//char*,SSL_CTX*
}https;
typedef struct{
    int cl;
    https* f;
    SSL* ssl;
    char* get;
    int n,m,ip,port;
}https_para;
typedef void(*https_work)(https_para*);
void https_INIT();
void https_init(https *a);
int https_add_web(https *a,const char* servername,const char* FILE_fullchain,const char* FILE_private);
void https_add(https *a,const char*name,https_work fun);
int https_start(https *a,int port);
void https_send(https_para *a,const char* head,const char* content,int n);

#ifdef __cplusplus
}
#endif
#endif