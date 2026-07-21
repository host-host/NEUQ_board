#ifndef MYLIB_H
#define MYLIB_H

#include <stddef.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

#define LOG(a,...) printf("<%s : %d>%s : " a "\n",__FILE__,__LINE__,__func__,##__VA_ARGS__)
void mylib_random_string(char* out,size_t length);
void mylib_sha256(const void* data,size_t length,char out[44]);

#ifdef __cplusplus
}
#endif

#endif
