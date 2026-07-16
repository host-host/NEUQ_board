#include"http.h"
#include<sys/socket.h>
#include<sys/time.h>
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
	while((*a<'0'||*a>'9')&&*a)a++;
    while(*a>='0'&&*a<='9')x=x*10+*a++-'0';
	return x;
}

__attribute((constructor)) void http_INIT(){
    signal(SIGPIPE,SIG_IGN);
}
void http_init(struct http *a){
    a->plen=0;
    a->p=malloc(16*16);
    a->stop=0;
    a->active=0;
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
    a->get=(char*)malloc(5*1024*1000);
    while(1){
        int m=read(a->cl,a->get+n,5000000-n);
        if(m<=0)break;
        a->get[n+m]=0;
        char* t2=strstr(a->get+max(0,n-8),"\r\n\r\n");
        n+=m;
        if(t2){
            int N=t2-a->get+4,M=0;
            char *t3=strstr(a->get,"\r\nContent-Length:");
            if(t3)M=readll(t3+15);
            if(N+M<5000000)while(n<N+M){
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
    __sync_fetch_and_sub(&a->f->active,1);
    free(a);
    return 0;
}
int http_start(struct http *a,in_addr_t in_addr,int port){
    int sock=socket(AF_INET,SOCK_STREAM,0);
    if(sock<0)return 161;
    int opt=1;
    setsockopt(sock,SOL_SOCKET,SO_REUSEADDR,&opt,sizeof(opt));
    struct sockaddr_in addr;
    addr.sin_family=AF_INET;
    addr.sin_port=htons(port);
    addr.sin_addr.s_addr=htonl(in_addr);
    if(bind(sock,(struct sockaddr*)&addr,sizeof(addr))<0)return 162;
    if(listen(sock,5)<0)return 163;
    struct timeval acto={1,0};
    setsockopt(sock,SOL_SOCKET,SO_RCVTIMEO,(char*)&acto,sizeof(acto));
	pthread_t thread_id;
    while(!a->stop) {
        struct sockaddr_in addr;
        unsigned int len=sizeof(addr);
        int client=accept(sock,(struct sockaddr*)&addr,&len);
        if(client<0)continue;
        if(a->stop){close(client);break;}
        __sync_fetch_and_add(&a->active,1);
        http_para *tmp=(http_para*)malloc(sizeof(http_para));
        memset(tmp,0,sizeof(http_para));
        tmp->cl=client;
        tmp->f=a;
        tmp->ip=addr.sin_addr.s_addr;
        tmp->port=addr.sin_port;
        if(pthread_create(&thread_id,0,(void*(*)(void*))http_w,(void*)tmp)){
            __sync_fetch_and_sub(&a->active,1);
            close(client);
            free(tmp);
            continue;
        }
        pthread_detach(thread_id);
    }
    close(sock);
    while(__sync_fetch_and_add(&a->active,0)>0)usleep(100000);
    return 0;
}
void http_stop(struct http *a){
    a->stop=1;
}
void http_send(http_para *a,const char* head,const char* content,int n){
    if(n==0)n=strlen(content);
    char* con=(char*)malloc(n+strlen(head)+35);
    int m=sprintf(con,"%sContent-Length: %d\r\n\r\n",head,n);
    memcpy(con+m,content,n);
    if(write(a->cl,con,m+n));
    free(con);
}