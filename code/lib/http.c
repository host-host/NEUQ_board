#ifndef HTTPS_
#define HTTPS_

#include<openssl/ssl.h>

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
#include<stdio.h>
#include<stdlib.h>

#define ll long long
static int max(int x,int y){
    return x>y?x:y;
}
static int bncmp(const char* a,const char* b){
    for(int i=0;b[i];i++)if(a[i]!=b[i])return a[i]-b[i];
    return 0;
}
static inline ll readll(const char* a){
    ll x=0;
	while(*a<'0'||*a>'9')a++;
    while(*a>='0'&&*a<='9')x=x*10+*a++-'0';
	return x;
}

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
    a->get=(char*)malloc(2*1024*1024);
    while(1){
        int m=read(a->cl,a->get+n,2048000-n);
        if(m<=0)break;
        a->get[n+m]=0;
        char* t2=strstr(a->get+max(0,n-8),"\r\n\r\n");
        n+=m;
        if(t2){
            int N=t2-a->get+4,M=0;
            char *t3=strstr(a->get,"\r\nContent-Length:");
            if(t3)M=readll(t3+15);
            if(N+M<2048000)while(n<N+M){
                    int m=read(a->cl,a->get+n,N+M-n);
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
    if(a->get)free(a->get);
    if(a->cl)close(a->cl);
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
    if(listen(sock,5)<0)return 163;
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
int https_change_ssl_ctx_by_servername(SSL *ssl,int *ad,void *arg){
    return SSL_TLSEXT_ERR_OK;///////////////////////
	const char *servername=SSL_get_servername(ssl, TLSEXT_NAMETYPE_host_name);
    if(servername==NULL)return SSL_TLSEXT_ERR_ALERT_FATAL;
    struct https *a=(struct https *)arg;
    if(strstr(servername,*(char**)a->ctx))return SSL_TLSEXT_ERR_OK;
    for(int i=1;i<a->ctxlen;i++)if(strstr(servername,*(char**)((ll)a->ctx+i*16))){
            SSL_set_SSL_CTX(ssl,*(SSL_CTX**)((ll)a->ctx+i*16+8));
            return SSL_TLSEXT_ERR_OK;
        }
	return SSL_TLSEXT_ERR_ALERT_FATAL;
}
void https_init(struct https *a){
    a->plen=a->ctxlen=0;
    a->p=malloc(16*16);
    a->ctx=malloc(16*16);
}
int https_add_web(struct https *a,const char* servername,const char* FILE_fullchain,const char* FILE_private){
    if((a->ctxlen&(-a->ctxlen))==a->ctxlen&&a->ctxlen>=16){
        void* tmp=malloc(a->ctxlen*2*16);
        memcpy(tmp,a->ctx,a->ctxlen*16);
        free(a->ctx);
        a->ctx=tmp;
    }
    *(ll*)((ll)a->ctx+a->ctxlen*16)=(ll)servername;
    SSL_CTX* ctx=SSL_CTX_new(TLS_server_method());
    if(SSL_CTX_use_certificate_chain_file(ctx,FILE_fullchain)<=0)return 158;
    if(SSL_CTX_use_PrivateKey_file(ctx,FILE_private,SSL_FILETYPE_PEM)<=0)return 159;
    if(!SSL_CTX_check_private_key(ctx))return 160;
    *(ll*)((ll)a->ctx+a->ctxlen*16+8)=(ll)ctx;
    a->ctxlen++;
    return 0;
}
void https_add(struct https *a,const char*name,https_work fun){
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
void* https_w(https_para* a){
    int n=0,bj=0;
    struct timeval timehttps={10,0};
    setsockopt(a->cl,SOL_SOCKET,SO_RCVTIMEO,(char*)&timehttps,sizeof(struct timeval));
    SSL_set_mode(a->ssl=SSL_new(*(SSL_CTX**)((ll)a->f->ctx+8)),SSL_MODE_AUTO_RETRY);
    a->get=(char*)malloc(102400);
    if(SSL_set_fd(a->ssl,a->cl)<=0||SSL_accept(a->ssl)<=0)goto https;
    while(1){
        int m=SSL_read(a->ssl,a->get+n,100000-n);
        if(m<=0){
            if(SSL_get_error(a->ssl,m)==SSL_ERROR_WANT_READ){
                if(++bj>6)break;
                usleep((1<<bj)*10000);
                continue;
            }
            break;
        }
        a->get[n+m]=bj=0;
        char* t2=strstr(a->get+max(0,n-8),"\r\n\r\n");
        n+=m;
        if(t2){
            int N=t2-a->get+4,M=0;
            char *t3=strstr(a->get,"\r\nContent-Length:");
            if(t3)M=readll(t3+15);
            if(N+M<100000)while(n<N+M){
                    int m=SSL_read(a->ssl,a->get+n,N+M-n);
                    if(m<=0){
                        if(SSL_get_error(a->ssl,m)==SSL_ERROR_WANT_READ){
                            if(++bj>6)break;
                            usleep((1<<bj)*10000);
                            continue;
                        }
                        break;
                    }
                    a->get[n+=m]=bj=0;
                }
            if(n==N+M){
                a->n=N;
                a->m=M;
                for(int i=0;i<a->f->plen;i++)if(bncmp(a->get,*(char**)((ll)a->f->p+i*16))==0){
                        (**(https_work*)((ll)a->f->p+i*16+8))(a);
                        break;
                    }
                break;
            }
        }
    }
    https://free.neuqboard.cn
    free(a->get);
    SSL_free(a->ssl);
    close(a->cl);
    free(a);
    return 0;
}
int https_start(struct https *a,int port){
    SSL_CTX* ctx=*(SSL_CTX**)((ll)a->ctx+8);
    if(a->ctxlen<1)return 1;
    SSL_CTX_set_tlsext_servername_callback(ctx,https_change_ssl_ctx_by_servername);
    SSL_CTX_set_tlsext_servername_arg(ctx,(void*)a);
    int sock=socket(AF_INET,SOCK_STREAM,0);
    if(sock<0)return 161;
    struct sockaddr_in addr;
    addr.sin_family=AF_INET;
    addr.sin_port=htons(port);
    addr.sin_addr.s_addr=htonl(INADDR_ANY);
    if(bind(sock,(struct sockaddr*)&addr,sizeof(addr))<0)return 162;
    if(listen(sock,5)<0)return 163;
	pthread_t thread_id;
    while(1) {
        struct sockaddr_in addr;
        uint len=sizeof(addr);
        int client=accept(sock,(struct sockaddr*)&addr,&len);
        if(client<0)continue;
        https_para *tmp=(https_para*)malloc(sizeof(https_para));
        memset(tmp,0,sizeof(https_para));
        tmp->cl=client;
        tmp->f=a;
        tmp->ip=addr.sin_addr.s_addr;
        tmp->port=addr.sin_port;
        pthread_create(&thread_id,0,(void*(*)(void*))https_w,(void*)tmp);
        pthread_detach(thread_id);
    }
    // close(sock);
    // SSL_CTX_free(ctx);
    // EVP_cleanup();
    return 0;
}
void https_send(https_para *a,const char* head,const char* content,int n){
    if(n==0)n=strlen(content);
    char* con=(char*)malloc(n+strlen(head)+35);
    int m=sprintf(con,"%sContent-Length: %d\r\n\r\n",head,n);
    memcpy(con+m,content,n);
    if(!SSL_get_shutdown(a->ssl))SSL_write(a->ssl,con,m+n);
    free(con);
}

#ifdef __cplusplus
}
#endif
#endif