const char Head1[]="HTTP/1.1 200 OK\r\ncache-control: max-age=0, public\r\nContent-Length:";
const char Head2[]="HTTP/1.1 200 OK\r\nAccess-Control-Allow-Origin: *\r\nAccess-Control-Allow-Methods: POST, GET, OPTIONS\r\nAccess-Control-Allow-Credentials: false";
const char Head4[]="HTTP/1.1 200 OK\r\ncache-control: max-age=0, public\r\nContent-Length:75\r\nSet-Cookie: id=; Expires=Thu, 01 Jan 1970 00:00:00 GMT; Path=/; Domain=121.36.103.216\r\n\r\n<script>window.location.href = 'http://121.36.103.216/board.html';</script>";
int readint(char*a){
    int x=0;
    while(*a<'0'||'9'<*a)a++;
    while('0'<=*a&&*a<='9')x=x*10+(*a++)-'0';
    return x;
}
int writeint(char* a,int x){
    if(x>9){
        int tmp=writeint(a,x/10);
        a[tmp]=x%10+'0';
        return tmp+1;
    }
    *a=x+'0';
    return 1;
}
int writestr(char*a,char*b){
    int n=0;
    for(;b[n];n++)a[n]=b[n];
    return n;
}
void send(int cl,const char*a,int n=0){
    if(n==0)n=strlen(a);
	char* content=(char*)malloc(n+300);//head不会超过300
    sprintf(content,"%s%d\r\n\r\n",Head1,n);
    int m=strlen(content);
    memcpy(content+m,a,n);
    write(cl,content,m+n);
    free(content);
}