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

static inline cJSON *mylib_jget_key(const cJSON *a,const char *key){return cJSON_IsObject(a)?cJSON_GetObjectItemCaseSensitive(a,key):0;}
static inline cJSON *mylib_jget_index(const cJSON *a,int index){return cJSON_IsArray(a)?cJSON_GetArrayItem(a,index):0;}
static inline cJSON_bool mylib_jinsert_take(cJSON *a,const char *key,cJSON *item){
    if(!cJSON_IsObject(a)||!key||!item)return 0;
    cJSON_DeleteItemFromObjectCaseSensitive(a,key);
    return cJSON_AddItemToObject(a,key,item);
}
static inline cJSON_bool mylib_jinsert_string(cJSON *a,const char *key,const char *value){
    if(!cJSON_IsObject(a)||!key)return 0;
    cJSON_DeleteItemFromObjectCaseSensitive(a,key);
    return cJSON_AddStringToObject(a,key,value)!=0;
}
static inline cJSON_bool mylib_jinsert_number(cJSON *a,const char *key,double value){
    if(!cJSON_IsObject(a)||!key)return 0;
    cJSON_DeleteItemFromObjectCaseSensitive(a,key);
    return cJSON_AddNumberToObject(a,key,value)!=0;
}
// static inline cJSON_bool mylib_jinsert_bool(cJSON *a,const char *key,_Bool value){
//     if(!cJSON_IsObject(a)||!key)return 0;
//     cJSON_DeleteItemFromObjectCaseSensitive(a,key);
//     return cJSON_AddBoolToObject(a,key,value)!=0;
// }
#define MYLIB_JGET_STEP(object,key) _Generic((key), \
    char *: mylib_jget_key, \
    const char *: mylib_jget_key, \
    int: mylib_jget_index, \
    unsigned int: mylib_jget_index, \
    long: mylib_jget_index, \
    unsigned long: mylib_jget_index, \
    long long: mylib_jget_index, \
    unsigned long long: mylib_jget_index \
)((object),(key))
#define mylib_jget2(a,b) MYLIB_JGET_STEP((a),(b))
#define mylib_jget3(a,b,c) MYLIB_JGET_STEP(mylib_jget2((a),(b)),(c))
#define mylib_jget4(a,b,c,d) MYLIB_JGET_STEP(mylib_jget3((a),(b),(c)),(d))
#define mylib_jget5(a,b,c,d,e) MYLIB_JGET_STEP(mylib_jget4((a),(b),(c),(d)),(e))
#define mylib_jget6(a,b,c,d,e,f) MYLIB_JGET_STEP(mylib_jget5((a),(b),(c),(d),(e)),(f))
#define MYLIB_JGET_SELECT(_1,_2,_3,_4,_5,_6,name,...) name
#define jget(...) MYLIB_JGET_SELECT(__VA_ARGS__,mylib_jget6,mylib_jget5,mylib_jget4,mylib_jget3,mylib_jget2)(__VA_ARGS__)
#define MYLIB_JINSERT_ONE(object,key,value) _Generic((value), \
    char *: mylib_jinsert_string, \
    const char *: mylib_jinsert_string, \
    int: mylib_jinsert_number, \
    double: mylib_jinsert_number, \
    cJSON *: mylib_jinsert_take \
)((object),(key),(value))
#define mylib_jinsert3(a,x1,y1) MYLIB_JINSERT_ONE((a),(x1),(y1))
#define mylib_jinsert5(a,x1,y1,x2,y2) (mylib_jinsert3((a),(x1),(y1))&&MYLIB_JINSERT_ONE((a),(x2),(y2)))
#define mylib_jinsert7(a,x1,y1,x2,y2,x3,y3) (mylib_jinsert5((a),(x1),(y1),(x2),(y2))&&MYLIB_JINSERT_ONE((a),(x3),(y3)))
#define MYLIB_JINSERT_SELECT(_1,_2,_3,_4,_5,_6,_7,name,...) name
#define jinsert(...) MYLIB_JINSERT_SELECT(__VA_ARGS__,mylib_jinsert7,mylib_jinsert5,mylib_jinsert5,mylib_jinsert3,mylib_jinsert3)(__VA_ARGS__)
cJSON* file2j(const char*file);
#ifdef __cplusplus
}
#endif

#endif
