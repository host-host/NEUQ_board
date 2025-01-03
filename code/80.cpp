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
#include<signal.h>
#include<string>
using std::string;
#define ll long long
const char  uuu[]="HTTP/1.1 301 Moved Permanently\r\nLocation: https://",
            uuu2[]="\r\nStrict-Transport-Security: max-age=3600; includeSubDomains\r\n\r\n";
const char tmp[]="HTTP/1.1 200 OK\r\nContent-Length: 87\r\n\r\n"
                 "tR7u6D-pogOj3NP2juY-MNeNCt2VQFMrYa800e4cLD8.k_dw0dolba_oKg9vg6u43B_8nH2f6EllVI7FC_J02bM";
void* work(void* cil){
    char* get=(char*)malloc(102400),*c;
    int cl=(long long)cil,n=recv(cl,get,2000,0);
    string a=uuu;
    if(n<=0)goto out;
    get[n]=get[n+1]=0;
    c=strstr(get,"Host: ");
    if(c){
        int i;
        for(i=6;c[i]&&c[i]!='\r';i++);
        c[i]='\0';
        c[i+1]='\0';
        a+=c+6;
        for(i=0;get[i]&&get[i]!=' ';i++);
        i++;
        for(;get[i]&&get[i]!=' ';i++)a+=get[i];
    }else return 0;
    a+=uuu2;
    if(strstr(get,"t2VQFMrYa800e4cLD8"))
        write(cl,tmp,strlen(tmp));
    else 
        write(cl,a.c_str(),a.length());
    out:
    free(get);
    close(cl);
    return 0;
}
int main(){
	int serverSock=-1;
	struct sockaddr_in serverAddr;
	serverSock=socket(AF_INET, SOCK_STREAM, 0);
	if(serverSock<0)return 1;
	memset(&serverAddr,0,sizeof(serverAddr));
	serverAddr.sin_family=AF_INET;
	serverAddr.sin_port=htons(80);
	serverAddr.sin_addr.s_addr=INADDR_ANY;
	if(bind(serverSock,(struct sockaddr*)&serverAddr,sizeof(serverAddr))==-1)return 2;
	if(listen(serverSock,10)==-1)return 3;
    signal(SIGPIPE,SIG_IGN);
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