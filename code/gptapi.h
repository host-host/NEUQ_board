#ifndef GPTAPI_
#define GPTAPI_
#ifdef __cplusplus
extern "C"{
#endif

#include <stdio.h>
#include <string.h>
#include <openssl/bio.h>
#include <openssl/ssl.h>
#include <openssl/err.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <curl/curl.h>
#include"http.h"
#include"user.h"
CURL *curl;
INIT void gptapi_init(){
    curl_global_init(CURL_GLOBAL_DEFAULT);//curl_global_cleanup();
    curl = curl_easy_init();//curl_easy_cleanup(curl);
    curl_easy_setopt(curl, CURLOPT_URL, "https://c-z0-api-01.hash070.com/v1/chat/completions");
    struct curl_slist *headers = NULL;//curl_slist_free_all(headers);
    headers = curl_slist_append(headers, "Content-Type: application/json");
    headers = curl_slist_append(headers, "Authorization: Bearer sk-JeiXwT5X8DDB6F4e5d73T3BLbKFJ5cE554d3ED414d9196Db");
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
}
size_t write_callback(void *ptr, size_t size, size_t nmemb, char *data) {
    http_para *a=(http_para *)data;
    if(a->m==0){
        a->m=1;
        const char* tmp=Hok "Content-Type: text/event-stream\r\n" Hc0 "\r\n";
        // printf("%s",tmp);
        // const char* tmp=Hok Hc0 Htxt "\r\n";
        if(write(a->cl,tmp,strlen(tmp)));
    }
    for(int i=0;i<(int)(size*nmemb);i++)printf("%c",((char*)(ptr))[i]);
    if(write(a->cl,ptr,size*nmemb));
    return size * nmemb;
}
void gptapi(http_para *a){
    // LOG("%s",a->get);
    user_* puser=getuser(a->get);
    if(bncmp(a->get+a->n,"{\"model\":\"gpt-3.5-turbo\",")){
        if(!puser||!puser->admin)return http_send(a,Hok Hc0 "Content-Type: text/event-stream\r\n",
        "data: {\"choices\":[{\"index\":0,\"delta\":{\"role\":\"assistant\",\"content\":\"抱歉，你没有权限使用该模型。请联系NIT授权\"}}]}\r\n\r\ndata: [DONE]",0);
    }
    // CURLcode res;
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, a->get+a->n);
    // char *response =(char*)malloc(102400);
    // response[0]=0;
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, a);
    a->m=0;
    // res = 
    curl_easy_perform(curl);

    // if(res == CURLE_OK) {
    //     http_send(a,Hok Hc0 Hjson,response,0);
    // }
    // free(response);
}

#ifdef __cplusplus
}
#endif
#endif