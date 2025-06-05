/*
g++ ./code/1002.cpp -o 1002 -O2 -Wall -lssl -lcrypto


*/
#include<stdio.h>
#include"https.h"
#include<map>
std::map<int,http_para*>mp;
int ID;
SSL* SHO;
pthread_mutex_t mutex;
void solve(https_para* a){
    if(bncmp(a->get+a->n,"W%rf-z*."))return;
    // LOG("NEW connection");
    pthread_mutex_lock(&mutex);
    mp.clear();
    SHO=a->ssl;
    pthread_mutex_unlock(&mutex);
    a->n=0;
    while(SHO==a->ssl){
        int m=SSL_read(a->ssl,a->get+a->n,8-a->n);
        if(m>0)a->n+=m;
        if(a->n<8||m<=0){
            usleep(2000);
            pthread_mutex_lock(&mutex);
            if(SSL_write(SHO,"\0\0\0\0\0\0\0\0",8)!=8)SHO=0;
            pthread_mutex_unlock(&mutex);
            continue;
        }
        int len=*(int*)(a->get),id=*(int*)(a->get+4);
        if(len<0)len=0;
        a->m=0;
        while(a->m<len&&SHO==a->ssl){
            m=SSL_read(a->ssl,a->get+a->m,len-a->m);
            if(m>0)a->m+=m;
            if(m<=0)usleep(2000);
        }
        pthread_mutex_lock(&mutex);
        if(mp.count(id)){
            http_para*tmp=mp[id];
            if(write(tmp->cl,a->get,len));
            close(tmp->cl);
            tmp->cl=0;
            mp.erase(id);
        }
        pthread_mutex_unlock(&mutex);
        a->n=0;
    }
}
void fun(http_para* a){
    // LOG();
    if(SHO==0)return;
    pthread_mutex_lock(&mutex);
    int id=++ID;
    if(SHO){
        mp[id]=a;
        int l=a->n+a->m;
        memmove(a->get+8,a->get,l);
        *(int*)a->get=l;
        *(int*)(a->get+4)=id;
        if(SSL_write(SHO,a->get,l+8));
        // LOG("new get %d %d",l,id);
    }
    pthread_mutex_unlock(&mutex);
    while(1){
        sleep(10);
        pthread_mutex_lock(&mutex);
        int tmp=mp.count(id);
        pthread_mutex_unlock(&mutex);
        if(!tmp)return;
    }
}
void * workk(void*){
    struct https b;
    https_init(&b);
    https_add_web(&b,"neuqboard.cn","./res/ssl/fullchain.crt","./res/ssl/private.pem");
    https_add(&b,"",solve);
    https_start(&b,1000);
    exit(0);
}
int main(){
    pthread_mutex_init(&mutex,0);
	pthread_t thread_id;
    pthread_create(&thread_id,0,workk,0);
    pthread_detach(thread_id);
    struct http a;
    http_init(&a);
    http_add(&a,"",fun);
    // printf("start!\n");
    http_start(&a,1002);
    return 0;
}