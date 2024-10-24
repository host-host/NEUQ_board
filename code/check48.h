#ifndef CHECK48_
#define CHECK48_
#define ll long long
#define CHECK_LEN 48
std::map<unsigned ll,int>check48_mp;
int rand_base,rand_base1;
pthread_mutex_t check48_lock;
INIT void check48_init(){
    struct timespec timeSeed;
    clock_gettime(CLOCK_REALTIME,&timeSeed);
    srand(timeSeed.tv_sec * 1000000 + timeSeed.tv_nsec);
    rand_base=rand();
    rand_base1=rand();
    pthread_mutex_init(&check48_lock,0);
}
int check_48_(unsigned ll a){
    pthread_mutex_lock(&check48_lock);
    if(!check48_mp.count(a)){
        pthread_mutex_unlock(&check48_lock);
        return 0;
    }
    int tmp=check48_mp[a];
    check48_mp.erase(a);
    pthread_mutex_unlock(&check48_lock);
    return tmp>=time(0);
}
void check48(https_para* ssl){
    ll a[CHECK_LEN],s=0,n=0;
    char c[1024];
    for(int i=0;i<CHECK_LEN;i++){
        a[i]=((ll)rand()<<32)+rand();
        a[i]=a[i]%30000000000000000ll;
        if(rand()%2)s+=a[i];
    }
    n=sprintf(c,"%d %lld\n",CHECK_LEN,s);
    for(int i=0;i<CHECK_LEN;i++)n+=sprintf(c+n,"%lld ",a[i]);
    unsigned ll p=CHECK_LEN+rand_base1;
    p=p*rand_base+s+rand_base1;
    for(int i=0;i<CHECK_LEN;i++)p=p*rand_base+a[i]+rand_base1;
    pthread_mutex_lock(&check48_lock);
    check48_mp[p]=time(0)+60;
    if(check48_mp.size()>1000000){
        std::map<unsigned ll,int>tmp;
        int now=time(0);
        for(auto i:check48_mp)if(i.second>=now)tmp[i.first]=i.second;
        swap(tmp,check48_mp);
    }
    pthread_mutex_unlock(&check48_lock);
    return https_send(ssl,Hok Hc0 Htxt,c,n);
}
#endif