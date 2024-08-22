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
typedef void (*fun)(SSL* ssl,const char* re,const char* con,int n,const char* id);
std::map<string,int>users;
char (*user)[128],*data,*cont;
int fdata,fcont;
ll ldata,lcont;
struct point{
	const char *mat;
    fun a;
};
std::vector<point>e;
SSL_CTX *ctx;
#include"./password.h"
#include"./data.h"
void* work(void* __ssl){
    SSL* ssl=(SSL*)__ssl;
    int cl=SSL_get_fd(ssl),n=0,i,j;
    char* get=(char*)malloc(102400),id[13]="0";
    while(1){
        int m=SSL_read(ssl,get+n,1024-n);//head less than 1024
        if(m<=0)break;
        get[n+=m]=0;
        if(strstr(get,"\r\n\r\n"))break;
    }
    if(n<=0)goto https;
    for(i=0;i<e.size();i++){
        for(j=0;e[i].mat[j];j++)if(e[i].mat[j]!=get[j])break;
        if(e[i].mat[j])continue;
        addlog(e[i].mat);
        for(int len=0,j=1,k;j<n;j++){
            if(*(ll*)(get+j)==0x3D6469203A65696B)memcpy(id,get+j+8,10);
            if(*(ll*)(get+j)==0x6874676E654C2D74)len=readint(get+j);
            if(*(int*)(get+j)==168626701){
                while(len>n-(j+4)&&(k=SSL_read(ssl,get+n,102200-n))>0)n+=k;
                memset(get+n,0,200);
                e[i].a(ssl,get,get+j+4,n-(j+4),id);
                goto https;
            }
        }
    }
    https://free.neuqboard.cn/
    SSL_free(ssl);
    close(cl);
    free(get);
    return 0;
}
void admin(SSL *ssl,const char* re,const char* con,int n,const char* id){
    string a=printlog();
    mysslwrite(ssl,a.c_str(),a.length());
}
void OPTIONS(SSL *ssl,const char* re,const char* con,int n,const char* id){
    string tmp="Access-Control-Allow-Methods: POST, GET, OPTIONS\r\n";
    tmp=Hok+tmp+Hoptin+"\r\n";
    mysslwrite(ssl,tmp.c_str(),tmp.length());
}
int main() {
    // char* mylog=(char*)mmap(0,100*1024,PROT_READ|PROT_WRITE,MAP_SHARED,open("/1000/pri/log.dat",O_RDWR|O_APPEND|O_CREAT),0);
    // memset(mylog,0,102400);
    user=(char(*)[128])mmap(0,0x5AA5D000,PROT_READ|PROT_WRITE,MAP_SHARED,open("/1000/pri/user.txt",O_RDWR|O_CREAT),0);
    for(int i=0;*user[i];i++)users[(string)user[i]]=i;
    ldata=lseek(fdata=open("/1000/pri/data.dat",O_RDWR|O_APPEND|O_CREAT),0,SEEK_END);
    data=(char*)mmap(0,4ll<<30,PROT_READ|PROT_WRITE,MAP_SHARED,fdata,0);
    lcont=lseek(fcont=open("/1000/pri/cont.dat",O_RDWR|O_APPEND|O_CREAT),0,SEEK_END);
    cont=(char*)mmap(0,20ll<<30,PROT_READ|PROT_WRITE,MAP_SHARED,fcont,0);
    printf("user:%d\ndata:%lld\ncontent:%lld\n",(int)users.size(),ldata,lcont);
	e.push_back((point){"POST /api/login",login});
	e.push_back((point){"POST /api/register",reg});
	e.push_back((point){"GET /api/user",check_cookie_js});
	e.push_back((point){"POST /api/change_password",change_password});
	e.push_back((point){"GET /api/p=",getp});
	e.push_back((point){"GET /api/con=",getcon});
	e.push_back((point){"POST /api/sendmessage",postmsg});
	e.push_back((point){"GET /api/logout",logout});
	e.push_back((point){"GET /admin",admin});
	e.push_back((point){"OPTIONS",OPTIONS});
    srand(time(0));
    mystart(1000,work);
    return 0;
}