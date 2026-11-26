#ifndef COMPLETIONS2CLAUDE_H
#define COMPLETIONS2CLAUDE_H
#include "cppJSON.h"
#include <string>
//用户completions格式，上游claude格式（映射规则参考cc-switch proxy/providers/transform.rs 的反方向）
//请求转换：原地改写u，内部状态挂到internal（流式响应转换需要）
void completions2claude_request(cppJSON& u,void*& internal);
//响应转换：resp原地改写为completions格式；SSE时每次传入一行
void completions2claude_response(std::string& resp,void* internal,bool issse);
//释放internal（调用后internal悬空，不要再使用）
void completions2claude_clean(void* internal);
#endif
