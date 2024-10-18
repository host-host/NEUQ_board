#ifndef LOG_
#define LOG_
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
#else
if(puser==0||!puser->admin){
    char tmp[30]={0};
    sprintf(tmp,"%d.%d.%d.%d",ip>>24&255,ip>>16&255,ip>>8&255,ip&255);
    addlog(tmp);
    memcpy(tmp,get,29);
    for(int i=0;i<29;i++)if(tmp[i]=='\r'||tmp[i]=='\n')tmp[i]=0;
    addlog(tmp);
} else{
    if(bncmp(get,"GET /admin ")==0)return mysend(ssl,Hok Hc0 Hjson,printlog().c_str(),0);
    if(bncmp(get,"GET /reset ")==0){
        clearlog();
        return mysend(ssl,Hok Hc0 Htxt,"OK",0);
    }
}
#endif