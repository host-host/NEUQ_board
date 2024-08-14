const char  Hok[]=   "HTTP/1.1 200 OK\r\n",
            Hc0[]=   "cache-control: max-age=0, public\r\n",
            Hc3600[]="cache-control: max-age=3600, public\r\n",
            Hhtml[]= "Content-Type: text/html\r\n",
            Hjson[]= "Content-Type: application/json\r\n",
            Hjs[]=   "Content-Type: application/javascript\r\n",
            Hcss[]=  "Content-Type: text/css\r\n",
            Hico[]=  "Content-Type: image/x-icon\r\n",
            Hwebp[]= "Content-Type: image/webp\r\n",
            Htxt[]=  "Content-Type: text/plain\r\n";
const char Head4[]="HTTP/1.1 200 OK\r\ncache-control: max-age=0, public\r\nSet-Cookie: id=; Expires=Thu, 01 Jan 1970 00:00:00 GMT; Path=/;\r\n\r\n<script>window.location.href='/board';</script>";
const char Head404[]="HTTP/1.1 404 Not Found\r\n\r\n";
typedef void (*fun)(SSL* ssl,const char* re,const char* con,int n,const char* id);
#define ll long long
std::map<std::string,int>users;
char (*user)[128],*data,*cont;
int fdata,fcont,fil;
ll ldata,lcont;
struct point{
	const char *mat;
    fun a;
};
std::vector<point>e;
SSL_CTX *ctx;
int min(int x,int y){
    return x<y?x:y;
}
int max(int x,int y){
    return x>y?x:y;
}
ll readint(const char*a){
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
void mysend(SSL* ssl,const char*a,int n=0){
    if(n==0)n=strlen(a);
    char* content=(char*)malloc(n+300);//head不会超过300
    int m=sprintf(content,"%s%s\r\n",Hok,Hc0);
    memcpy(content+m,a,n);
    SSL_write(ssl,content,m+n);
    free(content);
}
int sendfile__(SSL* ssl, const char* a){
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
    SSL_write(ssl,content,headerLength+fileSize);
    free(content);
    fclose(file);
    return 1;
}
void sendfile(SSL* ssl,const char* a) {
    if(a[0]){
        std::string b="/443/pub/html"+(std::string)a+(a[strlen(a)-1]=='/'?"index.html":".html");
        if(sendfile__(ssl,b.c_str()))return;
        b="/443/pub"+(std::string)a;
        if(sendfile__(ssl,b.c_str()))return;
    }
    SSL_write(ssl,Head404,strlen(Head404));
}