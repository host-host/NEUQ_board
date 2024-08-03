const char Head_d[]="HTTP/1.1 200 OK\r\ncache-control: max-age=0, public\r\nContent-Length:          \r\n\r\n[";
const char sprintf_n[]="{\"id\":\"%lld\",\"date\":\"%d\",\"px\":\"%d\",\"name\":\"%s\",\"title\":%s},";
void getp(int cl,const char* re,const char* con,int n,const char* id){
    ll pc=readint(re+7);
    if(pc<=0||pc>lcont-8)return;
    ll pd=*(ll*)(cont+pc);
    if(pd<=0||pd>ldata-8||abs(*(ll*)(data+pd))!=pc)return;
    int s=0;
    char *c=(char*)malloc(10*1024*1024);
    memcpy(c,Head_d,n=strlen(Head_d));
    while((s+=(*(int*)(data+pd+28)==0))<=50&&n<=10000000&&(pd=*(ll*)(data+pd+16)))
        n+=sprintf(c+n,sprintf_n,*(ll*)(data+pd),*(int*)(data+pd+24),*(int*)(data+pd+28),data+pd+32,data+pd+33+strlen(data+pd+32));
    if(c[n-1]=='[')c[n++]=']';
    else c[n-1]=']';
    c[sprintf(c+66,"%d",n-80)+66]=' ';
    write(cl,c,n);
    free(c);
}
void getcon(int cl,const char* re,const char* con,int n,const char* id){
    ll pc=readint(re+7);
    if(pc<=0||pc>lcont-8)return;
    ll pd=*(ll*)(cont+pc);
    if(pd<=0||pd>ldata-8||abs(*(ll*)(data+pd))!=pc)return;
    mysend(cl,cont+pc+8);
}
volatile int u=0;
void postmsg(int cl,const char* re,const char* con,int n,const char* id){
    if(id[0]=='0')return mysend(cl,"Please log in first.");
    int x=0;
    for(int i=0;i<5;i++)x=x*26+id[i]-'A';
    if(x<0||x>=users.size()||*(ll*)(id+2)!=*(ll*)(user[x]+74))return;
    int ltitle=strlen(con),lcon=strlen(con+ltitle+1),lu=strlen(user[x]);
    if(ltitle>200||ltitle==0)return mysend(cl,"The title is too long.");
    ll pc=readint(con+ltitle+lcon+2);
    if(pc<=0||pc>lcont-8)return mysend(cl,"ERROR: Something wrong.Code VQ19.");
    ll pd=*(ll*)(cont+pc);
    if(pd<=0||pd>ldata-8||abs(*(ll*)(data+pd))!=pc)return mysend(cl,"ERROR: Something wrong.Code OV82.");
    int npx=(*(int*)(data+pd+28))+1;
    ll fpd=pd,lpd=pd,tlpd;
    while(*(int*)(data+fpd+28))fpd=*(ll*)(data+fpd+8);
    while((tlpd=*(ll*)(data+lpd+16))&&*(int*)(data+tlpd+28))lpd=tlpd;
    char a[300];
    *(ll*)(a+8)=lpd;
    *(int*)(a+24)=time(0);
    *(int*)(a+28)=max((*(int*)(data+pd+28))+1,*(int*)(data+lpd+28));
    memcpy(a+32,user[x],lu+1);
    memcpy(a+33+lu,con,ltitle+1);
    while(u)usleep(100000);
    u=1;
    *(ll*)a=lcon?lcont:-lcont;
    if(pd==1){
        ll tmp=*(ll*)(a+16)=*(ll*)(data+1+16);
        *(int*)(a+28)=0;
        *(ll*)(data+1+16)=ldata;
        if(tmp)*(ll*)(data+tmp+8)=ldata;
    }else{
        ll f=*(ll*)(data+fpd+8),l=*(ll*)(data+lpd+16);
        *(ll*)(data+f+16)=l;
        if(l)*(ll*)(data+l+8)=f;
        *(ll*)(data+lpd+16)=ldata;
        ll tmp=*(ll*)(a+16)=*(ll*)(data+1+16);
        if(tmp)*(ll*)(data+tmp+8)=ldata;
        *(ll*)(data+fpd+8)=1;
        *(ll*)(data+1+16)=fpd;
    }
    write(fdata,a,34+lu+ltitle);
    write(fcont,&ldata,8);
    write(fcont,con+ltitle+1,lcon+1);
    lcont+=lcon+9;
    ldata+=34+lu+ltitle;
    u=0;
    mysend(cl,"ok");
}