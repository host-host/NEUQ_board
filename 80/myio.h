const char Head1[]="HTTP/1.1 200 OK\r\ncache-control: max-age=3600, public\r\nContent-Length:";
const char Headhtml[]="HTTP/1.1 200 OK\r\ncache-control: max-age=3600, public\r\n"
                      "Content-Type: text/html\r\nContent-Length:";
const char Headjson[]="HTTP/1.1 200 OK\r\ncache-control: max-age=3600, public\r\n"
                      "Content-Type: application/json\r\nContent-Length:";
const char Headjs[]=  "HTTP/1.1 200 OK\r\ncache-control: max-age=3600, public\r\n"
                      "Content-Type: application/javascript\r\nContent-Length:";
const char Headcss[]= "HTTP/1.1 200 OK\r\ncache-control: max-age=3600, public\r\n"
                      "Content-Type: text/css\r\nContent-Length:";
const char Headico[]= "HTTP/1.1 200 OK\r\ncache-control: max-age=3600, public\r\n"
                      "Content-Type: image/x-icon\r\nContent-Length:";
const char Headwebp[]="HTTP/1.1 200 OK\r\ncache-control: max-age=3600, public\r\n"
                      "Content-Type: image/webp\r\nContent-Length:";
const char Headtxt[]= "HTTP/1.1 200 OK\r\ncache-control: max-age=3600, public\r\n"
                      "Content-Type: text/plain\r\nContent-Length:";
const char Head2[]="HTTP/1.1 200 OK\r\ncache-control: max-age=0, public\r\nContent-Length:";
const char Head4[]="HTTP/1.1 200 OK\r\ncache-control: max-age=0, public\r\nContent-Length:47\r\nSet-Cookie: id=; Expires=Thu, 01 Jan 1970 00:00:00 GMT; Path=/;\r\n\r\n<script>window.location.href='/board';</script>";
const char Head404[]="HTTP/1.1 404 Not Found\r\n\r\n";
typedef void (*fun)(int cl,const char* re,const char* con,int n,const char* id);
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
void mysend(int cl,const char*a,int n=0){
    if(n==0)n=strlen(a);
    char* content=(char*)malloc(n+300);//head不会超过300
    int m=sprintf(content,"%s%d\r\n\r\n",Head2,n);
    memcpy(content+m,a,n);
    write(cl,content,m+n);
    free(content);
}
int sendfile__(int cl, const char* a){
    FILE* file=fopen(a,"rb");
    if(!file)return 0;
    fseek(file,0,SEEK_END);
    int fileSize=ftell(file);
    fseek(file,0,SEEK_SET);
    const char* head=Head1;
    if(strstr(a,".html"))head=Headhtml;
    else if(strstr(a,".json"))head=Headjson;
    else if(strstr(a,".js"))head=Headjs;
    else if(strstr(a,".css"))head=Headcss;
    else if(strstr(a,".ico"))head=Headico;
    else if(strstr(a,".webp"))head=Headwebp;
    else if(strstr(a,".txt"))head=Headtxt;
    char* content=(char*)malloc(fileSize+300);
    sprintf(content,"%s%d\r\n\r\n",head,fileSize);
    int headerLength=strlen(content);
    size_t bytesRead=fread(content+headerLength,1,fileSize,file);
    if (bytesRead!=fileSize) {
        free(content);
        fclose(file);
        return 0;
    }
    write(cl,content,headerLength+fileSize);
    free(content);
    fclose(file);
    return 1;
}
void sendfile(int cl,const char* a) {
    if(a[0]){
        std::string b="/80/html"+(std::string)a;
        if(a[strlen(a)-1]=='/')b+="index.html";
        else b+=".html";
        if(sendfile__(cl,b.c_str()))return;
        b="/80"+(std::string)a;
        if(sendfile__(cl,b.c_str()))return;
    }
    write(cl,Head404,strlen(Head404));
}