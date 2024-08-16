#include<openssl/ssl.h>
#include<openssl/err.h>
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
SSL_CTX *ctx;
#include"./myio.h"
const char Head404[]="HTTP/1.1 404 Not Found\r\n\r\n";
std::map<std::string,int>mp;
void sendfile(SSL* ssl,const char* a,int user) {
    if(user)mp[(std::string)a]++;
    if(a[0]){
        std::string b="/443/pub/html"+(std::string)a+(a[strlen(a)-1]=='/'?"index.html":".html");
        if(mysendfile(ssl,b.c_str()))return;
        b="/443/pub"+(std::string)a;
        if(mysendfile(ssl,b.c_str()))return;
    }
    mysslwrite(ssl,Head404,strlen(Head404));
}
void* work(void* cil){
    char* get=(char*)malloc(10240);
    int cl=(long long)cil,n=0,i,j,user=1;
    SSL* ssl=SSL_new(ctx);
    SSL_set_fd(ssl, cl);
    if (SSL_accept(ssl)<=0)goto https;
    while(1){
        int m=SSL_read(ssl,get+n,1024-n);//head less than 1024
        if(m<=0)break;
        get[n+=m]=0;
        if(strstr(get,"\r\n\r\n"))break;
    }
    if(strstr(get,"AAAAE"))user=0;
    if(n<=0)goto https;
    for(i=0;i<n;i++)if(*(ll*)(get+i)==0x75656E2E65657266)break;
    if(i>=n){
        if(*(int*)get==542393671)sendfile(ssl,"/fix",user);
        else mysslwrite(ssl,Head404,strlen(Head404));
        goto https;
    }
    if(*(int*)get==542393671){
        if(strstr(get,"AAAAE")&&strstr(get,"GET /admin")){
            std::string a="";
            for(auto i:mp){
                char t[10]={0};
                sprintf(t," %d\n",i.second);
                a+=i.first+t;
            }
            mysend(ssl,a.c_str(),a.length());
            goto https;
        }
        char file[128];
        for(n=1;n<127&&((file[n]=get[n+3])!=46||file[n-1]!=46);n++)
            if((file[n]<46||57<file[n])&&(file[n]<95||122<file[n]))break;
        file[n]=0;
        sendfile(ssl,file+1,user);
    }
    https://free.neuqboard.cn/
    SSL_free(ssl);
    close(cl);
    free(get);
    return 0;
}
int main() {
    int sock=mycreatsock(999,&ctx);
	pthread_t thread_id;
    while (1) {
        struct sockaddr_in addr;
        uint len=sizeof(addr);
        int client=accept(sock, (struct sockaddr*)&addr, &len);
        if(client<0)printf("accept failed\n");
		else {
            struct timeval timehttps = {10,0};
            setsockopt(client,SOL_SOCKET,SO_RCVTIMEO,(char *)&timehttps,sizeof(struct timeval));
            pthread_create(&thread_id,0,work,(void*)(long long)client);
            pthread_detach(thread_id);
        }
    }
    close(sock);
    SSL_CTX_free(ctx);
    EVP_cleanup();
    return 0;
}