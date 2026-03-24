#include"lib/http.h"
#include"user.h"
#include"chat.h"
#include"word.h"
#include"gptapi.h"
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
    http_add(&a,"GET /api/user ",apiuser);
    http_add(&a,"POST /api/change_password ",change_password);
    http_add(&a,"GET /api/check48 ",check48);
    http_add(&a,"POST /api/getword ",getword);
    http_add(&a,"POST /api/setword ",setword);
    http_add(&a,"POST /api/gpt2 ",gptapi2);
    http_add(&a,"POST /api/gpts2 ",gptapis2);
    // http_add(&a,"",other);
    return http_start(&a,1001);
}