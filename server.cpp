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
#include"/myio.h"
#include"/password.h"
#include"/data.h"
#include"/ini.h"
void post(int cl,char*get,int len,fun func){
    char t[]="Content-Length:",id[13]="0123456789",MAT[]="Cookie: id=";
    for(int n=0,i=0,k;i<len;i++){
        for(k=0;MAT[k];k++)if(MAT[k]!=get[i+k])break;
        if(!MAT[k])memcpy(id,get+i+k,10);
        for(k=0;t[k];k++)if(t[k]!=get[i+k])break;
        if(!t[k])n=readint(get+i+k);
        if(get[i]=='\n'&&get[i+2]=='\n'){
            while(n>len-(i+3)){
                k=recv(cl,get+len,100500-len,0);
                if(k>0)len+=k;
                else break;
            }
            return func(cl,get,get+i+3,len-(i+3),id);
        }
    }
}
void* work(void* cil){
    char* get=(char*)malloc(102400);
    memset(get,0,102400);
    int cl=(long long)cil,n=recv(cl,get,100500,0);
    if(n>0){
        for(int i=0,bj;i<e.size();i++){
            for(int j=bj=0;e[i].mat[j];j++)if(e[i].mat[j]!=get[j])bj=1;
            if(bj==0){
                if(e[i].n)write(cl,e[i].content,e[i].n);
                else post(cl,get,n,(fun)e[i].content);
                goto out;
            }
        }
        printf("%s\n-----------------------------------------\n",get);
    }
    out:
    free(get);
    close(cl);
    return 0;
}
int main(){
    // char c[1280];memset(c,0,1280);
    // FILE*fout=fopen("/user.txt","w");
    // for(int i=1;i<=20000;i++)fwrite(c,1,1280,fout);return 0;
    init();
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
	pthread_t thread_id;
	printf("Start to listen!\n");
	while(1) {
		socklen_t len=sizeof(struct sockaddr_in);
		int clientSock=accept(serverSock,(struct sockaddr*)&serverAddr,&len);
		if(clientSock<0)printf("accept failed\n");
		else {
            pthread_create(&thread_id,0,work,(void*)(long long)clientSock);
            pthread_detach(thread_id);
        }
	}
	close(serverSock);
    return 0;
}