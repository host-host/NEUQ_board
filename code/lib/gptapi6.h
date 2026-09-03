#ifndef GPTAPI6_H
#define GPTAPI6_H
#include"http.h"
#include"cppJSON.h"
#include<ctime>
#include<map>
#include<string>
std::string gpt6_request_model(http_para* a,const cppJSON& request,const std::string& format);
cppJSON my_format(const cppJSON& a,const std::string& format,int);
struct gpt6_ret{
    http_para*a;
    const char* format;
    cppJSON append;
    long long used_tokens;
    long long input,output,cache,makecache;
    long long start_ns,first_ns,last_ns,end_ns;
    int curlcode;
    long long httpcode;
    std::string response_id,header,body,bodydelta;
    int issse;//0 1no 2yes
};
gpt6_ret gpt6_work3(http_para* a,const char*message,const char*model,cppJSON conf,const char*format);
#endif
