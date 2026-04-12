#ifndef CHECK48_
#define CHECK48_

#include"http.h"
#ifdef __cplusplus
extern "C"{
#endif


#define CHECK_LEN 48
extern int rand_base,rand_base1;
void check48_init();
int check_48_(unsigned long long a);
void check48(http_para* ssl);

#ifdef __cplusplus
}
#endif
#endif