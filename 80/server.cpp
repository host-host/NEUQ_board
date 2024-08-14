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
#define ll long long
const char Headhtml[]="HTTP/1.1 200 OK\r\ncache-control: max-age=3600, public\r\n"
                      "Content-Type: text/html\r\nContent-Length:";
const char Head404[]="HTTP/1.1 404 Not Found\r\n\r\n";
const char uuu[]="HTTP/1.1 301 Moved Permanently\r\nLocation: https://free.neuqboard.cn\r\nStrict-Transport-Security: max-age=3600; includeSubDomains\r\n\r\n";
int sendfile(int cl, const char* a){
    FILE* file=fopen(a,"rb");
    if(!file)return 0;
    fseek(file,0,SEEK_END);
    int fileSize=ftell(file);
    fseek(file,0,SEEK_SET);
    char* content=(char*)malloc(fileSize+300);
    sprintf(content,"%s%d\r\n\r\n",Headhtml,fileSize);
    int headerLength=strlen(content);
    size_t bytesRead=fread(content+headerLength,1,fileSize,file);
    if (bytesRead!=fileSize) {
        free(content);
        fclose(file);
        return 0;
    }
    write(cl,content,headerLength+fileSize);
    free(content);
    fclose(file);
    return 1;
}
void* work(void* cil){
    char* get=(char*)malloc(102400),id[13]="0";
    int cl=(long long)cil,n=recv(cl,get,2000,0),i,j;
    if(n<=0)goto out;
    for(i=0;i<n;i++)if(*(ll*)(get+i)==0x75656E2E65657266)break;
    if(i>=n){
        if(*(int*)get==542393671)sendfile(cl,"/443/pub/html/fix.html");
        else write(cl,Head404,strlen(Head404));
        goto out;
    }
    write(cl,uuu,strlen(uuu));
    out:
    free(get);
    close(cl);
    return 0;
}
int main(){
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