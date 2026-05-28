#include<iostream>
#include"lib/ndb.h"
#include"lib/user.h"
#include"lib/gptapi2.h"
using namespace std;
void test1(){
    ndb user;
	ndb_init(&user,"/web/res/pri/user.dat",8,0x100000000);
    user_* a;
    // char tmp[]="tWEEYlnH";
    // a=(user_*)ndb_create_binary(&user,tmp,0);
    // printf("%s:%s %s\n",tmp,a->name,a->email);
    // a->admin=1;
    char tmp[16]={0};
    while(1){
        a=(user_*)ndb_next(&user,tmp);
        if(a==0)break;
        if(a->admin)
        printf("%s:%s %d\n",tmp,a->name,a->admin);
    }
}
void test2(){
extern ndb gpt_con;  // content_id->gpt_content
// extern ndb gpt_user; // char[24]->gpt_userhistory
    char c[64]={0};
    gpt_content*p=(gpt_content*)ndb_next(&gpt_con,c);
    while(1){
        p=(gpt_content*)ndb_next(&gpt_con,c);
        if(p==0)break;
        printf("%s\n",p->owner);
    }
    // printf("%s",c);
    // gpt_userhistory*p=(gpt_userhistory*)ndb_create_binary(&gpt_user,c,0);
    // printf("%s",p->content[0].a);
}

int main() {
    test2();
    return 0;
}