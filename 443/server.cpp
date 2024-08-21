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
#include<string>
using std::string;
#include"/443/myio.h"
const char Head404[]="HTTP/1.1 404 Not Found\r\n\r\n";
char (*user)[128];
void sendfile(SSL* ssl,const char* a,int notadmin,string root) {
    if(notadmin)addlog(root+a);
    if(a[0]){
        string b=root+"/html"+a+(a[strlen(a)-1]=='/'?"index.html":".html");
        if(mysendfile(ssl,b.c_str()))return;
        b=root+a;
        if(mysendfile(ssl,b.c_str()))return;
    }
    mysslwrite(ssl,Head404,strlen(Head404));
}
int ifac[256];
void* work(void* __ssl){
    SSL* ssl=(SSL*)__ssl;
    int cl=SSL_get_fd(ssl),n=0,i,j,notadmin=1;
    char* get=(char*)malloc(10240),*id;
    string root="/443/www";
    n=mysslread(ssl,get,2048);
    if(n<=0)goto https;
    if((id=strstr(get,"Cookie: id="))){
        int tmp=0;
        for(i=11;i<16;i++)tmp=tmp*10+id[i]-'A';
        if(0<=tmp&&tmp<=10000&&*(ll*)(id+11+2)==*(ll*)(user[tmp]+74)&&user[tmp][127]==1)notadmin=0;
    }
    if(strstr(get,"ost: free.neuqboard.cn"))root="/443/free";
    if(strstr(get,"ost: chat.neuqboard.cn"))root="/443/chat";
    if(*(int*)get==542393671){
        if(notadmin==0){
            if(strstr(get,"GET /admin")){
                string a=printlog();
                mysend(ssl,a.c_str(),a.length());
                goto https;
            }
            if(strstr(get,"GET /reset")){
                clearlog();
                mysend(ssl,"OK",2);
                goto https;
            }
        }
        char file[128];
        for(n=1;n<127&&((file[n]=get[n+3])!=46||file[n-1]!=46)&&ifac[(unsigned char)file[n]];n++);
        file[n]=0;
        sendfile(ssl,file+1,notadmin,root);
    }else{
        for(int i=0;i<=50;i++)if(get[i]=='\n'||get[i]=='\r')get[i]=0;
        get[50]=0;
        if(notadmin)addlog(get);
    }
    https://free.neuqboard.cn/
    SSL_free(ssl);
    close(cl);
    free(get);
    return 0;
}
int main() {
    for(char i='0';i<='9';i++)ifac[i]=1;
    for(char i='A';i<='Z';i++)ifac[i]=1;
    for(char i='a';i<='z';i++)ifac[i]=1;
    ifac['/']=ifac['_']=ifac['-']=ifac['.']=1;
    user=(char(*)[128])mmap(0,0x5AA5D000,PROT_READ|PROT_WRITE,MAP_SHARED,open("/1000/pri/user.txt",O_RDWR|O_CREAT),0);
    // mylog=(char*)mmap(0,100*1024,PROT_READ|PROT_WRITE,MAP_SHARED,open("/443/pri/log.dat",O_RDWR|O_CREAT),0);
    // memset(mylog,0,102400);
    mystart(999,work);
    return 0;
}