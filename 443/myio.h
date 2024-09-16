#include<openssl/ssl.h>
#include<openssl/err.h>
#include<signal.h>
#define ll long long
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
#define Hgzip "Content-Encoding: gzip\r\n"
#define Hbr   "Content-Encoding: br\r\n"
#define Hzstd "Content-Encoding: zstd\r\n"
#define Hdown "Content-Disposition: form-data\r\n"
int min(int x,int y){
    return x<y?x:y;
}
int max(int x,int y){
    return x>y?x:y;
}
int ncmp(const char* a,const char* b,int n){
    for(int i=0;i<n;i++)if(a[i]!=b[i])return a[i]-b[i];
    return 0;
}
ll readint(const char* a){
    ll x=0;
    while(*a&&(*a<'0'||'9'<*a))a++;
    while('0'<=*a&&*a<='9')x=x*10+(*a++)-'0';
    return x;
}
void JSON(const char* p,char* a){
    int i=0,bj=0;
    if(p[0]=='\"')p++;
    a[i++]='\"';
    while(*p)if(*p=='\\'){
            char nex=*(p+1);
            if(nex=='\"'||nex=='\\'||nex=='/')a[i++]=*p++;
            else a[i++]='\\';
            a[i++]=*p++;
        }else{
            if(*p=='\"')
                if(*(p+1))a[i++]='\\';
                else bj=1;
            a[i++]=*p++;
        }
    if(bj==0)a[i++]='\"';
    a[i]=0;
}
struct myst{
    int cl,ip;
    SSL_CTX* ctx;
    void (*work)(SSL*,char*,int,int,int);
};
void* mywork(myst* tmp){
    int cl=tmp->cl,n=0,bj=0;
    timeval timehttps={7,0};
    setsockopt(cl,SOL_SOCKET,SO_RCVTIMEO,(char*)&timehttps,sizeof(timeval));
    SSL* ssl=SSL_new(tmp->ctx);
    SSL_set_mode(ssl,SSL_MODE_AUTO_RETRY);
    char* get=(char*)malloc(102400),*t2=0;
    if(SSL_set_fd(ssl,cl)<=0||SSL_accept(ssl)<=0)goto https;
    while(1){
        int m=SSL_read(ssl,get+n,100000-n);
        if(m<=0){
            if(SSL_get_error(ssl,m)==SSL_ERROR_WANT_READ){
                if(++bj>6)break;
                usleep((1<<bj)*10000);
                continue;
            }
            break;
        }
        get[n+=m]=bj=0;
        if((t2=strstr(get,"\r\n\r\n")))break;
    }
    if(t2){
        int N=t2-get+4,M=0;
        char *t3=strstr(get,"\r\nContent-Length:");
        if(t3)M=readint(t3);
        if(N+M<100000)while(n<N+M){
            int m=SSL_read(ssl,get+n,N+M-n);
            if(m<=0){
                if(SSL_get_error(ssl,m)==SSL_ERROR_WANT_READ){
                    if(++bj>6)break;
                    usleep((1<<bj)*10000);
                    continue;
                }
                break;
            }
            get[n+=m]=bj=0;
        }
        if(n==N+M)tmp->work(ssl,get,N,M,tmp->ip);
    }
    https://free.neuqboard.cn
    free(get);
    SSL_free(ssl);
    close(cl);
    free(tmp);
    return 0;
}
void mystart(int port,void (*work)(SSL*,char*,int,int,int)){
    sockaddr_in addr;
    SSL_load_error_strings();
    OpenSSL_add_ssl_algorithms();
    SSL_CTX* ctx=SSL_CTX_new(SSLv23_server_method());
    if(SSL_CTX_use_certificate_chain_file(ctx,"/443/pri/fullchain.crt")<=0)exit(158);
    if(SSL_CTX_use_PrivateKey_file(ctx,"/443/pri/private.pem",SSL_FILETYPE_PEM)<=0)exit(159);
    if(!SSL_CTX_check_private_key(ctx))exit(160);
    int sock=socket(AF_INET,SOCK_STREAM,0);
    if(sock<0)exit(161);
    addr.sin_family=AF_INET;
    addr.sin_port=htons(port);
    addr.sin_addr.s_addr=htonl(INADDR_ANY);
    if(bind(sock,(sockaddr*)&addr,sizeof(addr))<0)exit(162);
    if(listen(sock,1)<0)exit(163);
    signal(SIGPIPE,SIG_IGN);
	pthread_t thread_id;
    while(1) {
        sockaddr_in addr;
        uint len=sizeof(addr);
        int client=accept(sock,(sockaddr*)&addr,&len);
        if(client<0)continue;
        myst* tmp=(myst*)malloc(sizeof(myst));
        tmp->cl=client;
        tmp->ctx=ctx;
        tmp->work=work;
        tmp->ip=addr.sin_addr.s_addr;
        pthread_create(&thread_id,0,(void*(*)(void*))mywork,(void*)tmp);
        pthread_detach(thread_id);
    }
    close(sock);
    SSL_CTX_free(ctx);
    EVP_cleanup();
}
void mysend(SSL* ssl,const char* head,const char* a,int n){
    if(n==0)n=strlen(a);
    char* content=(char*)malloc(n+strlen(head)+35);
    int m=sprintf(content,"%sContent-Length: %d\r\n\r\n",head,n);
    memcpy(content+m,a,n);
    if(!SSL_get_shutdown(ssl))SSL_write(ssl,content,m+n);
    free(content);
}