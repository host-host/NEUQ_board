//g++ -O2 -Wall -Wextra code/unused/dbclean2.cpp build/ndb2.o build/mylib.o -o build/dbclean2 -lssl -lcrypto -lcurl -lz
#include "../lib/ndb2.h"
#include "../lib/mylib.h"
#include "../lib/cppJSON.h"
#include "../lib/gptapi5.h"
#include "../lib/gptapi6.h"
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
int main(){
    ndb2 content_db;//con_id -> content
    content_db=ndb2_init("/web/res/pri/gpt5content.ndb2");
    ndb2 c0=0;//ndb2_init("/web/res/pri/gpt5content2.ndb2");
    ndb2 c1=0;//ndb2_init("/web/res/pri/gpt5content3.ndb2");
    map<int,int>mp;
    char key[48]={0};
    for(content*p=(content*)ndb2_next(content_db,key);p;p=(content*)ndb2_next(content_db,key)){
        {
            // if(p->deleted==1)p->deleted=2;
        }
        mp[p->deleted]++;
        if(p->deleted==2)continue;
        if(p->deleted==1&&!c1)continue;
        if(p->deleted==0&&!c0)continue;
        int len=sizeof(content)+strlen(p->content)+1;
        content*q=(content*)ndb2_got(p->deleted==1?c1:c0,p->con_id,len);
        if(!q){
            cout<<"!!DB ERROR"<<endl;
            return 0;
        }
        memcpy(q,p,len);
    }
    for(auto i:mp)cout<<i.first<<" "<<i.second<<endl;
    return 0;
}