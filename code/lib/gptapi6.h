#ifndef GPTAPI6_H
#define GPTAPI6_H
#include"http.h"
#include"cppJSON.h"
#include<string>

/*
 * 请求上游 OpenAI 兼容接口，同时透传响应并提取本地历史。
 * 
 * 参数解析
 * 
 * a 非空时，透传响应头和响应体写入客户端。
 * a 为空时不写入客户端。
 * url为上游接口url
 * Authorization为上游接口需要的apikey，放于请求头即可
 * message为请求体，原样传给上游即可
 * format表示用什么格式解析响应体，目前支持 "responses" 和 "completions"。
 * response_id 赋值为本次响应的ID
 * 返回值为 追加到本地历史的内容。
 
 * 返回值详解：
 * 对于 responses 格式，返回追加到 input 中的内容，需要尽可能保证和下一次标准客户端的请求的 input 前面部分匹配
 * 对于 completions 格式，返回追加到 messages 中的内容，需要尽可能保证和下一次标准客户端的请求的 messages 前面部分匹配
 */
cppJSON gpt6_work(
    http_para* a,
    const std::string& url,
    const std::string& Authorization,
    const std::string& message,
    const std::string& format,
    std::string& response_id
);
cppJSON my_format(const cppJSON& a);
#endif
