#include"lib/http.h"
#include"lib/user.h"
#include"lib/chat.h"
#include"lib/check48.h"
// #include"lib/word.h"
#include"lib/gptapi3.h"
#include"lib/mylib.h"
#include<cstdio>
#include<cstring>
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
    http_add(&a,"POST /api/gpt_chat ",gpt_chat);
    http_add(&a,"POST /api/gpts2 ",gptapis2);
    http_add(&a,"POST /api/gpt_idname ",gpt_idname);
    http_add(&a,"POST /api/gpt_changename ",gpt_changename);
    http_add(&a,"POST /api/gpt_idcontent ",gpt_idcontent);
    http_add(&a,"POST /api/gpt_getuserhistory ",gpt_getuserhistory);
    http_add(&a,"POST /api/gpt_deletehistory ",gpt_deletehistory);
    http_add(&a,"POST /api/gpt_share ",gpt_share);
    http_add(&a,"GET /api/stop ",apistop);
    // http_add(&a,"",other);
    return http_start(&a,INADDR_LOOPBACK,1001);//INADDR_ANY
}
