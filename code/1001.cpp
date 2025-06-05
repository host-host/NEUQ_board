/*
g++ -O2 -Wall -o 1001 ./code/1001.cpp -lssl -lcrypto -lcurl -Wall
*/

#include"http.h"
#include"user.h"
#include"chat.h"
#include"word.h"
#include"gptapi.h"
// void other(http_para* a){
//     LOG("%s\n",a->get);
// }
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
    http_add(&a,"POST /api/gpt ",gptapi);
    http_add(&a,"POST /api/gpt/chat/completions ",gptapi);
    http_add(&a,"POST /api/gpts ",gptapis);
    // http_add(&a,"",other);
    return http_start(&a,1001);
}
/*


getuser : 
POST /api/gpt HTTP/1.1
Host: www.neuqboard.cn
X-Real-IP: 183.199.68.49
X-Forwarded-For: 183.199.68.49
X-Forwarded-Proto: https
Connection: close
Content-Length: 105
sec-ch-ua-platform: "Windows"
user-agent: Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/131.0.0.0 Safari/537.36 Edg/131.0.0.0
sec-ch-ua: "Microsoft Edge";v="131", "Chromium";v="131", "Not_A Brand";v="24"
content-type: application/json
sec-ch-ua-mobile: ?0
accept: *//*
origin: https://www.neuqboard.cn
sec-fetch-site: same-origin
sec-fetch-mode: cors
sec-fetch-dest: empty
referer: https://www.neuqboard.cn/gpt
accept-encoding: gzip, deflate, br, zstd
accept-language: zh-CN,zh;q=0.9,en;q=0.8,en-GB;q=0.7,en-US;q=0.6
priority: u=1, i
cookie: id=EFShBBxEOPX

{"model":"TA/meta-llama/Meta-Llama-3.1-8B-Instruct-Turbo","messages":[{"role":"user","content":"hello"}]}
 */