//g++ -O2 -Wall -Wextra code/unused/dbclean.cpp build/ndb2.o build/mylib.o -o build/dbclean -lssl -lcrypto -lcurl -lz
//需独占gpt5content！！！！
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
    map<int,int>mp;
    char key[48]={0};
    for(content*p=(content*)ndb2_next(content_db,key);p;p=(content*)ndb2_next(content_db,key)){
        mp[p->deleted]++;
        if(p->deleted==2)ndb2_del(content_db,key);
    }
    ndb2_free(content_db);
    content_db=ndb2_init_freespace("/web/res/pri/gpt5content.ndb2");
    for(auto i:mp)cout<<i.first<<" "<<i.second<<endl;
    return 0;
}