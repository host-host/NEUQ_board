#ifndef CHAT_
#define CHAT_

#include<unistd.h>
#include<pthread.h>
#include<sys/types.h>
#include<fcntl.h>
#include<sys/mman.h>
#include"myio.h"
#include"user.h"
#include"lib/ndb.h"
struct chat_a{//0~8id 8~16time
    char next[16];//only deep==0
    ll time;
    ll lasttime;//only deep==0
    int deep;
    int future;
    char name[24];
    char content[0];
};//64
ndb chatn;
pthread_mutex_t chat_mutex;
INIT void chat_init(){
    ndb_init(&chatn,"./res/pri/chat.dat",16,1ll<<34);
    ll tmp=0;
    ndb_create(&chatn,(char*)&tmp,64);
}
void getcon(http_para*a){
    ll pc=readll(a->get+7);
    chat_a*p=(chat_a*)ndb_create(&chatn,(char*)&pc,0);
    if(!p)return;
    http_send(a,Hok Hc0 Htxt,p->content+strlen(p->content),0);
}
void getp(http_para*a){
    ll pc=readll(a->get+7);
    chat_a*p=(chat_a*)ndb_create(&chatn,(char*)&pc,0);
    if(!p)return;
    int s=0;
    char *c=(char*)malloc(10*1024*1024);
    int n=sprintf(c,"[");
    char bug1[600],bug2[600];
    while(n<=10000000){
        ll nex=*(ll*)p->next;
        s++;
        if(s>150||(s>50&&p->deep==0))break;
        if(!(p=(chat_a*)ndb_create(&chatn,p->next,0)))break;
        myJSON(p->content,bug1);
        myJSON(p->content+strlen(p->content),bug2);
        n+=sprintf(c+n,"{\"id\":\"%lld\",\"date\":\"%lld\",\"px\":\"%d\",\"name\":%s,\"title\":%s},",nex,p->time,p->deep,bug1,bug2);
    }
    if(c[n-1]=='[')c[n++]=']';
    else c[n-1]=']';
    http_send(a,Hok Hc0 Hjson,c,n);
    free(c);
}
void sendmessage(http_para*b){
    char* con=b->get+b->n;
    user_* puser=getuser(b->get);
    if(!puser)return http_send(b,Hok Hc0 Htxt,"Please log in first.",0);
    int ltitle=strlen(con),lcon=strlen(con+ltitle+1),lu=strlen(puser->name);
    if(ltitle>200||ltitle==0)return http_send(b,Hok Hc0 Htxt,"The title is too long.",0);
    if(lcon>1024*20)return http_send(b,Hok Hc0 Htxt,"The content is too long.",0);
    ll id=abs((ll)rand()*rand()*rand()*rand())*(lcon?1:-1);
    while(ndb_create(&chatn,(char*)&id,0))id=abs((ll)rand()*rand()*rand()*rand())*(lcon?1:-1);
    chat_a*p=(chat_a*)ndb_create(&chatn,(char*)&id,64+ltitle+lcon);
    if(!p)return http_send(b,Hok Hc0 Htxt,"ERROR: Something wrong.Code VQ119.",0);
    ll pc=readll(con+ltitle+lcon+2);
    p->time=time(0);
    strncpy(p->name,puser->name,lu);
    p->name[23]=0;
    memcpy(p->content,con,ltitle+1);
    memcpy(p->content+ltitle+1,con+ltitle+1,lcon+1);
    pthread_mutex_lock(&chat_mutex);
    chat_a* parent=(chat_a*)ndb_create(&chatn,(char*)&pc,0);
    if (!parent) {
        pthread_mutex_unlock(&chat_mutex);
        return http_send(b, Hok Hc0 Htxt, "ERROR: Parent node not found or Database error.", 0);
    }

    if (pc == 0) {
        // --- 新开话题 (New Thread) ---
        p->deep = 0;
        ll first_id = *(ll*)head->next;
        
        // 插入到头节点之后
        *(ll*)p->prev = 0; // 0 表示 head
        *(ll*)p->next = first_id;
        
        *(ll*)head->next = id;
        if (first_id) {
            chat_a* first = (chat_a*)ndb_create(&chatn, (char*)&first_id, 0);
            if (first) *(ll*)first->prev = id;
        }
    } else {
        // --- 回复已有话题 (Reply) ---
        
        // 1. 寻找话题的根节点 (fpd - First Node of Thread)
        // 根节点的特征是 deep == 0
        ll fpd_id = pc;
        chat_a* fpd = parent;
        while (fpd->deep > 0) {
            fpd_id = *(ll*)fpd->prev;
            fpd = (chat_a*)ndb_create(&chatn, (char*)&fpd_id, 0);
        }

        // 2. 寻找话题的尾节点 (lpd - Last Node of Thread)
        // 话题是一段连续的链表，直到遇到下一个 deep==0 的节点或链表结束
        ll lpd_id = pc; // 从父节点开始往下找
        chat_a* lpd = parent;
        ll tmp_id;
        while ((tmp_id = *(ll*)lpd->next)) {
            chat_a* tmp = (chat_a*)ndb_create(&chatn, (char*)&tmp_id, 0);
            if (!tmp || tmp->deep == 0) break; // 遇到了下一个话题的根，说明当前话题结束
            lpd = tmp;
            lpd_id = tmp_id;
        }
        
        // 设置新节点深度
        p->deep = parent->deep + 1;

        // 3. 链表操作：将整个话题 [fpd ... lpd] 移动到最前面，并将新节点 p 追加到 lpd 后面
        
        ll first_id = *(ll*)head->next; // 当前排第一的话题ID
        
        if (fpd_id == first_id) {
            // Case A: 话题已经在第一位了，只需将 p 追加到 lpd 后面
            ll next_thread_id = *(ll*)lpd->next;
            
            *(ll*)lpd->next = id;
            *(ll*)p->prev = lpd_id;
            
            *(ll*)p->next = next_thread_id;
            if (next_thread_id) {
                chat_a* nex = (chat_a*)ndb_create(&chatn, (char*)&next_thread_id, 0);
                if (nex) *(ll*)nex->prev = id;
            }
        } else {
            // Case B: 话题不在第一位，需要“挖出”整段链表并移到头部
            
            // B1. 将 [fpd ... lpd] 从原位置断开
            ll prev_block_id = *(ll*)fpd->prev;
            ll next_block_id = *(ll*)lpd->next; // 这是下一个话题的开头
            
            if (prev_block_id) {
                chat_a* pr = (chat_a*)ndb_create(&chatn, (char*)&prev_block_id, 0);
                if (pr) *(ll*)pr->next = next_block_id;
            }
            if (next_block_id) {
                chat_a* nx = (chat_a*)ndb_create(&chatn, (char*)&next_block_id, 0);
                if (nx) *(ll*)nx->prev = prev_block_id;
            }
            
            // B2. 将 p 连接到 lpd 后面（p成为该话题的新尾部）
            *(ll*)lpd->next = id;
            *(ll*)p->prev = lpd_id;
            
            // B3. 将 [fpd ... p] 插入到 Head 之后
            *(ll*)head->next = fpd_id;
            *(ll*)fpd->prev = 0;
            
            *(ll*)p->next = first_id; // p 后面接原来的第一名
            if (first_id) {
                chat_a* first = (chat_a*)ndb_create(&chatn, (char*)&first_id, 0);
                if (first) *(ll*)first->prev = id; 
            }
        }
    }
    
    pthread_mutex_unlock(&chat_mutex);
    http_send(b,Hok Hc0 Htxt,"ok",0);
}
#endif