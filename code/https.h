#ifndef HTTPS_
#define HTTPS_
#include"myio.h"
#ifdef __cplusplus
extern "C"{
#endif

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

struct https{
    int plen;
    void* p;
    SSL_CTX* ctx;
};
typedef struct{
    int cl;
    https* f;
    SSL* ssl;
    char* get;
    int n,m,ip;
}https_para;
typedef void(*https_work)(https_para*);
INIT void https_INIT(){
    SSL_load_error_strings();
    OpenSSL_add_ssl_algorithms();// EVP_cleanup();
    signal(SIGPIPE,SIG_IGN);
}
int https_init(struct https *a,const char* FILE_fullchain,const char* FILE_private){
    a->plen=0;
    a->p=malloc(16*16);
    a->ctx=SSL_CTX_new(SSLv23_server_method());// SSL_CTX_free(ctx);
    if(SSL_CTX_use_certificate_chain_file(a->ctx,FILE_fullchain)<=0)return 158;
    if(SSL_CTX_use_PrivateKey_file(a->ctx,FILE_private,SSL_FILETYPE_PEM)<=0)return 159;
    if(!SSL_CTX_check_private_key(a->ctx))return 160;
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
    struct timeval timehttps={7,0};
    setsockopt(a->cl,SOL_SOCKET,SO_RCVTIMEO,(char*)&timehttps,sizeof(struct timeval));
    SSL_set_mode(a->ssl=SSL_new(a->f->ctx),SSL_MODE_AUTO_RETRY);
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
        char* t2=strstr(a->get+max(0,n-8),"\r\n\r\n");
        a->get[n+=m]=bj=0;
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
        https_para *tmp=(https_para*)malloc(sizeof(https_para));
        memset(tmp,0,sizeof(https_para));
        tmp->cl=client;
        tmp->f=a;
        tmp->ip=addr.sin_addr.s_addr;
        pthread_create(&thread_id,0,(void*(*)(void*))https_w,(void*)tmp);
        pthread_detach(thread_id);
    }
    close(sock);
    SSL_CTX_free(a->ctx);
    EVP_cleanup();
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