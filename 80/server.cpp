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
#include"./myio.h"
#include"./password.h"
#include"./data.h"
void post(int cl,char*g,int l,fun func){
    char id[13]="0";
    for(int n=0,i=1,k;i<l;i++){
        if(*(ll*)(g+i)==0x3D6469203A65696B)memcpy(id,g+i+8,10);
        if(*(ll*)(g+i)==0x6874676E654C2D74)n=readint(g+i);
        if(*(int*)(g+i)==168626701){
            while(n>l-(i+4)&&(k=recv(cl,g+l,100500-l,0))>0)l+=k;
            memset(g+l,0,200);
            return func(cl,g,g+i+4,l-(i+4),id);
        }
    }
}
void* work(void* cil){
    char* get=(char*)malloc(102400);
    int cl=(long long)cil,n=recv(cl,get,2000,0),i,j;
    if(n<=0)goto out;
    for(i=0;i<n;i++)if(*(ll*)(get+i)==0x75656E2E65657266)break;
    if(i>=n){
        sendfile(cl,"/fix.html");
        goto out;
    }
    for(i=0;i<e.size();i++){
        for(j=0;e[i].mat[j];j++)if(e[i].mat[j]!=get[j])break;
        if(!e[i].mat[j]){
            post(cl,get,n,e[i].a);
            goto out;
        }
    }
    if(*(int*)get==542393671){
        char file[128];
        int l=0;
        for(;l<127;l++){
            file[l]=get[l+4];
            if(l&&file[l]=='.'&&file[l-1]=='.')break;
            int bj=0;
            bj+=file[l]=='.';
            bj+=file[l]=='/';
            bj+=file[l]=='_';
            bj+='a'<=file[l]&&file[l]<='z';
            bj+='A'<=file[l]&&file[l]<='Z';
            bj+='0'<=file[l]&&file[l]<='9';
            if(bj==0)break;
        }
        file[l]=0;
        sendfile(cl,file);
    }
    out:
    free(get);
    close(cl);
    return 0;
}
int main(){
    user=(char(*)[128])mmap(0,0x5AA5D000,PROT_READ|PROT_WRITE,MAP_SHARED,fil=open("/user.txt",O_RDWR|O_CREAT),0);
    for(int i=0;*user[i];i++)users[(std::string)user[i]]=i;
    ldata=lseek(fdata=open("/data.dat",O_RDWR|O_APPEND|O_CREAT),0,SEEK_END);
    data=(char*)mmap(0,4ll*1024*1024*1024,PROT_READ|PROT_WRITE,MAP_SHARED,fdata,0);
    lcont=lseek(fcont=open("/cont.dat",O_RDWR|O_APPEND|O_CREAT),0,SEEK_END);
    cont=(char*)mmap(0,20ll*1024*1024*1024,PROT_READ|PROT_WRITE,MAP_SHARED,fcont,0);
    printf("user:%d\ndata:%lld\ncontent:%lld\n",(int)users.size(),ldata,lcont);
	e.push_back((point){"POST /api/login",login});
	e.push_back((point){"POST /api/register",reg});
	e.push_back((point){"GET /api/user",check_cookie_js});
	e.push_back((point){"POST /api/change_password",change_password});
	e.push_back((point){"GET /p=",getp});
	e.push_back((point){"GET /con=",getcon});
	e.push_back((point){"POST /api/sendmessage",postmsg});
	e.push_back((point){"GET /logout",logout});
    srand(time(0));
	int serverSock=-1;
	struct sockaddr_in serverAddr;
	serverSock=socket(AF_INET, SOCK_STREAM, 0);
	if(serverSock<0)return-1;
	memset(&serverAddr,0,sizeof(serverAddr));
	serverAddr.sin_family=AF_INET;
	serverAddr.sin_port=htons(888);
	serverAddr.sin_addr.s_addr=INADDR_ANY;
	if(bind(serverSock,(struct sockaddr*)&serverAddr,sizeof(serverAddr))==-1)return-1;
	if(listen(serverSock,10)==-1)return-1;
	pthread_t thread_id;
	printf("Start to listen!\n");
	while(1) {
		socklen_t len=sizeof(struct sockaddr_in);
		int clientSock=accept(serverSock,(struct sockaddr*)&serverAddr,&len);
		if(clientSock<0)printf("accept failed\n");
		else {
            struct timeval timeout = {10,0};
            setsockopt(clientSock,SOL_SOCKET,SO_RCVTIMEO,(char *)&timeout,sizeof(struct timeval));
            pthread_create(&thread_id,0,work,(void*)(long long)clientSock);
            pthread_detach(thread_id);
        }
	}
	close(serverSock);
    return 0;
}