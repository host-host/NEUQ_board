#include<openssl/ssl.h>
#include<openssl/err.h>
#include<signal.h>
#define ll long long
const char  Hok[]=   "HTTP/1.1 200 OK\r\n",
            Hc0[]=   "cache-control: no-cache\r\n",
            Hc3600[]="cache-control: max-age=3600, public\r\n",
            Hhtml[]= "Content-Type: text/html\r\n",
            Hjson[]= "Content-Type: application/json\r\n",
            Hjs[]=   "Content-Type: application/javascript\r\n",
            Hcss[]=  "Content-Type: text/css\r\n",
            Hico[]=  "Content-Type: image/x-icon\r\n",
            Hwebp[]= "Content-Type: image/webp\r\n",
            Htxt[]=  "Content-Type: text/plain\r\n",
            Hoptin[]="Access-Control-Allow-Origin: https://free.neuqboard.cn\r\n"
                     "Access-Control-Allow-Credentials: true\r\n";
const char Ctoboard[]="\r\n<script>window.location.href='https://free.neuqboard.cn/board';</script>";
std::map<string,int>logmap;
volatile int sumthread=0;
int min(int x,int y){
    return x<y?x:y;
}
int max(int x,int y){
    return x>y?x:y;
}
ll readint(const char* a){
    ll x=0;
    while(*a&&(*a<'0'||'9'<*a))a++;
    while('0'<=*a&&*a<='9')x=x*10+(*a++)-'0';
    return x;
}
void JSON(char* p,char* a){
    int i=0,bj=0;
    if(p[0]=='\"')p++;
    a[i++]='\"';
    while(*p){
        if(*p=='\\'){
            char nex=*(p+1);
            if(nex=='\"'||nex=='\\'||nex=='/')a[i++]=*p++;
            else a[i++]='\\';
            a[i++]=*p++;
            continue;
        }
        if(*p=='\"'){
            if(*(p+1))a[i++]='\\';
            else bj=1;
        }
        a[i++]=*p++;
    }
    if(bj==0)a[i++]='\"';
    a[i]=0;
}
string printlog(){
    string a="ALL threads: "+std::to_string(sumthread)+"\n";
    for(auto i:logmap)a+=i.first+" "+std::to_string(i.second)+"\n";
    return a;
}
void signal_handler(int sig){
	logmap["SIGPIPE"+std::to_string(sig)]++;
    signal(SIGPIPE,signal_handler);
}
int mycreatsock(int port,SSL_CTX** __ctx){
    struct sockaddr_in addr;
    SSL_load_error_strings();
    OpenSSL_add_ssl_algorithms();
    SSL_CTX* ctx = SSL_CTX_new(SSLv23_server_method());
    if (!ctx)exit(157);
    if(SSL_CTX_use_certificate_chain_file(ctx, "/443/pri/free.neuqboard.cn.crt")<=0)exit(158);
    if(SSL_CTX_use_PrivateKey_file(ctx, "/443/pri/free.neuqboard.cn.key",SSL_FILETYPE_PEM)<=0)exit(159);
    if (!SSL_CTX_check_private_key(ctx))exit(160);
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock<0)exit(161);
    addr.sin_family=AF_INET;
    addr.sin_port=htons(port);
    addr.sin_addr.s_addr=htonl(INADDR_ANY);
    if(bind(sock,(struct sockaddr*)&addr,sizeof(addr))<0)exit(162);
    if(listen(sock,1)<0)exit(163);
    signal(SIGPIPE,signal_handler);
    *__ctx=ctx;
    return sock;
}
void mystart(int sock,void* (*work)(void*),char* mylog){
    SSL_CTX *ctx;
    sock=mycreatsock(sock,&ctx);
	pthread_t thread_id;
    while (1) {
        struct sockaddr_in addr;
        uint len=sizeof(addr);
        int client=accept(sock, (struct sockaddr*)&addr, &len);
        if(client<0)continue;
        struct timeval timehttps = {5,0};
        setsockopt(client,SOL_SOCKET,SO_RCVTIMEO,(char*)&timehttps,sizeof(struct timeval));
        SSL* ssl=SSL_new(ctx);
        mylog[0]=1;
        if(SSL_set_fd(ssl,client)<=0||SSL_accept(ssl)<=0){
            mylog[0]=0;
            SSL_free(ssl);
            close(client);
            continue;
        }
        mylog[0]=0;
        sumthread++;
        pthread_create(&thread_id,0,work,(void*)ssl);
        pthread_detach(thread_id);//need free : ssl,client,sumthread
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
    char* content=(char*)malloc(n+300);//head不会超过300
    int m=sprintf(content,"%s%s%s\r\n",Hok,Hc0,Hoptin);
    memcpy(content+m,a,n);
    mysslwrite(ssl,content,m+n);
    free(content);
}
int mysendfile(SSL* ssl, const char* a){
    FILE* file=fopen(a,"rb");
    if(!file)return 0;
    fseek(file,0,SEEK_END);
    int fileSize=ftell(file);
    fseek(file,0,SEEK_SET);
    const char* head=Htxt;
    if(strstr(a,".html"))head=Hhtml;
    if(strstr(a,".js"))head=Hjs;
    if(strstr(a,".json"))head=Hjson;
    if(strstr(a,".css"))head=Hcss;
    if(strstr(a,".ico"))head=Hico;
    if(strstr(a,".webp"))head=Hwebp;
    char* content=(char*)malloc(fileSize+300);
    sprintf(content,"%s%s%s\r\n",Hok,Hc3600,head);
    int headerLength=strlen(content);
    size_t bytesRead=fread(content+headerLength,1,fileSize,file);
    if(bytesRead!=fileSize) {
        free(content);
        fclose(file);
        return 0;
    }
    mysslwrite(ssl,content,headerLength+fileSize);
    free(content);
    fclose(file);
    return 1;
}