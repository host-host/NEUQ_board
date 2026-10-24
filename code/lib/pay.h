#ifndef PAY_H
#define PAY_H
#ifdef __cplusplus
extern "C"{
#endif

#include "http.h"
void card_init();
void admin_card_create(http_para* a);
void card_redeem(http_para* a);

#ifdef __cplusplus
}
#endif
#endif
