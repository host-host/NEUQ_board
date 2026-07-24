#ifndef GPTAPI5_H
#define GPTAPI5_H
#ifdef __cplusplus
extern "C"{
#endif

#include "http.h"

void gptapi5_init();
void gpt5_apikey(http_para* a);
void gpt5_resolve(http_para* a);
void gpt5_history_list(http_para* a);
void gpt5_history_get(http_para* a);
void gpt5_history_rename(http_para* a);
void gpt5_history_delete(http_para* a);
void gpt5_share(http_para* a);
void gpt5_models(http_para* a);
void gpt5_responses(http_para* a);
void gpt5_chat_completions(http_para* a);

#ifdef __cplusplus
}
#endif
#endif
