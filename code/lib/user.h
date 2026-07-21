#ifndef USER_H
#define USER_H
#include"http.h"
#ifdef __cplusplus
extern "C"{
#endif

typedef struct {
	char name[24],pwd[24],cookie_rand[8];
	int admin,time;
	char email[80],phone[20],gptapikey[20];
}user_;
void user_init();
user_* getuser(const char* get);
void login(http_para* ssl);
void reg(http_para* ssl);
void logout(http_para *ssl);
void apiuser(http_para* ssl);
void uploads_file(http_para* ssl);
void download_file(http_para* ssl);
void change_password(http_para* ssl);

#ifdef __cplusplus
}
#endif
#endif
