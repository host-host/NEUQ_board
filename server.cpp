#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<sys/types.h>
#include<sys/socket.h>
#include<netinet/in.h>
#include<unistd.h>
#include<errno.h>
#include<pthread.h>
#include<vector>
#include<time.h>
#include"/myio.h"
#include"/password.h"
#include"/ini.h"
void post(int cl,char*get,int len,void(*fun)(char*,int)){
    if(fun==check_cookie_js)return check_cookie_js(get,cl);
    char t[]="Content-Length:";
    int n=0,i=0;
    for(;i<len;i++)if(get[i]=='\n'){
        int bj=1,j=0;
        for(;t[j];j++)if(get[i+1+j]!=t[j])bj=0;
        if(bj==1)n=readint(get+i+j);
        if(get[i+2]=='\n')return fun(get+i+3,cl);
    }
}
void* work(void* cli) {
	char* get=(char*)malloc(4196);
	int cl=(long long)cli;
    int n=recv(cl,get,4000,0);//post不会超过4000
    if(n<0){
        free(get);
        return 0;
    }
    get[n]=0;
	for(int i=0;i<e.size();i++){
		int bj=1;
		for(int j=0;e[i].mat[j];j++)if(e[i].mat[j]!=get[j])bj=0;
		if(bj==1){
            if(e[i].n==0)post(cl,get,n,(void(*)(char*,int))e[i].content);
			else write(cl,e[i].content,e[i].n);
			goto out;
		}
	}
	printf("%s\n-----------------------------------------\n",get);
	out:
	free(get);
	close(cl);
	return 0;
}
int main() {
    ini();
	int serverSock=-1;
	struct sockaddr_in serverAddr;
	serverSock=socket(AF_INET, SOCK_STREAM, 0);
	if(serverSock<0)return-1;
	printf("Socket create successfully.\n");
	memset(&serverAddr,0,sizeof(serverAddr));
	serverAddr.sin_family=AF_INET;
	serverAddr.sin_port=htons(888);
	serverAddr.sin_addr.s_addr=INADDR_ANY;
	if(bind(serverSock,(struct sockaddr*)&serverAddr,sizeof(serverAddr))==-1)return-1;
	printf("Bind successful.\n");
	if(listen(serverSock,10)==-1)return-1;
	printf("Start to listen!\n");
	pthread_t thread_id;
	while(1) {
		socklen_t len=sizeof(struct sockaddr_in);
		int clientSock=accept(serverSock,(struct sockaddr*)&serverAddr,&len);
		if(clientSock<0)printf("accept failed\n");
		else pthread_create(&thread_id,0,work,(void*)(long long)clientSock);
	}
	close(serverSock);
	return 0;
}