#ifndef LOG_
#define LOG_


#include<map>
std::map<string,int>mlog;
pthread_mutex_t mloglock;
INIT void log_init(){
    pthread_mutex_init(&mloglock,0);
}
void addlog(const char*a){
    pthread_mutex_lock(&mloglock);
    mlog[a]++;
    pthread_mutex_unlock(&mloglock);
}
void clearlog(){
    pthread_mutex_lock(&mloglock);
    mlog.clear();
    pthread_mutex_unlock(&mloglock);
}
string printlog(){
    string a=(string)"{\n";
    char p[2048];
    ll all=0;
    pthread_mutex_lock(&mloglock);
    for(auto i:mlog){
        all+=i.second;
        myJSON(i.first.c_str(),p);
        a=a+p+":"+std::to_string(i.second)+",\n";
    }
    pthread_mutex_unlock(&mloglock);
    return a+"\"ALL access:\":"+std::to_string(all)+"\n}";
}
void admin(https_para *ssl){
    user_ *puser=getuser(ssl->get);
    if(puser&&puser->admin)return https_send(ssl,Hok Hc0 Hjson,printlog().c_str(),0);
}
void reset(https_para *ssl){
    user_ *puser=getuser(ssl->get);
    if(puser&&puser->admin){
        clearlog();
        return https_send(ssl,Hok Hc0 Htxt,"OK",0);
    }
}



#endif