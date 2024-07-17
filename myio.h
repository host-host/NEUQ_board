/********************************************
 * 版权声明
 * 
 * 本程序由 刘峻畅 编写。
 * 版权所有：Copyright (c) 2024 刘峻畅。
 * 
 * 本程序是开源的，你可以自由地复制、分发和修改它，
 * 但请保留此版权声明。
 * 
 * 注意：使用本程序时，请遵守相关法律法规。 
 ********************************************/
const char Head1[]="HTTP/1.1 200 OK\r\ncache-control: max-age=0, public\r\nContent-Length:";
const char Head2[]="HTTP/1.1 200 OK\r\nAccess-Control-Allow-Origin: *\r\nAccess-Control-Allow-Methods: POST, GET, OPTIONS\r\nAccess-Control-Allow-Credentials: false";
const char Head4[]="HTTP/1.1 200 OK\r\ncache-control: max-age=0, public\r\nContent-Length:75\r\nSet-Cookie: id=; Expires=Thu, 01 Jan 1970 00:00:00 GMT; Path=/; Domain=121.36.103.216\r\n\r\n<script>window.location.href = 'http://121.36.103.216/board.html';</script>";
typedef void (*fun)(int cl,const char* re,const char* con,int n,const char* id);
#define ll long long
#define ul unsigned long long
std::map<std::string,int>users;
char (*user)[128],*data,*cont;
int fdata,fcont;
ll ldata,lcont;
struct point{
	const char* content,*mat;
	int n;
};
std::vector<point>e;
ll readint(const char*a){
    ll x=0;
    while(*a&&(*a<'0'||'9'<*a))a++;
    while('0'<=*a&&*a<='9')x=x*10+(*a++)-'0';
    return x;
}
ul hash(const char*a){
    ul x=0;
    while(*a)x=x*129+*a;
    return x;
}
int min(int x,int y){
    return x<y?x:y;
}
int max(int x,int y){
    return x>y?x:y;
}
void mysend(int cl,const char*a,int n=0){
    if(n==0)n=strlen(a);
    char* content=(char*)malloc(n+300);//head不会超过300
    sprintf(content,"%s%d\r\n\r\n",Head1,n);
    int m=strlen(content);
    memcpy(content+m,a,n);
    write(cl,content,m+n);
    free(content);
}