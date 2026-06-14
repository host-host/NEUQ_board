#ifndef HTTP_H
#define HTTP_H

#include<signal.h>
#include<netinet/in.h>
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
    void* p;//char*,fun
    volatile int stop;//置1后http_start不再接受新连接
    volatile int active;//当前活跃连接数
};
typedef struct{
    int cl;
    struct http* f;
    char* get;
    int n,m,ip,port;//get~get+n是消息头，get+n~get+n+m是消息内容
}http_para;
typedef void(*http_work)(http_para*);
void http_INIT();
void http_init(struct http *a);
void http_add(struct http *a,const char*name,http_work fun);
int http_start(struct http *a,unsigned int addr,int port);
void http_stop(struct http *a);
void http_send(http_para *a,const char* head,const char* content,int n);

#ifdef __cplusplus
}
#endif
#endif