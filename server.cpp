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
const char Head1[]="HTTP/1.1 200 OK\r\nContent-Type: text/html\r\nContent-Length:";
const char Head2[]="HTTP/1.1 200 OK\r\nAccess-Control-Allow-Origin: *\r\nAccess-Control-Allow-Methods: POST, GET, OPTIONS\r\nAccess-Control-Allow-Credentials: false";
struct point{
	const char* content,*mat;
	int n;
};
std::vector<point>e;
void add(const char* file,const char* mat,const char* head){
	char *fil=(char*)malloc(102400);
	FILE*fin=fopen(file,"r");
	int n=fread(fil,1,102400,fin);
    printf("%d\n",n);
	fclose(fin);
	char* content=(char*)malloc(n+100);
	sprintf(content,"%s%d\r\n\r\n%s",head,n,fil);
	free(fil);
	e.push_back((point){content,mat,(int)strlen(content)});
}
void post(int cl,char*get,int len){
    char t[]="Content-Length:";
    int n=0,i=0;
    for(;i<len;i++)if(get[i]=='\n'){
        int bj=1,j=0;
        for(;t[j];j++)if(get[i+1+j]!=t[j])bj=0;
        if(bj==1)sscanf(get+i+j+1,"%d",&n);
        if(get[i+2]=='\n')break;
    }
    write(cl,get+i+3,n);
}
void* work(void* cli) {
	char* get=(char*)malloc(4196);
	int cl=(long long)cli,n=recv(cl,get,4196,0);
	for(int i=0;i<e.size();i++){
		int bj=1;
		for(int j=0;e[i].mat[j];j++)if(e[i].mat[j]!=get[j])bj=0;
		if(bj==1){
            if(i==0)post(cl,get,n);
			else write(cl,e[i].content,e[i].n);
			close(cl);
			return 0;
		}
	}
	printf("%s\n",get);
	close(cl);
	return 0;
}
int main() {
	e.push_back((point){0,"POST",0});
	add("/main.html","GET / ",Head1);
	add("/login.html","GET /login.html ",Head1);
	add("/reg.html","GET /reg.html ",Head1);
	e.push_back((point){Head2,"OPTIONS",strlen(Head2)});
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
