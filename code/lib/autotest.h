#ifndef AUTOTEST_H
#define AUTOTEST_H
#include "http.h"
#ifdef __cplusplus
extern "C"{
#endif
void autotest_init();
bool autotest_blocked(const char* model,const char* provider);
void autotest_userok(const char* model,const char* provider);
void autotest_get(http_para* a);
void autotest_set(http_para* a);
#ifdef __cplusplus
}
#endif
#endif
