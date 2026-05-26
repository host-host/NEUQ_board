#include<iostream>
#include"lib/ndb.h"
#include"lib/user.h"
#include"lib/gptapi2.h"
using namespace std;
void test1(){
    ndb user;
	ndb_init(&user,"./res/pri/user.dat",8,0x100000000);
    user_* a;
    // char tmp[]="zRdvHNcf";
    // a=(user_*)ndb_create_binary(&user,tmp,0);
    // printf("%s:%s %s\n",tmp,a->name,a->email);
    // a->admin=1;
    char tmp[16]={0};
    while(1){
        a=(user_*)ndb_next(&user,tmp);
        if(a==0)break;
        printf("%s:%s %d\n",tmp,a->name,a->admin);
    }
}

extern ndb gpt_con;  // content_id->gpt_content
extern ndb gpt_user; // char[24]->gpt_userhistory
int main() {
    char c[24]={0};
    gpt_content*p=(gpt_content*)ndb_next(&gpt_con,c);
    printf("%d\n",p->publish);
    printf("%s\n",p->owner);
    printf("%d\n",p->createtime);
    printf("%s\n",p->content);
    // printf("%s",c);
    // gpt_userhistory*p=(gpt_userhistory*)ndb_create_binary(&gpt_user,c,0);
    // printf("%s",p->content[0].a);
    return 0;
}