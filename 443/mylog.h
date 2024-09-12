#include<map>
std::map<string,int>mlog;
pthread_mutex_t mloglock;
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
    string a=(string)Hok+Hc0+Hjson+"\r\n{\r";
    char p[2048];
    ll all=0;
    pthread_mutex_lock(&mloglock);
    for(auto i:mlog){
        all+=i.second;
        JSON(i.first.c_str(),p);
        a=a+p+":"+std::to_string(i.second)+",\n";
    }
    pthread_mutex_unlock(&mloglock);
    return a+"\"ALL access:\":"+std::to_string(all)+"\n}";
}