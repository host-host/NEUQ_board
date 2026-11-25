#ifndef GPTAPI5_H
#define GPTAPI5_H
#ifdef __cplusplus
extern "C"{
#endif

#include "http.h"
#include "ndb2.h"
#include "gptapi4.h"
#include <time.h>
struct content{
    bool publish;
    char deleted;//废弃
    bool isusing;
    char ownerid[10];
    char ownername[24];
    long long createtime;
    long long updatetime;
    char name[64];
    char format[20];
    char con_id[32];
    char hash[44];
    char other[1024-44];
    char content[0];
};
void gptapi5_init();
void gpt5_dbclean4(http_para* a);
void gpt5_apikey(http_para* a);
void gpt5_resolve(http_para* a);
void gpt5_history_list(http_para* a);
void gpt5_history_get(http_para* a);
void gpt5_history_rename(http_para* a);
void gpt5_history_delete(http_para* a);
void gpt5_share(http_para* a);
void gpt5_models(http_para* a);
void gpt4_askstable(http_para* a);
void gpt5_responses(http_para* a);
void gpt5_chat_completions(http_para* a);
void gpt5_claude_messages(http_para* a);
void gpt5_gemini_generate_content(http_para* a);
void gpt5_image_generations(http_para* a);
void gpt5_model_list(http_para *a);
bool local_request(http_para* a);

#ifdef __cplusplus
}
#endif
#endif
