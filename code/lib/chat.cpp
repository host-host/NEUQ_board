#include<unistd.h>
#include<pthread.h>
#include<stdlib.h>
#include<string.h>
#include<stdio.h>
#include<time.h>
#include"user.h"
#include"chat.h"
#include"ndb2.h"
#include"cppJSON.h"
using std::string;
ndb2 chatdb;
int *next_id;
pthread_mutex_t chat_mutex;
__attribute((constructor)) void chat_init(){
    pthread_mutex_init(&chat_mutex,0);
    chat*p=(chat*)ndb2_got(chatdb=ndb2_init("/web/res/pri/chat.ndb2"),"1",sizeof(chat)+2);
    next_id=(int*)ndb2_got(chatdb,"next_id",8);
    if(!p->next[0]){
        memset(p,0,sizeof(chat)+2);
        p->next[0]=p->prev[0]='1';
        *next_id=1;
    }
}
void chat_list(http_para* a){
    cppJSON req(a->get+a->n);
    string id=req["id"].valuestring();
    chat* nd=(chat*)ndb2_got(chatdb,id.c_str(),0);
    if(!nd)return http_send(a,Hok Hc0 Hjson,"[]",2);
    cppJSON ans("[]");
    for(int s=0;s<50&&strcmp(nd->next,"1");s+=(nd->deep==0)){
        cppJSON item("{}");
        item.insert("id",nd->next);
        if(!(nd=(chat*)ndb2_got(chatdb,nd->next,0)))break;
        item.insert("date",(double)nd->time);
        item.insert("px",(double)nd->deep);
        item.insert("name",nd->name);
        item.insert("title",nd->content);
        if(nd->content[strlen(nd->content)+1]=='\0')item.insert("empty",true);
        ans.push_back(std::move(item));
    }
    http_send(a,Hok Hc0 Hjson,ans.stringify_Unformatted().c_str(),0);
}
void chat_content(http_para* a){
    cppJSON req(a->get+a->n);
    string id=req["id"].valuestring();
    chat* nd=(chat*)ndb2_got(chatdb,id.c_str(),0);
    if(!nd)return http_send(a,Hok Hc0 Hjson,"{\"status\":\"error\",\"content\":\"未找到内容\"}",0);
    cppJSON ans("{\"status\":\"ok\"}");
    ans.insert("content",nd->content+strlen(nd->content)+1);
    http_send(a,Hok Hc0 Hjson,ans.stringify_Unformatted().c_str(),0);
}
static void chat_add(chat*x,chat*y,chat*addx,const char*idx,chat*addy,const char*idy){
    memcpy(addx->prev,y->prev,16);
    memcpy(addy->next,x->next,16);
    memcpy(y->prev,idy,16);
    memcpy(x->next,idx,16);
}
void chat_send(http_para* a){
    user_* puser=getuser(a->get);
    if(!puser)return http_send(a,Hok Hc0 Hjson,"{\"status\":\"error\",\"message\":\"Please log in first.\"}",0);
    cppJSON req(a->get+a->n);
    if(!req)return http_send(a,Hok Hc0 Hjson,"{\"status\":\"error\",\"message\":\"Bad JSON format.\"}",0);
    string title=req["title"].valuestring(),content=req["content"].valuestring(),pid=req["parentId"].valuestring();
    int ltitle=title.length();
    if(ltitle>200||ltitle==0)return http_send(a,Hok Hc0 Hjson,"{\"status\":\"error\",\"message\":\"The title is too long or empty.\"}",0);
    chat* parent=(chat*)ndb2_got(chatdb,pid.c_str(),0);
    if(!parent)return http_send(a,Hok Hc0 Hjson,"{\"status\":\"error\",\"message\":\"Parent node not found.\"}",0);
    pthread_mutex_lock(&chat_mutex);
    chat*up=parent,*fup=(chat*)ndb2_got(chatdb,up->prev,0),*down=up,*ndown=(chat*)ndb2_got(chatdb,down->next,0);
    for(;up->deep;fup=(chat*)ndb2_got(chatdb,up->prev,0))up=fup;
    for(;ndown->deep;ndown=(chat*)ndb2_got(chatdb,down->next,0))down=ndown;
    char id[16]={0};
    sprintf(id,"%d",++(*next_id));
    chat* nn=(chat*)ndb2_got(chatdb,id,sizeof(chat)+ltitle+content.length()+2);
    if(!nn){
        pthread_mutex_unlock(&chat_mutex);
        return http_send(a,Hok Hc0 Hjson,"{\"status\":\"error\",\"message\":\"Database allocation failed.\"}",0);
    }
    nn->time=time(0);
    nn->deep=(pid=="1")?0:parent->deep+1;
    nn->other=0;
    strncpy(nn->name,puser->name,47);
    strcpy(nn->content,title.c_str());
    strcpy(nn->content+ltitle+1,content.c_str());
    if(pid=="1")chat_add(parent,(chat*)ndb2_got(chatdb,parent->next,0),nn,id,nn,id);
    else{
        chat_add(down,ndown,nn,id,nn,id);
        char upid[16]={0};
        memcpy(upid,fup->next,16);
        memcpy(fup->next,nn->next,16);
        memcpy(ndown->prev,up->prev,16);
        chat* root=(chat*)ndb2_got(chatdb,"1",0);
        chat_add(root,(chat*)ndb2_got(chatdb,root->next,0),up,upid,nn,id);
    }
    pthread_mutex_unlock(&chat_mutex);
    return http_send(a,Hok Hc0 Hjson,"{\"status\":\"ok\"}",0);
}