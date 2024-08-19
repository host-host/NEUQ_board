#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<sys/types.h>
#include<sys/socket.h>
#include<sys/mman.h>
#include<netinet/in.h>
#include<unistd.h>
#include<fcntl.h>
#include<errno.h>
#include<pthread.h>
#include<time.h>
#include<map>
#include<vector>
#include<string>
using std::string;
#include"/443/myio.h"
const char Head404[]="HTTP/1.1 404 Not Found\r\n\r\n";
char (*user)[128];
void sendfile(SSL* ssl,const char* a,int notadmin) {
    if(notadmin)logmap[(string)a]++;
    if(a[0]){
        string b="/443/pub/html"+(string)a+(a[strlen(a)-1]=='/'?"index.html":".html");
        if(mysendfile(ssl,b.c_str()))return;
        b="/443/pub"+(string)a;
        if(mysendfile(ssl,b.c_str()))return;
    }
    mysslwrite(ssl,Head404,strlen(Head404));
}
void* work(void* __ssl){
    SSL* ssl=(SSL*)__ssl;
    int cl=SSL_get_fd(ssl),n=0,i,j,notadmin=1;
    char* get=(char*)malloc(10240),*id;
    while(1){
        int m=SSL_read(ssl,get+n,1024-n);//head less than 1024
        if(m<=0)break;
        get[n+=m]=0;
        if(strstr(get,"\r\n\r\n"))break;
    }
    if(n<=0)goto https;
    if((id=strstr(get,"Cookie: id="))){
        int tmp=0;
        for(i=11;i<16;i++)tmp=tmp*10+id[i]-'A';
        if(0<=tmp&&tmp<=10000&&*(ll*)(id+11+2)==*(ll*)(user[tmp]+74)&&user[tmp][127]==1)notadmin=0;
    }
    if(!strstr(get,"free.neuqboard.cn")){
        if(*(int*)get==542393671)sendfile(ssl,"/fix",notadmin);
        else mysslwrite(ssl,Head404,strlen(Head404));
        goto https;
    }
    if(*(int*)get==542393671){
        if(notadmin==0){
            if(strstr(get,"GET /admin")){
                string a=printlog();
                mysend(ssl,a.c_str(),a.length());
                goto https;
            }
            if(strstr(get,"GET /reset")){
                logmap.clear();
            }
        }
        char file[128];
        for(n=1;n<127&&((file[n]=get[n+3])!=46||file[n-1]!=46);n++)
            if((file[n]<46||57<file[n])&&(file[n]<95||122<file[n]))break;
        file[n]=0;
        sendfile(ssl,file+1,notadmin);
    }
    https://free.neuqboard.cn/
    SSL_free(ssl);
    close(cl);
    free(get);
    sumthread--;
    return 0;
}
int main() {
    user=(char(*)[128])mmap(0,0x5AA5D000,PROT_READ|PROT_WRITE,MAP_SHARED,open("/1000/pri/user.txt",O_RDWR|O_CREAT),0);
    char* mylog=(char*)mmap(0,100*1024,PROT_READ|PROT_WRITE,MAP_SHARED,open("/443/pri/log.dat",O_RDWR|O_CREAT),0);
    memset(mylog,0,102400);
    mystart(999,work,mylog);
    return 0;
}