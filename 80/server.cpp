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
void* work(void* cil){
    char* get=(char*)malloc(102400),id[13]="0";
    int cl=(long long)cil,n=recv(cl,get,2000,0),i,j;
    if(n<=0)goto out;
    for(i=0;i<n;i++)if(*(ll*)(get+i)==0x75656E2E65657266)break;
    if(i>=n){
        if(*(int*)get==542393671)sendfile(cl,"/fix");
        else write(cl,Head404,strlen(Head404));
        goto out;
    }
    for(i=0;i<e.size();i++){
        for(j=0;e[i].mat[j];j++)if(e[i].mat[j]!=get[j])break;
        if(e[i].mat[j])continue;
        for(int len=0,j=1,k;j<n;j++){
            if(*(ll*)(get+j)==0x3D6469203A65696B)memcpy(id,get+j+8,10);
            if(*(ll*)(get+j)==0x6874676E654C2D74)len=readint(get+j);
            if(*(int*)(get+j)==168626701){
                while(len>n-(j+4)&&(k=recv(cl,get+n,100500-n,0))>0)n+=k;
                memset(get+n,0,200);
                e[i].a(cl,get,get+j+4,n-(j+4),id);
                goto out;
            }
        }
    }
    if(*(int*)get==542393671){
        char file[128];
        for(n=1;n<127&&((file[n]=get[n+3])!=46||file[n-1]!=46);n++)
            if((file[n]<46||57<file[n])&&(file[n]<95||122<file[n]))break;
        file[n]=0;
        sendfile(cl,file+1);
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
    data=(char*)mmap(0,4ll<<30,PROT_READ|PROT_WRITE,MAP_SHARED,fdata,0);
    lcont=lseek(fcont=open("/cont.dat",O_RDWR|O_APPEND|O_CREAT),0,SEEK_END);
    cont=(char*)mmap(0,20ll<<30,PROT_READ|PROT_WRITE,MAP_SHARED,fcont,0);
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