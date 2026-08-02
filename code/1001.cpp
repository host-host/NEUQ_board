#include"lib/http.h"
#include"lib/user.h"
#include"lib/chat.h"
#include"lib/check48.h"
// #include"lib/word.h"
#include"lib/gptapi5.h"
#include"lib/mylib.h"
#include<cstdio>
#include<cstring>
#include <pthread.h>
void other(http_para* a){
    LOG("%s\n",a->get);
}
void apistop(http_para* a){//curl http://127.0.0.1:1001/api/stop
    a->get[a->n]=0;
    if(strstr(a->get,"\r\nX-Forwarded-For:")||strstr(a->get,"\r\nX-Real-IP:"))
        return http_send(a,Hok Hc0 Htxt,"Error: Permission Denied.",0);
    http_send(a,Hok Hc0 Htxt,"ok",0);
    http_stop(a->f);
}
int main() {
    http a;
    http_init(&a);
    pthread_t thread_id;
    if(!pthread_create(&thread_id,0,gpt5_probe_loop,0))pthread_detach(thread_id);
    else return -98;
    http_add(&a,"POST /api/chat_list ",chat_list);
    http_add(&a,"POST /api/chat_content ",chat_content);
    http_add(&a,"POST /api/chat_send ",chat_send);
    http_add(&a,"POST /api/login ",login);
    http_add(&a,"POST /api/register ",reg);
    http_add(&a,"GET /api/logout ",logout);
    http_add(&a,"POST /api/logout ",logout);
    http_add(&a,"GET /api/user ",apiuser);
    http_add(&a,"POST /api/uploads_file ",uploads_file);
    http_add(&a,"POST /api/download_file ",download_file);
    http_add(&a,"POST /api/change_password ",change_password);
    http_add(&a,"GET /api/check48 ",check48);
    // http_add(&a,"POST /api/getword ",getword);
    // http_add(&a,"POST /api/setword ",setword);
    http_add(&a,"POST /api/gpt5_model_list ",gpt5_model_list);
    http_add(&a,"POST /api/gpt5_askstable ",gpt5_askstable);

    http_add(&a,"POST /v1/responses ",gpt5_responses);
    http_add(&a,"POST /v1/responses?",gpt5_responses);
    http_add(&a,"POST /api/v1/responses ",gpt5_responses);

    http_add(&a,"POST /v1/chat/completions ",gpt5_chat_completions);
    http_add(&a,"POST /v1/chat/completions?",gpt5_chat_completions);
    http_add(&a,"POST /api/v1/chat/completions ",gpt5_chat_completions);

    http_add(&a,"POST /v1/messages ",gpt5_claude_messages);
    http_add(&a,"POST /v1/messages?",gpt5_claude_messages);
    http_add(&a,"POST /api/v1/messages ",gpt5_claude_messages);

    http_add(&a,"POST /v1beta/models/",gpt5_gemini_generate_content);
    http_add(&a,"POST /api/v1beta/models/",gpt5_gemini_generate_content);

    http_add(&a,"POST /api/gpt5_apikey ",gpt5_apikey);
    http_add(&a,"POST /api/gpt5_resolve ",gpt5_resolve);
    http_add(&a,"POST /api/gpt5_history_list ",gpt5_history_list);
    http_add(&a,"POST /api/gpt5_history_get ",gpt5_history_get);
    http_add(&a,"POST /api/gpt5_history_rename ",gpt5_history_rename);
    http_add(&a,"POST /api/gpt5_history_delete ",gpt5_history_delete);
    http_add(&a,"POST /api/gpt5_share ",gpt5_share);
    http_add(&a,"GET /api/stop ",apistop);
    // http_add(&a,"",other);
    return http_start(&a,INADDR_LOOPBACK,1001);//INADDR_ANY
}
