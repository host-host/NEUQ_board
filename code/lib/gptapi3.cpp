#include"gptapi3.h"
#include"gptapi4.h"
#include"user.h"
#include"ndb2.h"
#include"cppJSON.h"
#include<curl/curl.h>
#include<pthread.h>
#include<cstdlib>
#include<cstring>
#include<map>
#include<string>
using namespace std;
ndb2 gpt_con;
ndb2 gpt_user;
map<string,int>api_is_using;
pthread_mutex_t mutex_api_is_using=PTHREAD_MUTEX_INITIALIZER;
__attribute((constructor)) static void gptapi3_init() {
    curl_global_init(CURL_GLOBAL_DEFAULT);
    gpt_con=ndb2_init("/web/res/pri/gpt_content.ndb2");
    gpt_user=ndb2_init("/web/res/pri/gpt_userhistory.ndb2");
}
static string gpt3_new_id() {
    char buf[32]={0};
    for(int i=0;i<31; i++) {
        int v=rand()%36;
        buf[i]=v<10?'0'+v:'a'+v-10;
    }
    return string(buf);
}
static void gpt3_add_history(const string& username,const string& id) {
    gpt_userhistory* uh=(gpt_userhistory*)ndb2_got(gpt_user, username.c_str(), 0);
    int n=uh?uh->n:0;
    long long new_size=sizeof(gpt_userhistory)+(n+1)*sizeof(content_id);
    uh=(gpt_userhistory*)ndb2_got(gpt_user,username.c_str(),new_size);
    if(!uh)return;
    if(n==0)strcpy(uh->user,username.c_str());
    strcpy(uh->content[n].a,id.c_str());
    uh->n=n+1;
}
static bool gpt3_same_messages(const cppJSON& old_,const cppJSON& new_) {
    cppJSON old=old_.child(),now=new_.child();
    for(; old; old=old.next(),now=now.next())if(!now||old.stringify_Unformatted()!=now.stringify_Unformatted())return false;
    return true;
}
static bool gpt3_model_in_array(const cppJSON& array,const string& model) {
    if(!array.IsArray())return false;
    for(cppJSON item=array.child(); item; item=item.next())if(item==model)return true;
    return false;
}
void gptapis2(http_para* a) {
    char* tmp=getgpt2json();
    cppJSON config(tmp);
    if(tmp)free(tmp);
    if(!config)return http_send(a,Hok Hc0 Htxt,"can not read gpt2.json.",0);
    for(cppJSON p=config.child(); p; p=p.next()) {
        p.erase("url");
        p.erase("Authorization");
    }
    http_send(a,Hok Hjson Hc0,config.stringify_Unformatted().c_str(),0);
}
static std::string utf8_substr(const std::string& str, unsigned int max_bytes) {
    if (str.length() <= max_bytes) return str;
    unsigned int len = max_bytes;
    while(len > 0 && (str[len] & 0xC0) == 0x80) len--;
    return str.substr(0, len) + "...";
}
void gpt_chat(http_para* a) {
    user_* puser=getuser(a->get);
    if(!puser)return http_send(a,Hok Hc0 Htxt,"Error: Please log in first.",0);
    cppJSON request(a->get+a->n);
    string provider=request["ida"]["provider"],id=request["ida"]["id"],model=request["model"];
    if(provider.empty()||model.empty())return http_send(a,Hok Hc0 Htxt,"Error: Invalid input.", 0);
    char* tmp=getgpt2json();
    cppJSON config(tmp);
    if(tmp)free(tmp);
    cppJSON entry=config.child(),messages=request["messages"].clone();
    for(;entry&&!(entry["provider"]==provider);entry=entry.next());
    if(!entry)return http_send(a,Hok Hc0 Htxt,"Error: cannot find provider", 0);
    if(gpt3_model_in_array(entry["public"],model)||(gpt3_model_in_array(entry["private"],model)&&puser->admin));
    else return http_send(a,Hok Hc0 Htxt,"Error: Permission denied", 0);
    bool is_new=id.empty();
    if(!is_new){
        gpt_content* old=(gpt_content*)ndb2_got(gpt_con,id.c_str(),0);
        if(!old||strcmp(old->owner,puser->name)!=0)is_new=true;
        else if(old->isusing)return http_send(a,Hok Hc0 Htxt,"Error: 当前对话正在生成回复，请稍后再试", 0);
        else if(!gpt3_same_messages(cppJSON(old->content),messages))is_new=true;
    }
    if(is_new) {
        id=gpt3_new_id();
        gpt_content* con=(gpt_content*)ndb2_got(gpt_con,id.c_str(),sizeof(gpt_content)+10);
        if(!con)return http_send(a,Hok Hc0 Htxt,"Error: gpt_con database error",0);
        memset(con,0,sizeof(gpt_content)+10);
        strcpy(con->owner,puser->name);
        con->createtime=time(0);
        strcpy(con->content,"[]");
        gpt3_add_history(puser->name,id);
    }
    gpt_content* con=(gpt_content*)ndb2_got(gpt_con,id.c_str(),0);
    if(!con)return http_send(a,Hok Hc0 Htxt,"Error: gpt_con database error",0);
    if(con->isusing)return http_send(a,Hok Hc0 Htxt,"Error: 当前对话正在生成回复，请稍后再试",0);
    string auth;
    int parallel=entry["parallel"].valuedouble();
    if(parallel<=0)parallel=1;
    for(cppJSON token=entry["Authorization"].child(); token; token=token.next()) {
        string candidate=token.valuestring();
        pthread_mutex_lock(&mutex_api_is_using);
        if(api_is_using[candidate]<parallel) {
            api_is_using[candidate]++;
            auth=candidate;
            pthread_mutex_unlock(&mutex_api_is_using);
            break;
        }
        pthread_mutex_unlock(&mutex_api_is_using);
    }
    if(auth.empty())return http_send(a,Hok Hc0 Htxt,"Error: Service busy", 0);
    con->isusing=true;
    request.erase("ida");
    string content=request.stringify_Unformatted();
    string url=entry["url"];
    cppJSON reply=gpt_req(a,id,url,auth,content,entry["format"]);
    pthread_mutex_lock(&mutex_api_is_using);
    api_is_using[auth]--;
    pthread_mutex_unlock(&mutex_api_is_using);
    if(con->name[0]==0) {
        cppJSON title_request("{\"stream\":false}");
        title_request.insert("model",config[0]["public"][0].valuestring());
        cppJSON title_messages("[]");
        cppJSON title_message("{\"role\":\"user\"}");
        string prompt="将以下回复的内容取一个简短的标题：\n"+reply["content"].stringify_Unformatted();
        title_message.insert("content",prompt.c_str());
        title_messages.push_back(std::move(title_message));
        title_request.insert("messages",std::move(title_messages));
        string url=config[0]["url"],auth=config[0]["Authorization"][0];//此处已确保配置文件第一个是用于取标题的 api。
        cppJSON title_reply=gpt_req(nullptr,"",url,auth,title_request.stringify_Unformatted(),config[0]["format"]);
        string title=title_reply["content"];
        if(title.size()>4&&title.substr(0,2)=="**"&&title.substr(title.size()-2)=="**")title=title.substr(2,title.size()-4);
        if(title.size()>60)title=utf8_substr(title,56)+"...";
        if(!title.empty())strcpy(con->name,title.c_str());
    }
    messages.push_back(std::move(reply));
    string final=messages.stringify_Unformatted();
    con=(gpt_content*)ndb2_got(gpt_con,id.c_str(),sizeof(gpt_content)+final.size()+10);
    if(con){
        strcpy(con->content,final.c_str());
        con->isusing=false;
    }
}
void gpt_idname(http_para *a) {
    gpt_content* con=(gpt_content*)ndb2_got(gpt_con,a->get+a->n,0);
    bool valid=true;
    if(!con)valid=false;
    else if(con->publish!=true){
            user_* puser=getuser(a->get);
            if(!puser||strcmp(con->owner,puser->name)!=0)valid=false;
        }
    if(valid==false)return http_send(a,Hok Hc0 Htxt,"Error: Invalid ID or Permission Denied", 0);
    cppJSON ret("{}");
    ret.insert("title",con->name[0]?con->name:"New Chat");
    ret.insert("owner",con->owner);
    http_send(a, Hok Hc0 Htxt, ret.stringify_Unformatted().c_str(), 0);
}
void gpt_changename(http_para *a) {
    user_* puser = getuser(a->get);
    if (!puser) return http_send(a, Hok Hc0 Htxt, "Error: Please log in first.", 0);
    cppJSON au(a->get + a->n);
    if(!au||!au.has("id")||!au.has("name"))return http_send(a, Hok Hc0 Htxt, "Error: JSON parse failed.", 0);
    std::string id = au["id"].valuestring();
    std::string new_name = au["name"].valuestring();
    if(new_name.length() >= 64)return http_send(a, Hok Hc0 Htxt, "Error: Name too long (max 63 chars).", 0);
    gpt_content* con = (gpt_content*)ndb2_got(gpt_con, id.c_str(), 0);
    if(!con||strcmp(con->owner, puser->name) != 0)return http_send(a, Hok Hc0 Htxt, "Error: Permission denied or Conversation not found.", 0);
    memset(con->name, 0, sizeof(con->name));
    strcpy(con->name, new_name.c_str());
    http_send(a, Hok Hc0 Htxt, "ok", 0);
}
void gpt_idcontent(http_para *a) {
    std::string id=a->get+a->n;
    gpt_content* con=(gpt_content*)ndb2_got(gpt_con,id.c_str(),0);
    bool valid=true;
    if(!con)valid=false;
    else if(con->publish!=true){
            user_* puser=getuser(a->get);
            if(!puser||strcmp(con->owner,puser->name)!=0)valid=false;
        }
    if(valid==false)return http_send(a, Hok Hc0 Htxt, "Error: Invalid ID or Permission Denied", 0);
    cppJSON res("{}");
    res.insert("publish", (bool)con->publish);
    res.insert("isusing", (bool)con->isusing);
    res.insert("owner", con->owner);
    res.insert("createtime", (double)con->createtime);
    res.insert("name", con->name);
    res.insert("other", "");
    cppJSON msgs(con->content);
    if (msgs) res.insert("content", msgs);
    else res.insert("content", cppJSON("[]"));
    http_send(a, Hok Hjson Hc0, res.stringify_Unformatted().c_str(), 0);
}
void gpt_getuserhistory(http_para *a) {
    user_* puser=getuser(a->get);
    if(!puser)return http_send(a, Hok Hc0 Htxt, "Error: Please log in first.",0);
    gpt_userhistory* uh=(gpt_userhistory*)ndb2_got(gpt_user, puser->name,0);
    cppJSON arr("[]");
    if(uh)for(int i=0; i<uh->n; i++)arr.push_back(uh->content[i].a);
    http_send(a, Hok Hjson Hc0, arr.stringify_Unformatted().c_str(),0);
}
void gpt_deletehistory(http_para *a) {
    std::string id=a->get+a->n;
    user_* puser=getuser(a->get);
    if(!puser)return http_send(a, Hok Hc0 Htxt, "error_not_logged_in",0);
    gpt_userhistory* uh=(gpt_userhistory*)ndb2_got(gpt_user,puser->name,0);
    if(uh) {
        for(int i=0; i<uh->n; i++)if(strcmp(uh->content[i].a, id.c_str())==0) {
                for(int j=i+1;j<uh->n;j++)uh->content[j-1]=uh->content[j];
                uh->n--;
                return http_send(a, Hok Hc0 Htxt, "ok", 0);
            }
        return http_send(a, Hok Hc0 Htxt, "error_id_not_found", 0);
    }
    http_send(a, Hok Hc0 Htxt, "error_no_history", 0);
}
void gpt_share(http_para *a) {
    user_* puser=getuser(a->get);
    if(!puser)return http_send(a, Hok Hc0 Htxt, "Error: Please log in first.", 0);
    cppJSON au(a->get+a->n);
    if(!au.has("id"))return http_send(a, Hok Hc0 Htxt, "Error: JSON parse failed.", 0);
    std::string id=au["id"].valuestring();
    gpt_content* con=(gpt_content*)ndb2_got(gpt_con, id.c_str(), 0);
   if (!con||strcmp(con->owner, puser->name) != 0)return http_send(a, Hok Hc0 Htxt, "Error: Permission denied or conversation not found.", 0);
    if (con->isusing) return http_send(a, Hok Hc0 Htxt, "Error: 当前对话正在生成回复，请稍后再试", 0);
    con->publish = true;
    http_send(a, Hok Hc0 Htxt, "ok", 0);
}
