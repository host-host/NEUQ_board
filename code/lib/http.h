#ifndef HTTP_H
#define HTTP_H

#include<openssl/ssl.h>
#include<signal.h>
#ifdef __cplusplus
extern "C"{
#endif

#define Hok   "HTTP/1.1 200 OK\r\n"
#define H404  "HTTP/1.1 404 Not Found\r\n"
#define H500  "HTTP/1.1 500 Internal Server Error\r\n"

#define Hc0   "Cache-Control: no-cache\r\n"
#define Hc1h  "Cache-Control: max-age=3600, public\r\n"
#define Hc7d  "Cache-Control: max-age=604800, public\r\n"

#define Hhtml "Content-Type: text/html\r\n"
#define Hjson "Content-Type: application/json\r\n"
#define Hjs   "Content-Type: application/javascript\r\n"
#define Hcss  "Content-Type: text/css\r\n"
#define Hico  "Content-Type: image/x-icon\r\n"
#define Hwebp "Content-Type: image/webp\r\n"
#define Htxt  "Content-Type: text/plain\r\n"
#define Hmp4  "Content-Type: video/mp4\r\n"

#define Hgzip "Content-Encoding: gzip\r\n"
#define Hbr   "Content-Encoding: br\r\n"
#define Hzstd "Content-Encoding: zstd\r\n"

#define Hdown "Content-Disposition: form-data\r\n"

struct http{
    int plen;
    void* p;
};
typedef struct{
    int cl;
    struct http* f;
    char* get;
    int n,m,ip,port;
}http_para;
typedef void(*http_work)(http_para*);
__attribute((constructor)) void http_INIT(){
    signal(SIGPIPE,SIG_IGN);
    SSL_load_error_strings();
    OpenSSL_add_ssl_algorithms();// EVP_cleanup();
}
void http_init(struct http *a);
void http_add(struct http *a,const char*name,http_work fun);
int http_start(struct http *a,int port);
void http_send(http_para *a,const char* head,const char* content,int n);

struct https{
    int plen,ctxlen;
    void* p;//char*,fun
    void* ctx;//char*,SSL_CTX*
};
typedef struct{
    int cl;
    https* f;
    SSL* ssl;
    char* get;
    int n,m,ip,port;
}https_para;
typedef void(*https_work)(https_para*);
void https_init(struct https *a);
int https_add_web(struct https *a,const char* servername,const char* FILE_fullchain,const char* FILE_private);
void https_add(struct https *a,const char*name,https_work fun);
int https_start(struct https *a,int port);
void https_send(https_para *a,const char* head,const char* content,int n);

#ifdef __cplusplus
}
#endif
#endif