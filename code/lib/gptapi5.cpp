#include "gptapi5.h"
#include "cppJSON.h"
#include "gptapi6.h"
#include "mylib.h"
#include "ndb2.h"
#include "user.h"
#include "http.h"
#include <pthread.h>
#include <sys/stat.h>
#include <time.h>
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <set>
#include <string>
using namespace std;
#define CONFIG "/web/res/pri/gpt2.json"
#define ll long long
ndb2 content_db;//con_id -> content
ndb2 index_db;//sha256(response_id) -> con_id
ndb2 history_db;//user_id -> history
static map<string,int> gpt5_api_is_using;
static pthread_mutex_t gpt5_api_mutex=PTHREAD_MUTEX_INITIALIZER;
struct content{
    bool publish;
    bool deleted;
    bool isusing;
    char ownerid[10];
    char ownername[24];
    ll createtime;
    ll updatetime;
    char name[64];
    char format[20];
    char con_id[32];
    char other[1024];
    char content[0];
};
struct history{
    int n;
    char user_id[10];
    char con_id[][32];
};
__attribute((constructor)) void gptapi5_init() {
    content_db=ndb2_init("/web/res/pri/gpt5content.ndb2");
    index_db=ndb2_init("/web/res/pri/gpt5sha256.ndb2");
    history_db=ndb2_init("/web/res/pri/gpt5userhistory.ndb2");
}
void gpt5_apikey(http_para* a) {
    user_* p=getuser(a->get);
    if(!p)return my_http_error(a,"Please log in first.");
    bool rotate=cppJSON(a->get+a->n)["rotate"]==true;
    if(!p->gptapikey[0]||rotate)mylib_random_string(p->gptapikey,19);
    cppJSON ans("{}");
    ans.insert("api_key",(string)"sk-"+p->userid+p->gptapikey);
    http_send(a,Hok Hjson Hc0,ans.stringify_Unformatted().c_str(),0);
}
void gpt5_resolve(http_para* a) {
    cppJSON re(a->get+a->n);
    string id=re["response_id"];
    if(id.empty())return my_http_error(a,"response_id is required.");
    id="response_id_"+id;
    char c[48];
    mylib_sha256(id.c_str(),id.size(),c);
    char* con_id=(char*)ndb2_got(index_db,c,0);
    if(!con_id)return my_http_error(a,"Response not found.");
    cppJSON response("{}");
    response.insert("con_id",con_id);
    http_send(a,Hok Hjson Hc0,response.stringify_Unformatted().c_str(),0);
}
void gpt5_history_list(http_para* a) {
    user_* p=getuser(a->get);
    if(!p)return my_http_error(a,"Please log in first.");
    history*h=(history*)ndb2_got(history_db,p->userid,0);
    if(!h)return http_send(a,Hok Hjson Hc0,"[]",0);
    cppJSON ans("[]");
    for(int i=0;i<h->n;i++){
        content* con=(content*)ndb2_got(content_db,h->con_id[i],0);
        if(!con||con->deleted)continue;
        cppJSON item("{}");
        item.insert("con_id",con->con_id);
        item.insert("name",con->name);
        ans.push_back(std::move(item));
    }
    return http_send(a,Hok Hjson Hc0,ans.stringify_Unformatted().c_str(),0);
}
void gpt5_history_get(http_para* a) {
    user_* p=getuser(a->get);
    string con_id=cppJSON(a->get+a->n)["con_id"];
    if(con_id.empty())return my_http_error(a,"con_id is required.");
    content* con=(content*)ndb2_got(content_db,con_id.c_str(),0);
    if(!con)return my_http_error(a,"conversation not found.");
    if(!con->publish&&(!p||strcmp(con->ownerid,p->userid)!=0))return my_http_error(a,"Permission denied.");
    cppJSON ans("{}");
    ans.insert("ownername",con->ownername);
    ans.insert("isusing",con->isusing);
    ans.insert("name",con->name);
    ans.insert("format",con->format);
    ans.insert("con_id",con->con_id);
    ans.insert("content",cppJSON(con->content));
    return http_send(a,Hok Hjson Hc0,ans.stringify_Unformatted().c_str(),0);
}
void gpt5_history_rename(http_para* a) {
    user_* p=getuser(a->get);
    if(!p)return my_http_error(a,"Please log in first.");
    cppJSON request(a->get+a->n);
    string con_id=request["con_id"],title=request["title"];
    if(con_id.empty())return my_http_error(a,"con_id is required.");
    if(title.empty())return my_http_error(a,"title is required.");
    if(title.size()>=sizeof(((content*)0)->name))return my_http_error(a,"title is too long.");
    content* con=(content*)ndb2_got(content_db,con_id.c_str(),0);
    if(!con)return my_http_error(a,"conversation not found.");
    if(strcmp(con->ownerid,p->userid)!=0)return my_http_error(a,"Permission denied.");
    memset(con->name,0,sizeof(con->name));
    memcpy(con->name,title.data(),title.size());
    http_send(a,Hok Hjson Hc0,"{\"ok\":true}",0);
}
void gpt5_history_delete(http_para* a) {
    user_* p=getuser(a->get);
    if(!p)return my_http_error(a,"Please log in first.");
    string con_id=cppJSON(a->get+a->n)["con_id"];
    if(con_id.empty())return my_http_error(a,"con_id is required.");
    content* con=(content*)ndb2_got(content_db,con_id.c_str(),0);
    if(!con)return my_http_error(a,"conversation not found.");
    if(strcmp(con->ownerid,p->userid)!=0)return my_http_error(a,"Permission denied.");
    con->deleted=true;
    history* h=(history*)ndb2_got(history_db,p->userid,0);
    if(h){
        int l=0;
        for(int i=0;i<h->n;i++){
            if(strncmp(h->con_id[i],con_id.c_str(),sizeof(h->con_id[i]))==0)continue;
            if(l!=i)memcpy(h->con_id[l],h->con_id[i],sizeof(h->con_id[l]));
            l++;
        }
        h->n=l;
    }
    http_send(a,Hok Hjson Hc0,"{\"ok\":true}",0);
}
void gpt5_share(http_para* a) {
    user_* p=getuser(a->get);
    if(!p)return my_http_error(a,"Please log in first.");
    cppJSON request(a->get+a->n);
    string con_id=request["con_id"];
    if(con_id.empty())return my_http_error(a,"con_id is required.");
    if(!(request["publish"]==true))return my_http_error(a,"publish must be true.");
    content* con=(content*)ndb2_got(content_db,con_id.c_str(),0);
    if(!con)return my_http_error(a,"conversation not found.");
    if(strcmp(con->ownerid,p->userid)!=0)return my_http_error(a,"Permission denied.");
    con->publish=true;
    http_send(a,Hok Hjson Hc0,"{\"ok\":true}",0);
}
static string gpt5_take_auth(const cppJSON& entry) {
    int parallel=(int)entry["parallel"].valuedouble();
    if(parallel<=0)parallel=1;
    pthread_mutex_lock(&gpt5_api_mutex);
    for(cppJSON i:entry["Authorization"]){
        string auth=i;
        if(!auth.empty()&&gpt5_api_is_using[auth]<parallel){
            gpt5_api_is_using[auth]++;
            pthread_mutex_unlock(&gpt5_api_mutex);
            return auth;
        }
    }
    pthread_mutex_unlock(&gpt5_api_mutex);
    return string();
}
static void gpt5_release_auth(const string& auth) {
    if(auth.empty())return;
    pthread_mutex_lock(&gpt5_api_mutex);
    gpt5_api_is_using[auth]--;
    pthread_mutex_unlock(&gpt5_api_mutex);
}
static std::string utf8_substr(const std::string& str, unsigned int max_bytes) {
    if (str.length() <= max_bytes) return str;
    unsigned int len = max_bytes;
    while(len > 0 && (str[len] & 0xC0) == 0x80) len--;
    return str.substr(0, len) + "...";
}
#define LOCK(a) while(__sync_val_compare_and_swap(a,0,1))usleep(0)
void insert2index_db(const string&a,const string&b){
    char tmp[48]={0};
    mylib_sha256(a.c_str(),a.length(),tmp);
    char* p=(char*)ndb2_got(index_db,tmp,b.length()+10);
    if(p)memcpy(p,b.c_str(),b.length());
}
static void gpt5_completion_request(http_para* a,const string& format,const char* array_name) {
    char *tmp=strcasestr(a->get,"Authorization: Bearer sk-");
    if(tmp)tmp+=25;
    user_* p=getuser_by_id(tmp);
    if(p&&memcmp(tmp+8,p->gptapikey,19))p=0;
    if(!p)return my_http_error(a,"Invalid API key.");
    cppJSON request(a->get+a->n);
    string model=request["model"];
    cppJSON input=my_format(request[array_name]),config=cppJSON::from_file(CONFIG),conf;
    if(!config)return my_http_error(a,"can not read gpt2.json.");
    for(cppJSON i:config){
        string config_format=i["format"];
        if(config_format.empty())config_format="completions";
        if(config_format!=format)continue;
        if(i["public"].has(model.c_str())||(p->admin&&i["private"].has(model.c_str()))){
            conf=i;
            break;
        }
    }
    if(!conf)return my_http_error(a,"Permission denied.");
    string auth=gpt5_take_auth(conf);
    if(auth.empty())return my_http_error(a,"Service busy.");
    cppJSON previous_new_input=input.clone();
    string hash;
    char hash_[48],*con_id=0;
    content*con=0;
    for(int i=previous_new_input.size()-1,j=0;i&&(++j)<15;i--){
        if(previous_new_input[i]["role"]=="assistant")break;
        previous_new_input.erase(i);
        hash="new_input_"+previous_new_input.stringify_Unformatted();
        mylib_sha256(hash.c_str(),hash.length(),hash_);
        char* candidate_id=(char*)ndb2_got(index_db,hash_,0);
        content* candidate=(content*)ndb2_got(content_db,candidate_id,0);
        string candidate_hash=candidate?(string)"new_input_"+candidate->content:string();
        if(candidate_hash==hash&&candidate){
            if(!strcmp(candidate->format,format.c_str())&&!memcmp(candidate->ownerid,p->userid,8)){
                con_id=candidate_id;
                con=candidate;
            }
            break;
        }
    }
    bool isnew=false;
    if(!con||strcmp(con->format,format.c_str())||memcmp(con->ownerid,p->userid,8)!=0)isnew=true;
    else{
        int o1=__sync_val_compare_and_swap(&con->isusing,0,1);
        if(o1)isnew=true;
        else{
            string hash2=(string)"new_input_"+con->content;
            if(hash2!=hash){
                con->isusing=0;
                isnew=true;
            }
        }
    }
    if(isnew){
        con_id=(char*)malloc(32);
        memset(con_id,0,32);
        mylib_random_string(con_id,20);
        con=(content*)ndb2_got(content_db,con_id,sizeof(content)+10);
        if(!con)return my_http_error(a,"content_db error");
        memset(con,0,sizeof(content)+3);
        memcpy(con->ownerid,p->userid,8);
        memcpy(con->ownername,p->name,min(strlen(p->name),sizeof(con->ownername)-1));
        con->createtime=con->updatetime=time(0);
        memcpy(con->format,format.data(),min(format.size(),sizeof(con->format)-1));
        memcpy(con->con_id,con_id,strlen(con_id));
        memcpy(con->content,"[]",3);
        con->isusing=true;
        history* h=(history*)ndb2_got(history_db,p->userid,0);
        int n=h?h->n+1:1;
        h=(history*)ndb2_got(history_db,p->userid,sizeof(history)+32*n);
        if(h){
            h->n=n;
            if(n==1)memcpy(h->user_id,p->userid,8);
            memcpy(h->con_id[n-1],con_id,32);
        }
    }
    string response_id;
    cppJSON output=gpt6_work(a,conf["url"],auth,(string)(a->get+a->n),format,response_id);
    insert2index_db("response_id_"+response_id,con_id);
    gpt5_release_auth(auth);
    if(!con->name[0]){//此处已确保配置文件第一个是用于取标题的 api。并且是completions格式
        cppJSON title_request("{\"stream\":false}");
        title_request.insert("model",config[0]["public"][0].valuestring());
        cppJSON title_messages("[]");
        cppJSON title_message("{\"role\":\"user\"}");
        string prompt="将以下回复的内容取一个简短的标题：\n"+output.stringify_Unformatted();
        title_message.insert("content",prompt.c_str());
        title_messages.push_back(std::move(title_message));
        title_request.insert("messages",std::move(title_messages));
        string _;
        cppJSON title_reply=gpt6_work(0,config[0]["url"],config[0]["Authorization"][0],title_request.stringify_Unformatted(),"completions",_);
        string title=title_reply[0]["content"];
        if(title.size()>4&&title.substr(0,2)=="**"&&title.substr(title.size()-2)=="**")title=title.substr(2,title.size()-4);
        if(title.size()>60)title=utf8_substr(title,56)+"...";
        if(!title.empty())strcpy(con->name,title.c_str());
    }
    for(cppJSON i:output)input.push_back(my_format(i));
    string new_input=input.stringify_Unformatted();
    insert2index_db("new_input_"+new_input,con_id);
    con=(content*)ndb2_got(content_db,con_id,sizeof(content)+new_input.length()+10);
    if(con){
        memcpy(con->content,new_input.c_str(),new_input.length()+1);
        con->isusing=0;
    }
    if(isnew)free(con_id);
}
void gpt5_responses(http_para* a) {
    gpt5_completion_request(a,"responses","input");
}
void gpt5_chat_completions(http_para* a) {
    gpt5_completion_request(a,"completions","messages");
}
