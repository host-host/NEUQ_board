/*

g++ -O2 -Wall -o 1003 ./code/1003.cpp -lssl -lcrypto -lcurl -Wall
*/
#include"http.h"
#include<iostream>
#include<set>
#include<vector>
#include <curl/curl.h>
using namespace std;
set<string>map_apiuse;
struct gpt_point{
    string web;
    vector<string>au;
    vector<string>name;
};
vector<gpt_point>v_gpt;
INIT void gptapi_init(){
    curl_global_init(CURL_GLOBAL_DEFAULT);//curl_global_cleanup();
}
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
void gpt_read(FILE*fin,char*c){
    int i=0;
    for(;i<1023;i++){
        int p=fscanf(fin,"%c",c+i);
        if(p!=1)break;
        if(i==0&&c[i]=='\n'){
            i--;
            continue;
        }
        if(c[i]=='\n')break;
    }
    c[i]=0;
}
void gpt_api_read(){
    vector<gpt_point>v;
    FILE*fin=fopen("./res/pri/gpt.txt","r");
    char c[1024];
    while(1){
        gpt_point New;
        gpt_read(fin,c);
        if(!c[0])break;
        New.web=c;
        int n;
        if(fscanf(fin,"%d",&n)!=1)break;
        for(int i=1;i<=n;i++){
            gpt_read(fin,c);
            New.au.push_back(c);
        }
        if(fscanf(fin,"%d",&n)!=1)break;
        for(int i=1;i<=n;i++){
            gpt_read(fin,c);
            New.name.push_back(c);
        }
        v.push_back(New);
    }
    fclose(fin);
    swap(v,v_gpt);
}
void gptapi1003(http_para *a){
    
    for(int i=0;i<a->n+a->m;i++){
        printf("%c",a->get[i]);
    }
    printf("\n");

    string name="deepseek-v3-250324";
    gpt_api_read();
    for(const gpt_point &i:v_gpt){
        int bj=0;
        for(const string &j:i.name){
            // printf("%s %s\n",name.c_str(),j.c_str());
            if(j==name)bj=1;
        }
        if(bj){
            for(const string &j:i.au){
                if(!map_apiuse.count(j)){
                    string tmp=j;
                    map_apiuse.insert(tmp);
                    CURL *curl=curl_easy_init();
                    curl_easy_setopt(curl, CURLOPT_URL, i.web.c_str());
                    struct curl_slist *headers = NULL;
                    headers = curl_slist_append(headers, "Content-Type: application/json");
                    headers = curl_slist_append(headers,tmp.c_str());
                    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
                    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, a->get+a->n);
                    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
                    curl_easy_setopt(curl, CURLOPT_WRITEDATA, a);
                    a->m=0;
                    curl_easy_perform(curl);// if(res == CURLE_OK) 
                    curl_slist_free_all(headers);
                    curl_easy_cleanup(curl);
                    map_apiuse.erase(tmp);
                    return;
                }
            }
            return http_send(a,Hok Hc0 "Content-Type: text/event-stream\r\n","data: {\"choices\":[{\"index\":0,\"delta\":{\"role\":\"assistant\",\"content\":\"抱歉，服务器繁忙\"}}]}\r\n\r\ndata: [DONE]",0);
        }
    }
    return http_send(a,Hok Hc0 "Content-Type: text/event-stream\r\n","data: {\"choices\":[{\"index\":0,\"delta\":{\"role\":\"assistant\",\"content\":\"错误，没有该模型\"}}]}\r\n\r\ndata: [DONE]",0);

}
int main() {
    http a;
    http_init(&a);
    http_add(&a,"",gptapi1003);
    return http_start(&a,1003);
}