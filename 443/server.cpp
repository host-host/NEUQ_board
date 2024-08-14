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
#include"./myio.h"
#include"./password.h"
#include"./data.h"
#include"./init.h"
void* work(void* cil){
    char* get=(char*)malloc(102400),id[13]="0";
    int cl=(long long)cil,n=0,i,j;
    SSL* ssl=SSL_new(ctx);
    SSL_set_fd(ssl, cl);
    if (SSL_accept(ssl)<=0)goto https;
    while(1){
        int m=SSL_read(ssl,get+n,1024-n);//head less than 1024
        if(m<=0)break;
        get[n+=m]=0;
        if(strstr(get,"\r\n\r\n"))break;
    }
    if(n<=0)goto https;
    for(i=0;i<n;i++)if(*(ll*)(get+i)==0x75656E2E65657266)break;
    if(i>=n){
        if(*(int*)get==542393671)sendfile(ssl,"/fix");
        else SSL_write(ssl,Head404,strlen(Head404));
        SSL_shutdown(ssl);
        goto https;
    }
    for(i=0;i<e.size();i++){
        for(j=0;e[i].mat[j];j++)if(e[i].mat[j]!=get[j])break;
        if(e[i].mat[j])continue;
        for(int len=0,j=1,k;j<n;j++){
            if(*(ll*)(get+j)==0x3D6469203A65696B)memcpy(id,get+j+8,10);
            if(*(ll*)(get+j)==0x6874676E654C2D74)len=readint(get+j);
            if(*(int*)(get+j)==168626701){
                while(len>n-(j+4)&&(k=SSL_read(ssl,get+n,102200-n))>0)n+=k;
                memset(get+n,0,200);
                e[i].a(ssl,get,get+j+4,n-(j+4),id);
                SSL_shutdown(ssl);
                goto https;
            }
        }
    }
    if(*(int*)get==542393671){
        char file[128];
        for(n=1;n<127&&((file[n]=get[n+3])!=46||file[n-1]!=46);n++)
            if((file[n]<46||57<file[n])&&(file[n]<95||122<file[n]))break;
        file[n]=0;
        sendfile(ssl,file+1);
    }
    SSL_shutdown(ssl);
    https://free.neuqboard.cn/
    SSL_free(ssl);
    close(cl);
    free(get);
    return 0;
}
int main() {
    int sock=init();
	pthread_t thread_id;
    while (1) {
        struct sockaddr_in addr;
        uint len = sizeof(addr);
        int client = accept(sock, (struct sockaddr*)&addr, &len);
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