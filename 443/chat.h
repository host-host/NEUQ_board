#ifndef CHAT_
#define CHAT_
char *data,*cont;
int fdata,fcont;
ll ldata,lcont;
pthread_mutex_t chat_mutex;
INIT void chat_init(){
    pthread_mutex_init(&chat_mutex,0);
    ldata=lseek(fdata=open("/443/pri/data.dat",O_RDWR|O_APPEND|O_CREAT),0,SEEK_END);
    data=(char*)mmap(0,4ll<<30,PROT_READ|PROT_WRITE,MAP_SHARED,fdata,0);
    lcont=lseek(fcont=open("/443/pri/cont.dat",O_RDWR|O_APPEND|O_CREAT),0,SEEK_END);
    cont=(char*)mmap(0,20ll<<30,PROT_READ|PROT_WRITE,MAP_SHARED,fcont,0);
}
#else

if(bncmp(get,"GET /api/p=")==0){
    char* con=get+n;
    ll pc=readint(get+7);
    if(pc<=0||pc>lcont-8)return;
    ll pd=*(ll*)(cont+pc);
    if(pd<=0||pd>ldata-8||abs(*(ll*)(data+pd))!=pc)return;
    int s=0;
    char *c=(char*)malloc(10*1024*1024);
    n=sprintf(c,"[");
    char bug1[600],bug2[600];
    while((s+=(*(int*)(data+pd+28)==0))<=50&&n<=10000000&&(pd=*(ll*)(data+pd+16))){
        myJSON(data+pd+32,bug1);
        myJSON(data+pd+33+strlen(data+pd+32),bug2);
        n+=sprintf(c+n,"{\"id\":\"%lld\",\"date\":\"%d\",\"px\":\"%d\",\"name\":%s,\"title\":%s},",
                   *(ll*)(data+pd),*(int*)(data+pd+24),*(int*)(data+pd+28),bug1,bug2);
    }
    if(c[n-1]=='[')c[n++]=']';
    else c[n-1]=']';
    mysend(ssl,Hok Hc0 Hjson,c,n);
    free(c);
}
if(bncmp(get,"GET /api/con=")==0){
    char* con=get+n;
    ll pc=readint(get+7);
    if(pc<=0||pc>lcont-8)return;
    ll pd=*(ll*)(cont+pc);
    if(pd<=0||pd>ldata-8||abs(*(ll*)(data+pd))!=pc)return;
    mysend(ssl,Hok Hc0 Htxt,cont+pc+8,0);
}
if(bncmp(get,"POST /api/sendmessage")==0){
    char* con=get+n;
    if(!puser)return mysend(ssl,Hok Hc0 Htxt,"Please log in first.",0);
    int ltitle=strlen(con),lcon=strlen(con+ltitle+1),lu=strlen(puser->name);
    if(ltitle>200||ltitle==0)return mysend(ssl,Hok Hc0 Htxt,"The title is too long.",0);
    ll pc=readint(con+ltitle+lcon+2);
    if(pc<=0||pc>lcont-8)return mysend(ssl,Hok Hc0 Htxt,"ERROR: Something wrong.Code VQ19.",0);
    ll pd=*(ll*)(cont+pc);
    if(pd<=0||pd>ldata-8||abs(*(ll*)(data+pd))!=pc)return mysend(ssl,Hok Hc0 Htxt,"ERROR: Something wrong.Code OV82.",0);
    ll fpd=pd,lpd=pd,tmp;
    while(*(int*)(data+fpd+28))fpd=*(ll*)(data+fpd+8);
    while((tmp=*(ll*)(data+lpd+16))&&*(int*)(data+tmp+28))lpd=tmp;
    char a[300];
    *(ll*)(a+8)=lpd;
    *(int*)(a+24)=time(0);
    *(int*)(a+28)=std::max((*(int*)(data+pd+28))+1,*(int*)(data+lpd+28));
    memcpy(a+32,puser->name,lu+1);
    memcpy(a+33+lu,con,ltitle+1);
    pthread_mutex_lock(&chat_mutex);
    *(ll*)a=lcon?lcont:-lcont;
    if(pd==1){
        tmp=*(ll*)(a+16)=*(ll*)(data+1+16);
        *(int*)(a+28)=0;
        *(ll*)(data+1+16)=ldata;
        if(tmp)*(ll*)(data+tmp+8)=ldata;
    }else{
        ll f=*(ll*)(data+fpd+8),l=*(ll*)(data+lpd+16);
        *(ll*)(data+f+16)=l;
        if(l)*(ll*)(data+l+8)=f;
        *(ll*)(data+lpd+16)=ldata;
        tmp=*(ll*)(a+16)=*(ll*)(data+1+16);
        if(tmp)*(ll*)(data+tmp+8)=ldata;
        *(ll*)(data+fpd+8)=1;
        *(ll*)(data+1+16)=fpd;
    }
    write(fdata,a,34+lu+ltitle);
    write(fcont,&ldata,8);
    write(fcont,con+ltitle+1,lcon+1);
    lcont+=lcon+9;
    ldata+=34+lu+ltitle;
    pthread_mutex_unlock(&chat_mutex);
    mysend(ssl,Hok Hc0 Htxt,"ok",0);
}
#endif