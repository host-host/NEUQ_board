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
    map<string,point>mp;
    char key[48]={0};
    for(content*p=(content*)ndb2_next(content_db,key);p;p=(content*)ndb2_next(content_db,key)){
        mp[key].p=(content*)p;
        cppJSON a(p->content);
        a=my_format(a,(string)p->format);
        if(!a.size()||!a.IsArray()){
            cout<<"ERROR!! 2 "<<p->con_id<<' '<<p->content<<endl;
            p->deleted=2;
            continue;
        }
        for(cppJSON i:a){
            string p=i.stringify_Unformatted();
            mp[key].v.push_back(s256(p));
        }
    }
    cout<<"ALL"<<mp.size()<<endl;
    int cnt=0;
    for(auto i:mp){
        if(++cnt%10==0)cout<<"WORK on "<<cnt<<endl;
        content *p=i.second.p;
        if(p->deleted==2)continue;
        vector<string>&v=i.second.v;
        for(auto j:mp){
            content *q=j.second.p;
            if(p==q)continue;
            if(q->deleted==2)continue;
            if(strcmp(q->ownerid,p->ownerid))continue;
            if(strcmp(q->format,p->format))continue;
            vector<string>&w=j.second.v;
            if(w.size()<v.size())continue;
            int v1=0;
            for(string tmp:w){
                if(tmp==v[v1])if(++v1==(int)v.size())break;
            }
            if(v1==(int)v.size()){
                p->deleted=2;//////////////////
                cout<<"DELETE "<<p->con_id<<endl;
                break;
            }
        }
    }
    return 0;
}