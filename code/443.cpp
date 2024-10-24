/*
g++ -O2 -Wall -o /code/443 /code/443.cpp -lssl
*/
#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<sys/types.h>
#include<sys/mman.h>
#include<sys/socket.h>
#include<netinet/in.h>
#include<openssl/ssl.h>
#include<signal.h>
#include<unistd.h>
#include<fcntl.h>
#include<errno.h>
#include<pthread.h>
#include<string>
#include<map>
#include<vector>
#include<cmath>
#include<vector>
using std::string;
#include"https.h"
#include"ndb.h"
#include"user.h"
#include"chat.h"
#include"word.h"
#include"sendfile.h"
int main() {
    https a;
    if(https_init(&a,"/443/pri/fullchain.crt","/443/pri/private.pem"))return 1;
    https_add(&a,"GET /api/p=",getp);
    https_add(&a,"GET /api/con=",getcon);
    https_add(&a,"POST /api/sendmessage",sendmessage);
    https_add(&a,"POST /api/login ",login);
    https_add(&a,"POST /api/register ",reg);
    https_add(&a,"GET /api/logout ",logout);
    https_add(&a,"GET /api/user ",apiuser);
    https_add(&a,"POST /api/change_password ",change_password);
    https_add(&a,"GET /api/check48 ",check48);
    https_add(&a,"POST /getword ",getword);
    https_add(&a,"POST /setword ",setword);
    https_add(&a,"GET ",sendfile);
    return https_start(&a,999);
}