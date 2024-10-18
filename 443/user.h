#ifndef USER_
#define USER_TOHOME "<script>window.location.href='/';</script>"
#define USER_
NitDB user/*ll->user_*/,name2id/*char[24]->ll*/;
struct user_{
    char name[24],pwd[24],cookie_rand[8];
    int admin,time;
    char other[80];
};
INIT void user_init(){
    NitDB_init(&user,"/443/pri/user.dat",8,0x100000000);
    NitDB_init(&name2id,"/443/pri/name2id.dat",24,0x10000000);//id:8
}
#else
user_* puser=0;
{
    char* tmpp;
    if((tmpp=(char*)strstr(get,"Cookie: id="))){
        user_* p1=(user_*)NitDB_create(&user,tmpp+11,0);
        if(p1&&ncmp(tmpp+11+8,p1->cookie_rand,8)==0)puser=p1;
    }
}
if(bncmp(get,"POST /api/login ")==0){
    const char* con=get+n;
    for(int i=4,j;con[i];i++)if(con[i]=='&'){
        char c[24]={0};
        user_* p1;
        ll* p;
        memcpy(c,con+4,std::min(23,i-4));
        if(!(p=(ll*)NitDB_create(&name2id,c,0)))return mysend(ssl,Hok Hhtml Hc0,"<script>alert('User does not exist!');</script>",0);
        if(!(p1=(user_*)NitDB_create(&user,p,0)))return mysend(ssl,Hok Htxt Hc0,"server error! D4F",0);
        if(bncmp(con+i+5,p1->pwd))return mysend(ssl,Hok Hhtml Hc0,"<script>alert('Password Error!');</script>",0);
        char tmp[]="Set-Cookie: id=1234567812345678; Max-Age=604800; Path=/; Secure; HttpOnly\r\n";
        memcpy(tmp+15,p,8);
        if(time(0)>p1->time){
            for(int i=0;i<8;i++)p1->cookie_rand[i]=rand()%26+'A';
            p1->time=time(0)+604800;
        }
        memcpy(tmp+15+8,p1->cookie_rand,8);
        return mysend(ssl,((string)Hok+tmp+Hc0).c_str(),USER_TOHOME,0);
    }
}
if(bncmp(get,"POST /api/register ")==0){
    const char* con=get+n;
    for(int i=4,j;con[i];i++)if(con[i]=='&'){
        char c[24]={0};
        user_* p1;
        ll* p;
        if(i-4<1||i-4>23)return mysend(ssl,Hok Hhtml Hc0,"<script>alert('Username length too long!');</script>",0);
        memcpy(c,con+4,i-4);
        if(c[0]==' '||c[i-4-1]==' ')return;
        if((p=(ll*)NitDB_create(&name2id,c,0)))return mysend(ssl,Hok Hhtml Hc0,"<script>alert('This user already exists.');</script>",0);
        if(!(p=(ll*)NitDB_create(&name2id,c,8)))return mysend(ssl,Hok Htxt Hc0,"server error! D3F",0);
        while(1){
            char tmp[8]={0};
            for(int i=0;i<8;i++)tmp[i]=rand()%26+(rand()%2?'a':'A');
            *p=*(ll*)tmp;
            if(!NitDB_create(&user,p,0))break;
        }
        if(!(p1=(user_*)NitDB_create(&user,p,sizeof(user_))))return mysend(ssl,Hok Htxt Hc0,"server error! D2F",0);
        memcpy(p1->name,c,24);
        for(j=i+5;con[j]&&con[j]!='&';j++);
        memset(p1->pwd,0,24);
        memcpy(p1->pwd,con+i+5,std::min(23,j-(i+5)));
        p1->admin=p1->time=0;
        while(con[j]&&con[j]!='&')j++;
        memset(p1->other,0,80);
        memcpy(p1->other,con+j+1,std::min(79,(int)strlen(con+j+1)));
        return mysend(ssl,Hok Hhtml Hc0,"<script>alert('Success, please log in.');window.location.href='/login';</script>",0);
    }
}
if(bncmp(get,"GET /api/logout ")==0)return mysend(ssl,Hok Hc0 Hhtml "Set-Cookie: id=; Expires=Thu, 01 Jan 1970 00:00:00 GMT; Path=/;\r\n",USER_TOHOME,0);
if(bncmp(get,"GET /api/user ")==0){
    if(puser)return mysend(ssl,Hok Hc0,puser->name,0);
    else return mysend(ssl,Hok Hc0,"Not_Logged_In",0);
}
if(bncmp(get,"POST /api/change_password ")==0){
    if(!puser)return mysend(ssl,Hok Hc0 Hhtml,"<script>alert('ERROR: Not Logged In');</script>",0);
    if(bncmp(get+n+12,puser->pwd))return mysend(ssl,Hok Hc0 Hhtml,"<script>alert('Password error!');</script>",0);
    int i=n,j;
    while(get[i]&&get[i]!='&')i++;
    for(j=i+9;get[j]&&get[j]!='&';j++);
    memset(puser->pwd,0,24);
    memcpy(puser->pwd,get+i+9,std::min(23,j-(i+9)));
    for(int i=0;i<8;i++)puser->cookie_rand[i]=rand()%26+'A';
    // memset(puser->cookie_rand,0,8);
    return mysend(ssl,Hok Hc0 Hhtml,"<script>alert('Success!');window.location.href='/login';</script>",0);
}
#endif