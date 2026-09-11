#include "admin.h"
#include "cppJSON.h"
#include "gptapi5.h"
#include "mylib.h"
#include "user.h"
#include <cctype>
#include <cmath>
#include <cstring>
#include <limits>
#include <queue>
#include <string>
#include <vector>
using namespace std;

struct admin_log_cursor{
    reslogs* logs;
    int index;
    string userid,username;
};
struct admin_log_cursor_less{
    bool operator()(const admin_log_cursor& a,const admin_log_cursor& b)const{
        const reslog& x=a.logs->a[a.index];
        const reslog& y=b.logs->a[b.index];
        if(x.time!=y.time)return x.time<y.time;
        if(x.start!=y.start)return x.start<y.start;
        if(a.userid!=b.userid)return a.userid<b.userid;
        return a.index<b.index;
    }
};
static bool admin_contains_ignore_case(const string& value,const string& query){
    if(query.empty())return true;
    if(query.size()>value.size())return false;
    for(size_t i=0;i+query.size()<=value.size();i++){
        size_t j=0;
        while(j<query.size()&&std::tolower((unsigned char)value[i+j])==std::tolower((unsigned char)query[j]))j++;
        if(j==query.size())return true;
    }
    return false;
}
static bool admin_user_matches(const string& userid,const string& username,const string& query){
    return admin_contains_ignore_case(userid,query)||admin_contains_ignore_case(username,query);
}
static cppJSON admin_log_json(const admin_log_cursor& cursor){
    const reslog& log=cursor.logs->a[cursor.index];
    cppJSON item("{}");
    item.insert("userid",cursor.userid);
    item.insert("user",cursor.username);
    item.insert("model",log.model);
    item.insert("provider",log.provider);
    item.insert("isimage",log.isimage==1);
    item.insert("used_tokens",(double)log.used_tokens);
    item.insert("input",(double)log.input);
    item.insert("output",(double)log.output);
    item.insert("cache",(double)log.cache);
    item.insert("makecache",(double)log.makecache);
    long long first,total;
    if(log.start>0){
        first=(log.first-log.start)/1000000000LL;
        total=(log.end-log.start)/1000000000LL;
    }else{
        first=log.first_deprecated;
        total=log.total_deprecated;
    }
    item.insert("first",(double)(first>0?first:0));
    item.insert("total",(double)(total>0?total:0));
    item.insert("multiply",log.multiply);
    item.insert("time",(double)log.time);
    item.insert("stable",(double)log.stable);
    item.insert("info",string(log.info,strnlen(log.info,sizeof(log.info))));
    return item;
}
void admin_log_list(http_para* a){
    user_* p=getuser(a->get);
    if(!p)return http_send(a,H401 Hjson Hc0,"{\"error\":{\"message\":\"Please log in first.\"}}",0);
    if(!(p->admin&2))return http_send(a,H403 Hjson Hc0,"{\"error\":{\"message\":\"Permission denied.\"}}",0);
    cppJSON req(a->get+a->n),ans("[]");
    long long start=req["start"].valuedouble(),end=req["end"].valuedouble();
    long long from=req["from"].valuedouble(),to=req["to"].valuedouble();
    string user_query=req["user"].valuestring();
    if(start<1||end<start||end-start>100||from<0||to<0||(from&&to&&from>to)||user_query.size()>64)
        return http_send(a,H400 Hjson Hc0,"{\"error\":{\"message\":\"Bad request.\"}}",0);
    priority_queue<admin_log_cursor,vector<admin_log_cursor>,admin_log_cursor_less> logs;
    char key[48]={0};
    for(reslogs* value=(reslogs*)ndb2_next(log_db,key);value;value=(reslogs*)ndb2_next(log_db,key)){
        long long size=ndb2_gotmaxlen(value);
        if(size<(long long)sizeof(reslogs))continue;
        long long capacity=(size-sizeof(reslogs))/sizeof(reslog);
        int n=value->n;
        if(n<=0||capacity<=0)continue;
        if(n>capacity)n=capacity;
        user_* owner=getuser_by_id(key);
        string username=owner?string(owner->name,strnlen(owner->name,sizeof(owner->name))):string();
        string userid=key;
        if(!admin_user_matches(userid,username,user_query))continue;
        int index=n-1;
        while(index>=0&&to&&value->a[index].time>to)index--;
        if(index<0||(from&&value->a[index].time<from))continue;
        logs.push({value,index,std::move(userid),std::move(username)});
    }
    for(long long position=1;position<=end&&!logs.empty();position++){
        admin_log_cursor current=logs.top();
        logs.pop();
        if(position>=start)ans.push_back(admin_log_json(current));
        if(current.index>0){
            current.index--;
            if(!from||current.logs->a[current.index].time>=from)logs.push(std::move(current));
        }
    }
    http_send(a,Hok Hjson Hc0,ans.stringify_Unformatted().c_str(),0);
}
void admin_add_token(http_para* a){
    user_* admin=getuser(a->get);
    if(!admin)return http_send(a,H401 Hjson Hc0,"{\"error\":{\"message\":\"Please log in first.\"}}",0);
    if(!(admin->admin&2))return http_send(a,H403 Hjson Hc0,"{\"error\":{\"message\":\"Permission denied.\"}}",0);
    cppJSON req(a->get+a->n);
    string userid=req["userid"].valuestring(),mode=req["mode"].valuestring();
    double amount=req["amount"].valuedouble();
    if(userid.size()!=8)return http_send(a,H400 Hjson Hc0,"{\"error\":{\"message\":\"Invalid user ID.\"}}",0);
    if(mode!="convert"&&mode!="direct")return http_send(a,H400 Hjson Hc0,"{\"error\":{\"message\":\"Invalid adjustment mode.\"}}",0);
    long long add_value=0;
    if(mode=="convert")add_value=amount/0.3*1000000.0;
    if(mode=="direct")add_value=amount;
    user_* target=getuser_by_id(userid.c_str());
    if(!target)return http_send(a,H404 Hjson Hc0,"{\"error\":{\"message\":\"User not found.\"}}",0);
    ADD(&target->token_limit,add_value);
    cppJSON ans("{}");
    ans.insert("userid",userid);
    ans.insert("user",target->name);
    ans.insert("added",(double)add_value);
    ans.insert("token_limit",(double)target->token_limit);
    ans.insert("token_used",(double)target->token_used);
    http_send(a,Hok Hjson Hc0,ans.stringify_Unformatted().c_str(),0);
}
