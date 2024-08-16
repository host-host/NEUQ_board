#define ll long long
const char  Hok[]=   "HTTP/1.1 200 OK\r\n",
            Hc0[]=   "cache-control: max-age=0, public\r\n",
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
int mycreatsock(int port,SSL_CTX** __ctx){
    struct sockaddr_in addr;
    SSL_load_error_strings();
    OpenSSL_add_ssl_algorithms();
    SSL_CTX* ctx = SSL_CTX_new(SSLv23_server_method());
    if (!ctx) {
        perror("Unable to create SSL context");
        ERR_print_errors_fp(stderr);
        exit(EXIT_FAILURE);
    }
    if(SSL_CTX_use_certificate_chain_file(ctx, "/443/pri/free.neuqboard.cn.crt")<=0){
        printf("ERROR crt\n");
    }
    if(SSL_CTX_use_PrivateKey_file(ctx, "/443/pri/free.neuqboard.cn.key", SSL_FILETYPE_PEM)<=0){
        printf("ERROR KEY\n");
    }
    if (!SSL_CTX_check_private_key(ctx)) {
        printf("Private key does not match the public certificate\n");
    }
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        perror("Unable to create socket");
        exit(EXIT_FAILURE);
    }
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    if (bind(sock, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("Unable to bind");
        exit(EXIT_FAILURE);
    }
    if (listen(sock, 1) < 0) {
        perror("Unable to listen");
        exit(EXIT_FAILURE);
    }
    *__ctx=ctx;
    return sock;
}
void mysslwrite(SSL* ssl,const char* a,int n){
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
    if (bytesRead!=fileSize) {
        free(content);
        fclose(file);
        return 0;
    }
    mysslwrite(ssl,content,headerLength+fileSize);
    free(content);
    fclose(file);
    return 1;
}