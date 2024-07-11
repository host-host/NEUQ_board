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
#include"/password.h"
const char Head1[]="HTTP/1.1 200 OK\r\nContent-Type: text/html\r\nContent-Length:";
const char Head2[]="HTTP/1.1 200 OK\r\nAccess-Control-Allow-Origin: *\r\nAccess-Control-Allow-Methods: POST, GET, OPTIONS\r\nAccess-Control-Allow-Credentials: false";
const char Head3[]="HTTP/1.1 200 OK\r\nContent-Type: image/x-icon\r\nContent-Length:";
const char Head4[]="<script>window.location.href = 'http://121.36.103.216/board.html';</script>";
const char Head5[]="<script>alert('Login failed, please try again.');</script>";
struct point{
	const char* content,*mat;
	int n;
};
std::vector<point>e;
#include"/ini.h"
void post(int cl,char*get,int len){
    char t[]="Content-Length:";
    int n=0,i=0;
    for(;i<len;i++)if(get[i]=='\n'){
        int bj=1,j=0;
        for(;t[j];j++)if(get[i+1+j]!=t[j])bj=0;
        if(bj==1)sscanf(get+i+j+1,"%d",&n);
        if(get[i+2]=='\n')break;
    }
    get[i+3+(n=std::min(n,4100-i))]=0;
    if(check(get+i+3))write(cl,Head4,strlen(Head4));
    else write(cl,Head5,strlen(Head5));
}
void* work(void* cli) {
	char* get=(char*)malloc(4196);
	int cl=(long long)cli,n=recv(cl,get,4000,0);
	for(int i=0;i<e.size();i++){
		int bj=1;
		for(int j=0;e[i].mat[j];j++)if(e[i].mat[j]!=get[j])bj=0;
		if(bj==1){
            if(e[i].n==0)post(cl,get,n);
			else write(cl,e[i].content,e[i].n);
			goto out;
		}
	}
	printf("%s\n",get);
	out:
	// printf("%s\n-----------------------------------------\n",get);
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