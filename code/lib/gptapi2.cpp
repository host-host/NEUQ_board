#include "gptapi2.h"
#include "user.h"
#include "ndb.h"
#include "cppJSON.h"
#include "gptapi.h"
#include <map>
#include <vector>
#include <string>
#include <mutex>
#include <curl/curl.h>
#include <cstdlib>
#include <cstring>
#include <cctype>
using namespace std;
#define LOG(a,...) printf("<%s : %d>%s : " a "\n",__FILE__,__LINE__,__func__,##__VA_ARGS__)
#define ll long long
ndb gpt_con;  // content_id->gpt_content
ndb gpt_user; // char[24]->gpt_userhistory
extern map<string, int> api_is_using;
extern pthread_mutex_t mutex_api_is_using;
struct cached_req {
    string req_json;
    time_t create_time;
    string owner;
};
map<string, cached_req> request_cache;
pthread_mutex_t request_cache_mutex = PTHREAD_MUTEX_INITIALIZER;
__attribute((constructor)) void gptapi2_init() {
	curl_global_init(CURL_GLOBAL_DEFAULT);
    ndb_init(&gpt_con, "/web/res/pri/gpt_content.dat", 32, 0x1000000000);
    ndb_init(&gpt_user, "/web/res/pri/gpt_userhistory.dat", 24, 0x100000000);
}
static std::string generate_new_id() {
    char buf[32]={0};
    for(int i=0; i<31; i++) {
        int v=rand()%36;
        buf[i]=v<10?'0'+v:'a'+v-10;
    }
    return std::string(buf);
}
static std::string utf8_substr(const std::string& str, unsigned int max_bytes) {
    if (str.length() <= max_bytes) return str;
    unsigned int len = max_bytes;
    while(len > 0 && (str[len] & 0xC0) == 0x80) len--;
    return str.substr(0, len) + "...";
}
void gpt_gotid(http_para *a) {
    user_* puser=getuser(a->get);
    if(!puser)return http_send(a, Hok Hc0 Htxt, "Error: Please log in first.", 0);
    time_t now=time(0);
    pthread_mutex_lock(&request_cache_mutex);
    for(auto it=request_cache.begin(); it!=request_cache.end();)
        if(now-it->second.create_time>300)it=request_cache.erase(it);
        else it++;
    pthread_mutex_unlock(&request_cache_mutex);
    cppJSON au(a->get + a->n);
    if(!au)return http_send(a, Hok Hc0 Htxt, "Error: JSON parse failed",0);
    string id;
    if(au.has("id")){
        id=au["id"].valuestring();
        au.erase("id");
    }
    bool is_new=false;//只有当是同一个用户，并且历史消息一致的时候判定为不是新对话
    cppJSON msgs=au["messages"];
    if(id.empty())is_new=true;
    else {
        gpt_content* old_con=(gpt_content*)ndb_create(&gpt_con,id.c_str(),0);
        if(old_con){
            if(strcmp(old_con->owner,puser->name)!=0)is_new=true;
            else if(old_con->isusing)return http_send(a, Hok Hc0 Htxt, "Error: 当前对话正在生成回复，请稍后再试", 0);
            cppJSON old_msgs(old_con->content);
            for(cppJSON old=old_msgs.child(),ne=msgs.child();old;old=old.next()){
                if(!ne||(old.stringify_Unformatted()!=ne.stringify_Unformatted())){
                    is_new=true;
                    break;
                }
                ne=ne.next();
            }
        }else is_new=true;
    }
    if(is_new==true)id=generate_new_id();
    if(is_new){
        gpt_content* con=(gpt_content*)ndb_create(&gpt_con,id.c_str(),sizeof(gpt_content)+10);
        if(!con)return http_send(a, Hok Hc0 Htxt, "Error: gpt_con database error", 0);
        con->publish=false;
        con->isusing=false;
        strcpy(con->owner,puser->name);
        con->createtime=time(0);
        memset(con->name, 0, sizeof(con->name));
        memset(con->other, 0, sizeof(con->other));
        strcpy(con->content,"[]");
        gpt_userhistory* uh = (gpt_userhistory*)ndb_create(&gpt_user,puser->name,0);
        int N=uh?uh->n:0;
        ll new_size=sizeof(gpt_userhistory)+(N+1)*sizeof(content_id);
        uh=(gpt_userhistory*)ndb_create(&gpt_user,puser->name,new_size);
        if(uh) {
            if(N==0)strcpy(uh->user,puser->name);
            strcpy(uh->content[N].a,id.c_str());
            uh->n=N+1;
        }
    }
    cached_req req;
    req.req_json = au.stringify_Unformatted();
    req.create_time = now;
    req.owner = puser->name;
    pthread_mutex_lock(&request_cache_mutex);
    request_cache[id] = req;
    pthread_mutex_unlock(&request_cache_mutex);
    http_send(a, Hok Hc0 Htxt, id.c_str(), 0);
}
struct askid_write_data {
    http_para* a;
    string response_accumulator;
};
static size_t askid_write_callback(void *ptr, size_t size, size_t nmemb, void *userdata) {
    askid_write_data* wd = (askid_write_data*)userdata;
    size_t total_size = size * nmemb;
    if (wd->a->m == 0) {
        wd->a->m = 1;
        const char* tmp = Hok "Content-Type: text/event-stream\r\n" Hc0 "\r\n";
        if (write(wd->a->cl, tmp, strlen(tmp)));
    }
    if (write(wd->a->cl, ptr, total_size));
    wd->response_accumulator.append((char*)ptr, total_size);
    return total_size;
}
static string extract_assistant_content(const string& sse_data) {
    string full_content = "";
    size_t pos = 0;
    while (pos < sse_data.length()) {
        size_t next_newline = sse_data.find('\n', pos);
        string line;
        if (next_newline == string::npos) {
            line = sse_data.substr(pos);
            pos = sse_data.length();
        } else {
            line = sse_data.substr(pos, next_newline - pos);
            pos = next_newline + 1;
        }
        if (!line.empty() && line.back() == '\r')line.pop_back();
        if (line.rfind("data: ", 0) == 0) {
            string json_str = line.substr(6);
            if (json_str == "[DONE]")break;
            cppJSON delta_json(json_str.c_str());
            if (delta_json) {
                cppJSON choices = delta_json["choices"];
                if (choices && cJSON_IsArray(choices.a) && cJSON_GetArraySize(choices.a) > 0) {
                    cppJSON first_choice = choices[0];
                    if (first_choice && first_choice.has("delta")) {
                        cppJSON delta = first_choice["delta"];
                        if (delta && delta.has("content")) {
                            full_content += delta["content"].valuestring();
                        }
                    }
                }
            }
        }
    }
    return full_content;
}
void gpt_askid(http_para *a) {
    user_* puser = getuser(a->get);
    if (!puser) return http_send(a, Hok Hc0 Htxt, "Error: Please log in first.", 0);
    string id = a->get + a->n;
    cached_req req;
    bool found = false;
    pthread_mutex_lock(&request_cache_mutex);
    auto it = request_cache.find(id);
    if (it != request_cache.end()) {
        req = it->second;
        request_cache.erase(it);
        found = true;
    }
    pthread_mutex_unlock(&request_cache_mutex);
    if (!found) return http_send(a, Hok Hc0 Htxt, "Error: Invalid ID or request expired.", 0);
    if (req.owner != puser->name) return http_send(a, Hok Hc0 Htxt, "Error: Permission Denied.", 0);
    gpt_content* con = (gpt_content*)ndb_create(&gpt_con, id.c_str(), 0);
    if (!con) return http_send(a, Hok Hc0 Htxt, "Error: gpt_con database error", 0);
    if (con->isusing) return http_send(a, Hok Hc0 Htxt, "Error: 当前对话正在生成回复，请稍后再试", 0);
    con->isusing = true;
    cppJSON au(req.req_json.c_str());
    if (!au) {
        con->isusing = false;
        return http_send(a, Hok Hc0 Htxt, "Error: JSON parse failed", 0);
    }
    int ida = au["ida"].valuedouble();
    string model = au["model"].valuestring();
    au.erase("ida");
    char* content = getgptjson();
    cppJSON json(content);
    if (content) free(content);
    if (!json) {
        con->isusing = false;
        return http_send(a, Hok Hc0 Htxt, "Error: Cannot read gpt.json", 0);
    }
    cppJSON now = json[ida];
    int maxparallel = now["parallel"].valuedouble();
    if (maxparallel == 0) maxparallel = 1;
    if (now.has("available") && now["available"] == false) {
        con->isusing = false;
        return http_send(a, Hok Hc0 Htxt, "Error: 错误，模型不可用", 0);
    }
    int admin = puser->admin;
    if (admin == 0 && !(now["public"] == true)) {
        con->isusing = false;
        return http_send(a, Hok Hc0 Htxt, "Error: 错误，没有权限", 0);
    }
    string auth_token = "";
    bool model_matched = false;
    for (cppJSON p = now["model"].child(); p.a; p = p.next()) {
        if (p == model) {
            model_matched = true;
            for (cppJSON q = now["Authorization"].child(); q.a; q = q.next()) {
                string tmp_auth = q.valuestring();
                pthread_mutex_lock(&mutex_api_is_using);
                if (api_is_using[tmp_auth] < maxparallel) {
                    api_is_using[tmp_auth]++;
                    auth_token = tmp_auth;
                    pthread_mutex_unlock(&mutex_api_is_using);
                    break;
                }
                pthread_mutex_unlock(&mutex_api_is_using);
            }
            break;
        }
    }
    if (!model_matched) {
        con->isusing = false;
        return http_send(a, Hok Hc0 Htxt, "Error: 错误，未找到模型", 0);
    }
    if (auth_token.empty()) {
        con->isusing = false;
        return http_send(a, Hok Hc0 Htxt, "Error: 服务繁忙，请稍后再试", 0);
    }
    string api_url = now["url"].valuestring();
    string auth_header = "Authorization: " + auth_token;
    string post_data = au.stringify_Unformatted();
    askid_write_data wd;
    wd.a = a;
    a->m = 0;
    CURL *curl = curl_easy_init();
    if (!curl) {
        pthread_mutex_lock(&mutex_api_is_using);
        api_is_using[auth_token]--;
        pthread_mutex_unlock(&mutex_api_is_using);
        con->isusing = false;
        return http_send(a, Hok Hc0 Htxt, "Error: curl_easy_init failed", 0);
    }
    curl_easy_setopt(curl, CURLOPT_URL, api_url.c_str());
    struct curl_slist *headers = NULL;
    headers = curl_slist_append(headers, "Content-Type: application/json");
    headers = curl_slist_append(headers, auth_header.c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, post_data.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, askid_write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &wd);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 60L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 1200L);
    CURLcode res = curl_easy_perform(curl);
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);
    pthread_mutex_lock(&mutex_api_is_using);
    api_is_using[auth_token]--;
    pthread_mutex_unlock(&mutex_api_is_using);
    if (res == CURLE_OK) {
        string assistant_reply = extract_assistant_content(wd.response_accumulator);
        cppJSON msgs = au["messages"];
        if (msgs && cJSON_IsArray(msgs.a)) {
            cppJSON reply_node("{}");
            reply_node.insert("role", "assistant");
            reply_node.insert("content", assistant_reply.c_str());
            msgs.push_back(std::move(reply_node));
            string final_content=msgs.stringify_Unformatted();
            if((con=(gpt_content*)ndb_create(&gpt_con,id.c_str(),sizeof(gpt_content)+final_content.length()+10)))
                strcpy(con->content, final_content.c_str());
        }
    }
    if((con=(gpt_content*)ndb_create(&gpt_con,id.c_str(),0)))con->isusing=0;
}
void gpt_idname(http_para *a) {
    std::string id=a->get+a->n;
    gpt_content* con=(gpt_content*)ndb_create(&gpt_con,id.c_str(),0);
    bool valid=true;
    if(!con)valid=false;
    else if(con->publish!=true){
            user_* puser=getuser(a->get);
            if(!puser||strcmp(con->owner,puser->name)!=0)valid=false;
        }
    if(valid==false)return http_send(a,Hok Hc0 Htxt,"Error: Invalid ID or Permission Denied", 0);
    std::string title="New Chat";
    if(con->name[0])title=con->name;
    else{
        cppJSON msgs(con->content);
        for(cppJSON c=msgs.child(); c; c=c.next())
            if (c.has("role") && string(c["role"].valuestring()) == "user" && c.has("content")) {
                title = c["content"].valuestring();
                break;
            }
        title=utf8_substr(title,25);
    }
    cppJSON ret("{}");
    ret.insert("title",title);
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
    gpt_content* con = (gpt_content*)ndb_create(&gpt_con, id.c_str(), 0);
    if(!con||strcmp(con->owner, puser->name) != 0)return http_send(a, Hok Hc0 Htxt, "Error: Permission denied or Conversation not found.", 0);
    memset(con->name, 0, sizeof(con->name));
    strcpy(con->name, new_name.c_str());
    http_send(a, Hok Hc0 Htxt, "ok", 0);
}
void gpt_idcontent(http_para *a) {
    std::string id=a->get+a->n;
    gpt_content* con=(gpt_content*)ndb_create(&gpt_con,id.c_str(),0);
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
    gpt_userhistory* uh=(gpt_userhistory*)ndb_create(&gpt_user, puser->name,0);
    cppJSON arr("[]");
    if(uh)for(int i=0; i<uh->n; i++)arr.push_back(uh->content[i].a);
    http_send(a, Hok Hjson Hc0, arr.stringify_Unformatted().c_str(),0);
}
void gpt_deletehistory(http_para *a) {
    std::string id=a->get+a->n;
    user_* puser=getuser(a->get);
    if(!puser)return http_send(a, Hok Hc0 Htxt, "error_not_logged_in",0);
    gpt_userhistory* uh=(gpt_userhistory*)ndb_create(&gpt_user,puser->name,0);
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
    gpt_content* con=(gpt_content*)ndb_create(&gpt_con, id.c_str(), 0);
    if (!con||strcmp(con->owner, puser->name) != 0)return http_send(a, Hok Hc0 Htxt, "Error: Permission denied or conversation not found.", 0);
    con->publish = true;
    http_send(a, Hok Hc0 Htxt, "ok", 0);
}