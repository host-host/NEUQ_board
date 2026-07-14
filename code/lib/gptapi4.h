#ifndef GPTAPI4_
#define GPTAPI4_

#include "http.h"
#include "cppJSON.h"
#include <string>

char* getgpt2json();
cppJSON gpt_req(http_para* a,const std::string& id,const std::string& url,const std::string& auth,const std::string& content,const std::string& format);

#endif
