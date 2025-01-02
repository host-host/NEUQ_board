#ifndef HTTP_
#define HTTP_
#ifdef __cplusplus
extern "C"{
#endif

#include<signal.h>
#include<sys/socket.h>
#include<netinet/in.h>
#include<stdlib.h>
#include<string.h>
#include<pthread.h>
#include<stdio.h>
#include<unistd.h>
#include"myio.h"

#define Hok   "HTTP/1.1 200 OK\r\n"
#define H404  "HTTP/1.1 404 Not Found\r\n"

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
};
typedef struct{
    int cl;
    struct http* f;
    char* get;
    int n,m,ip,port;
}http_para;
typedef void(*http_work)(http_para*);
INIT void http_INIT(){
    signal(SIGPIPE,SIG_IGN);
}
void http_init(struct http *a){
    a->plen=0;
    a->p=malloc(16*16);
}
void http_add(struct http *a,const char*name,http_work fun){
    if((a->plen&(-a->plen))==a->plen&&a->plen>=16){
        void* tmp=malloc(a->plen*2*16);
        memcpy(tmp,a->p,a->plen*16);
        free(a->p);
        a->p=tmp;
    }
    *(ll*)((ll)a->p+a->plen*16)=(ll)name;
    *(ll*)((ll)a->p+a->plen*16+8)=(ll)fun;
    a->plen++;
}
void* http_w(http_para* a){
    int n=0;
    struct timeval timehttps={10,0};
    setsockopt(a->cl,SOL_SOCKET,SO_RCVTIMEO,(char*)&timehttps,sizeof(struct timeval));
    a->get=(char*)malloc(102400);
    while(1){
        int m=read(a->cl,a->get+n,100000-n);
        if(m<=0)break;
        a->get[n+m]=0;
        char* t2=strstr(a->get+max(0,n-8),"\r\n\r\n");
        n+=m;
        if(t2){
            int N=t2-a->get+4,M=0;
            char *t3=strstr(a->get,"\r\nContent-Length:");
            if(t3)M=readll(t3+15);
            if(N+M<100000)while(n<N+M){
                    int m=read(a->cl,a->get+n,N+M-n);
                // LOG("%s %d",a->get+N,n-N);
                    if(m<=0)break;
                    a->get[n+=m]=0;
                }
            if(n==N+M){
                a->n=N;
                a->m=M;
                for(int i=0;i<a->f->plen;i++)if(bncmp(a->get,*(char**)((ll)a->f->p+i*16))==0){
                        (**(http_work*)((ll)a->f->p+i*16+8))(a);
                        break;
                    }
                break;
            }
        }
    }
    free(a->get);
    close(a->cl);
    free(a);
    return 0;
}
int http_start(struct http *a,int port){
    int sock=socket(AF_INET,SOCK_STREAM,0);
    if(sock<0)return 161;
    struct sockaddr_in addr;
    addr.sin_family=AF_INET;
    addr.sin_port=htons(port);
    addr.sin_addr.s_addr=htonl(INADDR_ANY);
    if(bind(sock,(struct sockaddr*)&addr,sizeof(addr))<0)return 162;
    if(listen(sock,1)<0)return 163;
	pthread_t thread_id;
    while(1) {
        struct sockaddr_in addr;
        uint len=sizeof(addr);
        int client=accept(sock,(struct sockaddr*)&addr,&len);
        if(client<0)continue;
        http_para *tmp=(http_para*)malloc(sizeof(http_para));
        memset(tmp,0,sizeof(http_para));
        tmp->cl=client;
        tmp->f=a;
        tmp->ip=addr.sin_addr.s_addr;
        tmp->port=addr.sin_port;
        pthread_create(&thread_id,0,(void*(*)(void*))http_w,(void*)tmp);
        pthread_detach(thread_id);
    }
}
void http_send(http_para *a,const char* head,const char* content,int n){
    if(n==0)n=strlen(content);
    char* con=(char*)malloc(n+strlen(head)+35);
    int m=sprintf(con,"%sContent-Length: %d\r\n\r\n",head,n);
    memcpy(con+m,content,n);
    if(write(a->cl,con,m+n));
    free(con);
}

#ifdef __cplusplus
}
#endif
#endif