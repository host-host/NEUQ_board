#ifndef WEBSOCKET_
#define WEBSOCKET_
#include"https.h"
#include<string>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <openssl/sha.h>
#include <openssl/bio.h>
#include <openssl/evp.h>
#ifdef __cplusplus
extern "C"{
#endif
/**
void printws(https_para *a,const char* get,int m) {
    int u=fwrite(get,1,m,ffmpeg);
    if(u!=m)LOG();
    fflush(ffmpeg);
}
void work(https_para *a){
    LOG("START");
    if(lock720==0){
        lock720=rand();
    }
    else return;
    sprintf(p,"ffmpeg -i - \
-map 0:v -map 0:a -c:v libx264 -b:v 900k -vf \"scale=1280:720\" -c:a aac -b:a 128k -strict experimental \
-hls_time 1 -hls_list_size 15 -force_key_frames \"expr:gte(t,n_forced*1)\" -f hls './res/free/video/video%d_.m3u8'",
lock720);
    if(!(ffmpeg=popen(p,"w"))){
        lock720=0;
        return;
    };
    startwss(a,printws);
    pclose(ffmpeg);
    lock720=0;
    LOG("END");
}
 */

void base64_encode(const unsigned char *buffer, size_t length, char *b64buffer) {
    BIO *bio, *b64;
    BUF_MEM *bufferPtr;
    b64 = BIO_new(BIO_f_base64());
    bio = BIO_new(BIO_s_mem());
    bio = BIO_push(b64, bio);
    BIO_set_flags(bio, BIO_FLAGS_BASE64_NO_NL);
    BIO_write(bio, buffer, length);
    BIO_flush(bio);
    BIO_get_mem_ptr(bio, &bufferPtr);
    BIO_set_close(bio, BIO_NOCLOSE);
    memcpy(b64buffer, bufferPtr->data, bufferPtr->length);
    b64buffer[bufferPtr->length] = 0;
    BIO_free_all(bio);
}
/* 
函数用于处理WebSocket消息
返回值<0代表解析时错误
返回值>=0代表解析的字符串的长度，返回在payload里，解析了输入的usem个字符，如果这是一个消息的最后一帧，会将ifend置1*/
ll processWebSocketMessage(https_para *a,const char* ch,ll m,char* payload,ll* usem,ll* ifend) {
    if(m<2)return 0;
    if(ch[0]&0x80)*ifend=1;
    char opcode=ch[0]&0x0F;
    if(opcode==8)return -1;
    if(opcode==9)SSL_write(a->ssl,"\x8A\0",2);
    if(opcode==9||opcode==10)*ifend=0;
    ll l=ch[1]&0x7F,start=2;
    char mask[4]={0};
    if(l==126){
        if(m<(start=4))return *ifend=0;
        l=(unsigned char)ch[2];
        l=l*256+(unsigned char)ch[3];
    }else if(l==127){
        if(m<(start=10))return *ifend=0;
        l=0;
        for(int i=2;i<10;i++)l=l*256+(unsigned char)ch[i];
        if(l<0)return -1;
    }
    if(ch[1]>>7&1){
        if(m<(start+=4))return *ifend=0;
        memcpy(mask,ch+start-4,4);
    }
    if(m<start+l)return *ifend=0;
    for(ll i=0;i<l;i++)payload[i]=ch[start+i]^mask[i%4];
    *usem=start+l;
    return l;
}
void startwss(https_para *a,void(*work)(https_para *a,const char* get,int m)){
    const char* p=strstr(a->get,"\r\nSec-WebSocket-Key: ");
    if(p==0)return;
    char tmp[100]={0};
    for(int i=21;p[i]>=32&&p[i]<=127&&i<50;i++)tmp[i-21]=p[i];
    strcat(tmp,"258EAFA5-E914-47DA-95CA-C5AB0DC85B11");
    unsigned char ans[SHA_DIGEST_LENGTH];
    SHA1((unsigned char *)tmp, strlen(tmp), ans);
    char hash[SHA_DIGEST_LENGTH*2+1]={0};
    base64_encode(ans, 20, hash);
    char ret[150]="HTTP/1.1 101 Switching Protocols\r\nUpgrade: websocket\r\nConnection: Upgrade\r\nSec-WebSocket-Accept: ";
    strcat(ret,hash);
    strcat(ret,"\r\n\r\n");
    SSL_write(a->ssl,ret,strlen(ret));
    int last=clock(),n=0,got=0,start=0,die=0;//0~got    start~n
    free(a->get);
    a->get=(char*)malloc(100*1024*1024);
    while(1){
        int m=SSL_read(a->ssl,a->get+n,100000000-n);
        if(m<=0){
            if(SSL_get_error(a->ssl,m)==SSL_ERROR_WANT_READ){
                if(die)return;
                if(clock()-last>=39*CLOCKS_PER_SEC){//ping
                    die=1;
                    SSL_write(a->ssl,"\x89\0",2);
                }
                continue;
            }else return;
        }
        // LOG("%d\n",m);
        n+=m;
        last=clock();
        die=0;
        while(1){
            ll usem=0,ifend=0;
            ll u=processWebSocketMessage(a,a->get+start,n-start,a->get+got,&usem,&ifend);
            if(u<0)return;
            got+=u;
            // LOG("%d\n",got);
            start+=usem;
            if(ifend){
                a->get[got]=0;
                work(a,a->get,got);
                got=0;
                for(int i=start;i<n;i++)a->get[i-start]=a->get[i];
                n-=start;
                start=0;
                continue;
            }
            break;
        }
    }
}
void wss_send(https_para *a,const char* get,ll n){
    if(n==0)n=strlen(get);
    char* tmp=(char*)malloc(n+10),*t1;
    tmp[0]='\x82';
    if(n<=125){
        tmp[1]=n;
        t1=tmp+2;
    }else if(n<65535){
        tmp[1]=126;
        tmp[2]=n/256;
        tmp[3]=n%256;
        t1=tmp+4;
    }else{
        tmp[1]=127;
        ll tn=n;
        for(char* t=tmp+9;t>=tmp+2;t--){
            *t=tn%256;
            tn/=256;
        }
        t1=tmp+10;
    }
    memcpy(t1,get,n);
    SSL_write(a->ssl,tmp,n+(t1-tmp));
    free(tmp);
}




#ifdef __cplusplus
}
#endif
#endif