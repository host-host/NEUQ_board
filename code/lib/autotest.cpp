#include "autotest.h"
#include "cppJSON.h"
#include "gptapi6.h"
#include "mylib.h"
#include "ndb2.h"
#include "user.h"
#include <pthread.h>
#include <sys/stat.h>
#include <unistd.h>
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <fstream>
#include <string>
using namespace std;
#define CONFIG "/web/res/pri/gpt4.json"
#define QUESTION "/web/res/pri/gpt5.json"
#define LOGDIR "/web/log/autotest"
#define DENY(H,message) http_send(a,H Hjson Hc0,"{\"error\":{\"message\":\"" message "\"}}",0)
struct autotest_item{
    double vtime;//测试间隔（小时），0不测
    long long last;//question非空：上次测试时间；question为空：该渠道上次用户请求成功时间
    int blocked;//仅question为空时使用：auto不再走该渠道
};
static ndb2 autotest_db;//sha256(model\nprovider\nquestion) -> autotest_item
static autotest_item* got(const string& model,const string& provider,const string& question,bool create){
    string s=model+"\n"+provider+"\n"+question;
    char key[48]={0};
    mylib_sha256(s.data(),s.size(),key);
    return (autotest_item*)ndb2_got(autotest_db,key,create?sizeof(autotest_item):0);
}
bool autotest_blocked(const char* model,const char* provider){
    autotest_item* p=got(model,provider,"",0);
    return p&&p->blocked;
}
void autotest_userok(const char* model,const char* provider){
    autotest_item* p=got(model,provider,"",1);
    if(p)p->last=time(0);
}
static string dayfile(time_t t){
    struct tm lt;
    char day[32]={0};
    localtime_r(&t,&lt);
    strftime(day,sizeof(day),"/%Y-%m-%d.jsonl",&lt);
    return (string)LOGDIR+day;
}
static void test(const string& model,const string& provider,const cppJSON& q,const cppJSON& conf){
    cppJSON req=q["question"].clone(),rec("{}");
    req.insert("model",model);
    string format=q["format"];
    if(format.empty())format="completions";
    long long start=gpt6_now_ns();
    gpt6_ret r=gpt6_work3(0,req.stringify_Unformatted().c_str(),model.c_str(),conf,format.c_str());
    rec.insert("time",(double)time(0));
    rec.insert("model",model);
    rec.insert("provider",provider);
    rec.insert("name",q.namestring());
    rec.insert("question",std::move(req));
    rec.insert("append",r.append);
    if(q["ans"].IsString()){
        rec.insert("ans",q["ans"].valuestring());
        rec.insert("match",r.append.stringify_Unformatted().find(q["ans"].valuestring())!=string::npos);
    }
    rec.insert("stable",(double)r.stable);
    rec.insert("httpcode",(double)r.httpcode);
    rec.insert("info",r.info);
    rec.insert("output",(double)r.output);
    if(r.first_ns)rec.insert("first",(r.first_ns-start)/1000000000.0);
    rec.insert("total",(r.end_ns-start)/1000000000.0);
    mkdir(LOGDIR,0700);
    FILE* f=fopen(dayfile(time(0)).c_str(),"a");
    if(f){
        string s=rec.stringify_Unformatted()+"\n";
        fwrite(s.data(),1,s.size(),f);
        fclose(f);
    }
}
static void* loop(void*){
    for(;;sleep(30)){
        cppJSON config=cppJSON::from_file(CONFIG),qs=cppJSON::from_file(QUESTION);
        if(!config||!qs)continue;
        for(cppJSON m:config["model"]){
            if(!m["auto"])continue;
            string model=m.namestring();
            for(cppJSON pv:config["model"][model]["all"]){
                string provider=pv;
                cppJSON conf=config["provider"][provider];
                if(!conf)continue;
                for(cppJSON q:qs){
                    autotest_item* it=got(model,provider,q.namestring(),0),*ch=got(model,provider,"",0);
                    if(!it||!(it->vtime>0))continue;
                    long long v=it->vtime*3600,next=max(it->last,ch?ch->last:0)+v;//用户请求成功会顺延，但不超过2*vtime
                    if(time(0)<min(next,it->last+2*v))continue;
                    test(model,provider,q,conf);
                    it->last=time(0);
                }
            }
        }
    }
    return 0;
}
void autotest_init(){
    autotest_db=ndb2_init("/web/res/pri/gpt5autotest.ndb2");
    pthread_t id;
    if(!pthread_create(&id,0,loop,0))pthread_detach(id);
    else exit(-98);
}
void autotest_get(http_para* a){//{"model":""} -> {"question":[],"state":{provider:{"blocked":bool,"vtime":{question:小时}}},"record":[]}
    user_* p=getuser(a->get);
    if(!p)return DENY(H401,"Please log in first.");
    if(!(p->admin&2))return DENY(H403,"Permission denied.");
    cppJSON config=cppJSON::from_file(CONFIG),qs=cppJSON::from_file(QUESTION);
    cppJSON ans("{}"),names("[]"),state("{}"),record("[]");
    string model=cppJSON(a->get+a->n)["model"];
    for(cppJSON q:qs)names.push_back(q.namestring());
    for(cppJSON pv:config["model"][model]["all"]){
        string provider=pv;
        cppJSON s("{}"),vt("{}");
        s.insert("blocked",autotest_blocked(model.c_str(),provider.c_str()));
        for(cppJSON q:qs){
            autotest_item* it=got(model,provider,q.namestring(),0);
            vt.insert(q.namestring().c_str(),it?it->vtime:0.0);
        }
        s.insert("vtime",std::move(vt));
        state.insert(provider.c_str(),std::move(s));
    }
    time_t now=time(0);
    for(int d=3;d>=0;d--){//最近3天
        ifstream fin(dayfile(now-d*86400));
        for(string line;getline(fin,line);){
            cppJSON rec(line.c_str());
            if(rec["model"]==model&&rec["time"].valuedouble()>=now-72*3600)record.push_back(std::move(rec));
        }
    }
    ans.insert("question",std::move(names));
    ans.insert("state",std::move(state));
    ans.insert("record",std::move(record));
    http_send(a,Hok Hjson Hc0,ans.stringify_Unformatted().c_str(),0);
}
void autotest_set(http_para* a){//{"model":"","provider":"","blocked":bool,"vtime":{question:小时}}，blocked和vtime均可省略
    user_* p=getuser(a->get);
    if(!p)return DENY(H401,"Please log in first.");
    if(!(p->admin&2))return DENY(H403,"Permission denied.");
    cppJSON req(a->get+a->n);
    string model=req["model"],provider=req["provider"];
    if(model.empty()||provider.empty())return DENY(H400,"Bad request.");
    if(req["blocked"].IsBool()){
        autotest_item* ch=got(model,provider,"",1);
        if(ch)ch->blocked=req["blocked"]==true;
    }
    for(cppJSON v:req["vtime"]){
        if(!v.IsNumber()||!(v.valuedouble()>=0))continue;
        autotest_item* it=got(model,provider,v.namestring(),1);
        if(it)it->vtime=v.valuedouble();
    }
    http_send(a,Hok Hjson Hc0,"{\"ok\":true}",0);
}
