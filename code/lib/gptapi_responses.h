#ifndef GPTAPI_RESPONSES_H
#define GPTAPI_RESPONSES_H

#include "http.h"
#include "cppJSON.h"
#include <string>

bool gpt_responses_request(http_para *a,
                           const std::string& api_url,
                           const std::string& auth_token,
                           const cppJSON& chat_request,
                           std::string& assistant_reply);

bool gpt_responses_title(const std::string& api_url,
                         const std::string& auth_token,
                         const cppJSON& chat_request,
                         std::string& title);

#endif
