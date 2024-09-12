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
#include"/443/mylog.h"
const char Head404[]="HTTP/1.1 404 Not Found\r\n\r\n";
char (*user)[128];
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
    char* content=(char*)malloc(fileSize+400);
    int n=sprintf(content,"%s%s%s",Hok,Hc3600,head);
    if(strstr(a,"/gzip/"))n+=sprintf(content+n,"%s",Hgzip);
    else if(strstr(a,"/zstd/"))n+=sprintf(content+n,"%s",Hzstd);
    else if(strstr(a,"/br/"))n+=sprintf(content+n,"%s",Hbr);
    if(strstr(a,"/file/"))n+=sprintf(content+n,"%s",Hdown);
    n+=sprintf(content+n,"%s%d\r\n\r\n",Hconl,fileSize);
    if(fread(content+n,1,fileSize,file)!=fileSize) {
        free(content);
        fclose(file);
        return 0;
    }
    mysslwrite(ssl,content,fileSize+n);
    free(content);
    fclose(file);
    return 1;
}
void sendfile(SSL* ssl,const char* a,int notadmin,string root) {
    if(notadmin)addlog((root+a).c_str());
    if(a[0]){
        string b=root+"/html"+a+(a[strlen(a)-1]=='/'?"index.html":".html");
        if(mysendfile(ssl,b.c_str()))return;
        b=root+a;
        if(mysendfile(ssl,b.c_str()))return;
    }
    mysslwrite(ssl,Head404,strlen(Head404));
}
int ifac[256];
void work(SSL* ssl,char* get,int n){
    int i,j,notadmin=1;
    char *id;
    string root="/443/www";
    if(n<=0)return;
    if((id=strstr(get,"Cookie: id="))){
        int tmp=0;
        for(i=11;i<16;i++)tmp=tmp*10+id[i]-'A';
        if(0<=tmp&&tmp<=10000&&*(ll*)(id+11+2)==*(ll*)(user[tmp]+74)&&user[tmp][127]==1)notadmin=0;
    }
    if(strstr(get,": free.neuqboard.cn"))root="/443/free";
    if(strstr(get,": chat.neuqboard.cn"))root="/443/chat";
    if(strstr(get,": dev.neuqboard.cn"))root="/443/dev";
    if(strstr(get,": file.neuqboard.cn"))root="/443/file";
    if(*(int*)get==542393671){
        if(notadmin==0){
            if(strstr(get,"GET /admin")){
                string a=printlog();
                mysslwrite(ssl,a.c_str(),a.length());
                return;
            }
            if(strstr(get,"GET /reset")){
                clearlog();
                mysend(ssl,"OK",2);
                return;
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
}
int main() {
    pthread_mutex_init(&mloglock,0);
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