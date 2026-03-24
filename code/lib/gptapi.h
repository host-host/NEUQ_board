#ifndef GPTAPI_
#define GPTAPI_

#include<stdio.h>
#include<string.h>
#include<openssl/bio.h>
#include<openssl/ssl.h>
#include<openssl/err.h>
#include<netinet/in.h>
#include<arpa/inet.h>
#include<unistd.h>
#include<curl/curl.h>
#include<set>
#include<vector>
#include<fstream>
#include<pthread.h>
#include"user.h"
#include"lib/cppJSON.h"
using namespace std;
pthread_mutex_t mutex_api_is_using;
size_t write_callback(void *ptr, size_t size, size_t nmemb, char *data) {
    http_para *a=(http_para *)data;
    if(a->m==0){
        a->m=1;
        const char* tmp=Hok "Content-Type: text/event-stream\r\n" Hc0 "\r\n";
        if(write(a->cl,tmp,strlen(tmp)));
    }
    if(write(a->cl,ptr,size*nmemb));
    return size*nmemb;
}

map<string,int>api_is_using;
char* getgptjson() {
    FILE* file=fopen("./res/pri/gpt.json", "r");
    if(file==0)return 0;
    fseek(file, 0, SEEK_END);
    long size = ftell(file);
    fseek(file, 0, SEEK_SET);
    char* buffer = (char*)malloc(size + 1);
    if (buffer == NULL) {
        fclose(file);
        return 0;
    }
    size_t bytes_read = fread(buffer, 1, size, file);
    if ((long)bytes_read != size) {
        free(buffer);
        fclose(file);
        return NULL;
    }
    buffer[size] = '\0';
    fclose(file);
    return buffer;
}
void gptapi2(http_para *a){
    char* content=getgptjson();
    cppJSON json=JSON.parse(content);
    if(content)free(content);
    if(!json)return http_send(a,H500 Hc0 Htxt,"can not read json file.",0);
    cppJSON au=JSON.parse(a->get+a->n);
    user_* tmp=getuser(a->get);
    int admin=tmp?tmp->admin:0;
    int ida=au["ida"].valuedouble();
    string model=au["model"].valuestring();
    au.erase("ida");
    cppJSON now=json[ida];
    int maxparallel=now["parallel"].valuedouble();
    if(maxparallel==0)maxparallel=1;
    LOG("%d",admin);
    if(now.has("available")&&now["available"]==false)return http_send(a,Hok Hc0 Htxt,"data: {\"choices\":[{\"index\":0,\"delta\":{\"role\":\"assistant\",\"content\":\"错误，模型不可用\"}}]}\r\n\r\ndata: [DONE]",0);
    if(admin==0&&!(now["public"]==true))return http_send(a,Hok Hc0 Htxt,"data: {\"choices\":[{\"index\":0,\"delta\":{\"role\":\"assistant\",\"content\":\"错误，没有权限\"}}]}\r\n\r\ndata: [DONE]",0);
    for(cppJSON p=now["model"].child();p.a;p=p.next()){
        if(p==model){
            for(cppJSON q=now["Authorization"].child();q.a;q=q.next()){
                string tmp=q.valuestring();
                int bj=0;
                pthread_mutex_lock(&mutex_api_is_using);
                if(api_is_using[tmp]<maxparallel){
                    bj=1;
                    api_is_using[tmp]++;
                }
                pthread_mutex_unlock(&mutex_api_is_using);
                if(bj){
                    string debug0,debug1,debug2;
                    debug0="Authorization: "+tmp;
                    debug1=JSON.stringify_Unformatted(au);
                    debug2=now["url"].valuestring();
                    CURL *curl=curl_easy_init();
                    curl_easy_setopt(curl, CURLOPT_URL, debug2.c_str());
                    struct curl_slist *headers = NULL;
                    headers = curl_slist_append(headers, "Content-Type: application/json");
                    headers = curl_slist_append(headers,debug0.c_str());
                    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
                    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, debug1.c_str());
                    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
                    curl_easy_setopt(curl, CURLOPT_WRITEDATA, a);
                    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 60L);
                    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 1200L);
                    a->m=0;
                    curl_easy_perform(curl);// if(res == CURLE_OK) 
                    curl_slist_free_all(headers);
                    curl_easy_cleanup(curl);
                    pthread_mutex_lock(&mutex_api_is_using);
                    api_is_using[tmp]--;
                    pthread_mutex_unlock(&mutex_api_is_using);
                    return;
                }
            }
            return http_send(a,Hok Hc0 Htxt,"data: {\"choices\":[{\"index\":0,\"delta\":{\"role\":\"assistant\",\"content\":\"服务繁忙，请稍后再试\"}}]}\r\n\r\ndata: [DONE]",0);
        }
    }
    return http_send(a,Hok Hc0 Htxt,"data: {\"choices\":[{\"index\":0,\"delta\":{\"role\":\"assistant\",\"content\":\"错误，未找到模型\"}}]}\r\n\r\ndata: [DONE]",0);
}
void gptapis2(http_para *a){
    char* content=getgptjson();
    cppJSON json=JSON.parse(content);
    if(content)free(content);
    for(cppJSON p=json.child();p.a;p=p.next()){
        p.erase("Authorization");
        p.erase("url");
    }
    return http_send(a,Hok Hjson Hc0,json.stringify(json).c_str(),0);
}
#endif