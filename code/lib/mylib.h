#ifndef MYLIB_H
#define MYLIB_H

#include <stddef.h>
#include <stdio.h>
#include "cJSON.h"
#ifdef __cplusplus
extern "C" {
#endif

#define LOG(a,...) printf("<%s : %d>%s : " a "\n",__FILE__,__LINE__,__func__,##__VA_ARGS__)
#define TRY(a) __sync_val_compare_and_swap(a,0,1)
#define LOCK(a) while(__sync_val_compare_and_swap(a,0,1))usleep(0)
#define UNLOCK(a) do{asm volatile("":::"memory");a=0;}while(0)
#define ADD(a,b) __atomic_add_fetch(a,b,__ATOMIC_SEQ_CST)
size_t mylib_random_string(char* out,size_t length);
void mylib_sha256(const void* data,size_t length,char out[44]);
size_t utf8_substr(const char *c,size_t n);

#ifdef __cplusplus
}
#endif

#endif
