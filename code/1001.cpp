#include"lib/http.h"
#include"lib/user.h"
#include"lib/chat.h"
#include"lib/check48.h"
#include"lib/gptapi.h"
// #include"lib/word.h"
#include"lib/gptapi.h"
#include"lib/gptapi2.h"
#define LOG(a,...) printf("<%s : %d>%s : " a "\n",__FILE__,__LINE__,__func__,##__VA_ARGS__)
void other(http_para* a){
    LOG("%s\n",a->get);
}
int main() {
    http a;
    http_init(&a);
    http_add(&a,"GET /api/p=",getp);
    http_add(&a,"GET /api/con=",getcon);
    http_add(&a,"POST /api/sendmessage",sendmessage);
    http_add(&a,"POST /api/login ",login);
    http_add(&a,"POST /api/register ",reg);
    http_add(&a,"GET /api/logout ",logout);
    http_add(&a,"POST /api/logout ",logout);
    http_add(&a,"GET /api/user ",apiuser);
    http_add(&a,"POST /api/change_password ",change_password);
    http_add(&a,"GET /api/check48 ",check48);
    // http_add(&a,"POST /api/getword ",getword);
    // http_add(&a,"POST /api/setword ",setword);
    http_add(&a,"POST /api/gpt2 ",gptapi2);
    http_add(&a,"POST /api/gpts2 ",gptapis2);
    http_add(&a,"POST /api/gpt_gotid ",gpt_gotid);
    http_add(&a,"POST /api/gpt_askid ",gpt_askid);
    http_add(&a,"POST /api/gpt_idname ",gpt_idname);
    http_add(&a,"POST /api/gpt_changename ",gpt_changename);
    http_add(&a,"POST /api/gpt_idcontent ",gpt_idcontent);
    http_add(&a,"POST /api/gpt_getuserhistory ",gpt_getuserhistory);
    http_add(&a,"POST /api/gpt_deletehistory ",gpt_deletehistory);
    http_add(&a,"POST /api/gpt_share ",gpt_share);
    // http_add(&a,"",other);
    return http_start(&a,1001);
}