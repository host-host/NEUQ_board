#ifndef GPTAPI6_H
#define GPTAPI6_H
#include"http.h"
#include"cppJSON.h"
#include<string>

/*
 * 请求上游 AI 接口，向客户端原样透传响应，并提取可追加到本地历史的内容。
 *
 * 参数：
 * a              非空时写入上游响应头和响应体，为空时只请求和解析上游。
 * url            上游接口地址。
 * Authorization  上游 API key；根据 format 写入 Authorization、x-api-key
 *                或 x-goog-api-key。Claude 固定携带 anthropic-version: 2023-06-01。
 * message        原样发送给上游的 JSON 请求体。
 * format         支持 responses、completions、claude 和 gemini。
 * response_id    上游原生响应 ID；上游未返回 ID 时为空。
 *
 * gpt6_work 的返回值必须是数组，数组内容是待追加的历史项：
 * responses   追加到下一次请求的 input。
 * completions 追加到下一次请求的 messages。
 * claude      追加到下一次 Claude 请求的 messages，保留 role/content blocks。
 * gemini      追加到下一次 Gemini 请求的 contents，保留 role/parts。
 */
cppJSON gpt6_work(
    http_para* a,
    const std::string& url,
    const std::string& Authorization,
    const std::string& message,
    const std::string& format,
    std::string& response_id
);
std::string gpt6_request_model(http_para* a,const cppJSON& request,const std::string& format);
bool gpt6_is_assistant(const cppJSON& item,const std::string& format);
cppJSON my_format(const cppJSON& a,const std::string& format,int);
#endif
