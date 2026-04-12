#ifndef GPTAPI_
#define GPTAPI_
#include"http.h"
#ifdef __cplusplus
extern "C"{
#endif

char* getgptjson();
void gptapi2(http_para *a);
void gptapis2(http_para *a);

#ifdef __cplusplus
}
#endif
#endif