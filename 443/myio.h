#include<openssl/ssl.h>
#include<openssl/err.h>
#include<signal.h>
#define ll long long
const char  Hok[]=   "HTTP/1.1 200 OK\r\n",
            Hc0[]=   "Cache-Control: no-cache\r\n",
            Hc3600[]="Cache-Control: max-age=3600, public\r\n",
            Hc7d[]=  "Cache-Control: max-age=604800, public\r\n",
            Hhtml[]= "Content-Type: text/html\r\n",
            Hjson[]= "Content-Type: application/json\r\n",
            Hjs[]=   "Content-Type: application/javascript\r\n",
            Hcss[]=  "Content-Type: text/css\r\n",
            Hico[]=  "Content-Type: image/x-icon\r\n",
            Hwebp[]= "Content-Type: image/webp\r\n",
            Htxt[]=  "Content-Type: text/plain\r\n",
            Hgzip[]= "Content-Encoding: gzip\r\n",
            Hbr[]=   "Content-Encoding: br\r\n",
            Hzstd[]= "Content-Encoding: zstd\r\n",
            Hdown[]= "Content-Disposition: form-data\r\n",
            // Hoptin[]="Access-Control-Allow-Origin: https://chat.neuqboard.cn\r\n"
            //          "Access-Control-Allow-Credentials: true\r\n",
            Hconl[]= "Content-Length: ";
// const char Ctoboard[]="\r\n<script>window.location.href='https://chat.neuqboard.cn';</script>";
int min(int x,int y){
    return x<y?x:y;
}
int max(int x,int y){
    return x>y?x:y;
}
int ncmp(const char* a,const char* b,int n){
    for(int i=0;i<n;i++)if(a[i]!=b[i])return 0;
    return 1;
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
    int cl;
    SSL_CTX* ctx;
    void (*work)(SSL*,char*,int);
};
void* mywork(void* tmp){
    int cl=((myst*)tmp)->cl,n=0,bj=0;
    struct timeval timehttps={5,0};
    setsockopt(cl,SOL_SOCKET,SO_RCVTIMEO,(char*)&timehttps,sizeof(struct timeval));
    SSL* ssl=SSL_new(((myst*)tmp)->ctx);
    char* get=(char*)malloc(10240);
    if(SSL_set_fd(ssl,cl)<=0||SSL_accept(ssl)<=0)goto https;
    while(1){
        int m=SSL_read(ssl,get+n,10000-n);
        if(m<=0){
            if(SSL_get_error(ssl,m)==SSL_ERROR_WANT_READ){
                if(++bj>12)break;
                usleep((1<<bj)*2000);
                continue;
            }
            break;
        }
        get[n+=m]=bj=0;
        if(strstr(get,"\r\n\r\n"))break;
    }
    if(n)((myst*)tmp)->work(ssl,get,n);
    https://free.neuqboard.cn
    free(get);
    SSL_free(ssl);
    close(cl);
    free(tmp);
    return 0;
}
void mystart(int port,void (*work)(SSL*,char*,int)){
    struct sockaddr_in addr;
    SSL_load_error_strings();
    OpenSSL_add_ssl_algorithms();
    SSL_CTX* ctx=SSL_CTX_new(SSLv23_server_method());
    if (!ctx)exit(157);
    if(SSL_CTX_use_certificate_chain_file(ctx,"/443/pri/fullchain.crt")<=0)exit(158);
    if(SSL_CTX_use_PrivateKey_file(ctx,"/443/pri/private.pem",SSL_FILETYPE_PEM)<=0)exit(159);
    if(!SSL_CTX_check_private_key(ctx))exit(160);
    int sock=socket(AF_INET, SOCK_STREAM, 0);
    if(sock<0)exit(161);
    addr.sin_family=AF_INET;
    addr.sin_port=htons(port);
    addr.sin_addr.s_addr=htonl(INADDR_ANY);
    if(bind(sock,(struct sockaddr*)&addr,sizeof(addr))<0)exit(162);
    if(listen(sock,1)<0)exit(163);
    signal(SIGPIPE,SIG_IGN);
	pthread_t thread_id;
    while (1) {
        struct sockaddr_in addr;
        uint len=sizeof(addr);
        int client=accept(sock, (struct sockaddr*)&addr, &len);
        if(client<0)continue;
        myst* tmp=(myst*)malloc(sizeof(myst));
        tmp->cl=client;
        tmp->ctx=ctx;
        tmp->work=work;
        pthread_create(&thread_id,0,mywork,(void*)tmp);
        pthread_detach(thread_id);
    }
    close(sock);
    SSL_CTX_free(ctx);
    EVP_cleanup();
}
void mysslwrite(SSL* ssl,const char*a,int n){
    if(SSL_get_shutdown(ssl))return;
    SSL_write(ssl,a,n);
}
void mysend(SSL* ssl,const char*a,int n=0){
    if(n==0)n=strlen(a);
    char* content=(char*)malloc(n+300);
    int m=sprintf(content,"%s%s%s%d\r\n\r\n",Hok,Hc0,Hconl,n);
    memcpy(content+m,a,n);
    mysslwrite(ssl,content,m+n);
    free(content);
}