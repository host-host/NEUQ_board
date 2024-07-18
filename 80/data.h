const char Head_d[]="HTTP/1.1 200 OK\r\ncache-control: max-age=0, public\r\nContent-Length:          \r\n\r\n[";
void getp(int cl,const char* re,const char* con,int n,const char* id){
    ll pc=readint(re+7);
    if(pc<=0||pc>lcont-8)return;
    ll pd=*(ll*)(cont+pc);
    if(pd<=0||pd>ldata-8||abs(*(ll*)(data+pd))!=pc)return;
    int s=0;
    char *c=(char*)malloc(10*1024*1024);
    memcpy(c,Head_d,n=strlen(Head_d));
    while(s<=50&&n<=10000000&&(pd=*(ll*)(data+pd+16))){
        int px=*(int*)(data+pd+28);
        sprintf(c+n,"{\"id\":\"%lld\",\"date\":\"%d\",\"px\":\"%d\",\"name\":\"%s\",\"title\":%s},",*(ll*)(data+pd),*(int*)(data+pd+24),px,data+pd+32,data+pd+33+strlen(data+pd+32));
        n+=strlen(c+n);
        if(px==0)s++;
    }
    if(c[n-1]=='['){
        c[n++]=']';
        c[n++]='\0';
    }else c[n-1]=']';
    int m=66,x=n-strlen(Head_d)+1;
    for(int i=x;i;i/=10)m++;
    for(;x;x/=10)c[m--]='0'+x%10;
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
    if(x<0||x>=users.size())return mysend(cl,"ERROR: Something wrong.Code FR50.");
    for(int i=5;i<10;i++)if(id[i]!=user[x][72+i])return mysend(cl,"ERROR: Something wrong.Code AQ37.");
    int ltitle=strlen(con),lcon=strlen(con+ltitle+1),lu=strlen(user[x]);
    if(ltitle>200||ltitle==0)return mysend(cl,"The title is too long.");
    ll pc=readint(con+ltitle+lcon+2);
    if(pc<=0||pc>lcont-8)return mysend(cl,"ERROR: Something wrong.Code VQ19.");
    ll pd=*(ll*)(cont+pc);
    if(pd<=0||pd>ldata-8||abs(*(ll*)(data+pd))!=pc)return mysend(cl,"ERROR: Something wrong.Code OV82.");
    int npx=(*(int*)(data+pd+28))+1;
    ll fpd=pd,lpd=pd;
    while(*(int*)(data+fpd+28))fpd=*(ll*)(data+fpd+8);
    while(1){
        ll tlpd=*(ll*)(data+lpd+16);
        if(tlpd!=0&&*(int*)(data+tlpd+28))lpd=tlpd;
        else break;
    }
    char a[300];
    *(int*)(a+24)=time(0);
    *(int*)(a+28)=max((*(int*)(data+pd+28))+1,*(int*)(data+lpd+28));
    memcpy(a+32,user[x],lu+1);
    memcpy(a+33+lu,con,ltitle+1);
    while(u)usleep(100000);
    u=1;
    ll tlmp=ldata;
    *(ll*)a=lcon?lcont:-lcont;
    if(pd==1){
        *(ll*)(a+8)=1;
        ll tmp=*(ll*)(a+16)=*(ll*)(data+1+16);
        *(int*)(a+28)=0;
        *(ll*)(data+1+16)=ldata;
        if(tmp)*(ll*)(data+tmp+8)=ldata;
    }else{
        ll f=*(ll*)(data+fpd+8),l=*(ll*)(data+lpd+16);
        *(ll*)(data+f+16)=l;
        if(l)*(ll*)(data+l+8)=f;
        *(ll*)(a+8)=lpd;
        *(ll*)(data+lpd+16)=ldata;
        ll tmp=*(ll*)(a+16)=*(ll*)(data+1+16);
        if(tmp)*(ll*)(data+tmp+8)=ldata;
        *(ll*)(data+fpd+8)=1;
        *(ll*)(data+1+16)=fpd;
    }
    write(fdata,a,34+lu+ltitle);
    ldata+=34+lu+ltitle;
    write(fcont,&tlmp,8);
    write(fcont,con+ltitle+1,lcon+1);
    lcont+=lcon+9;
    u=0;
    mysend(cl,"ok");
}