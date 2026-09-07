/**
 * 临时代码，不进入git
 */
#include "lib/ndb2.h"
#include "lib/mylib.h"
#include "lib/cppJSON.h"
#include "lib/gptapi5.h"
#include "lib/gptapi6.h"
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <dirent.h>
#include <string>
#include <unistd.h>
#include <vector>
#include<map>
#include<iostream>
using namespace std;
#define ll long long
string s256(string u){
    char p[60];
    mylib_sha256(u.data(),u.size(),p);
    return (string)p;
}
struct point{
    content*p;
    vector<string>v;
};
int main(){
    ndb2 content_db;//con_id -> content
    content_db=ndb2_init("/web/res/pri/gpt5content.ndb2");
    ndb2 c2=ndb2_init("/web/res/pri/gpt5content2.ndb2");
    char key[48]={0};
    for(content*p=(content*)ndb2_next(content_db,key);p;p=(content*)ndb2_next(content_db,key)){
        if(p->deleted==2)continue;
        int len=sizeof(content)+strlen(p->content)+1;
        content*q=(content*)ndb2_got(c2,p->con_id,len);
        if(!q){
            cout<<"!!DB ERROR"<<endl;
            return 0;
        }
        memcpy(q,p,len);
    }
    return 0;
}