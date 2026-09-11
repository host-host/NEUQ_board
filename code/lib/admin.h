#ifndef ADMIN_H
#define ADMIN_H
#ifdef __cplusplus
extern "C"{
#endif

#include "http.h"
void admin_log_list(http_para* a);
void admin_add_token(http_para* a);

#ifdef __cplusplus
}
#endif
#endif
