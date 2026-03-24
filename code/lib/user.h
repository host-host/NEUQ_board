#ifndef USER_
#define USER_
#include<string>
#include<math.h>
#include"lib/http.h"
#include"lib/ndb.h"
#define USER_TOHOME "<script>window.location.href='/';</script>"
#include"check48.h"
ndb user/*ll->user_*/,name2id/*char[24]->ll*/;
struct user_{
	char name[24],pwd[24],cookie_rand[8];
	int admin,time;
	char email[80],phone[20];
};
INIT void user_init(){
	ndb_init(&user,"./res/pri/user.dat",8,0x100000000);
	ndb_init(&name2id,"./res/pri/name2id.dat",24,0x10000000);//id:8
}
inline char* read_user(ll* a,char* b){
	while(*b&&(*b<'0'||*b>'9')&&*b!='-')b++;
	ll x=0,f=1;
	if(*b=='-'){
		f=-1;
		b++;
	}
	while(*b&&(*b>='0'&&*b<='9'))x=x*10+*b++-'0';
	*a=x*f;
	return b;
}
user_* getuser(const char* get){
	char* tmpp=(char*)strstr(get,"ookie: id=");
	if(tmpp){
		user_* p1=(user_*)ndb_create(&user,tmpp+10,0);
		// if(p1)LOG("%s",p1->cookie_rand);/
		if(p1&&ncmp(tmpp+10+8,p1->cookie_rand,8)==0&&time(0)<p1->time)return p1;
	}
    return 0;
}
void login(http_para* ssl){
	char* name=ssl->get+ssl->n,*pwd=name+strlen(name)+1;
    char c[24]={0};
    memcpy(c,name,min(23,strlen(name)));
    ll* p;
	user_* p1;
	if(!(p=(ll*)ndb_create(&name2id,c,0)))return http_send(ssl,Hok Hhtml Hc0,"User does not exist!",0);
	if(!(p1=(user_*)ndb_create(&user,(char*)p,0)))return http_send(ssl,Hok Htxt Hc0,"server error! D4F",0);
	if(bncmp(pwd,p1->pwd))return http_send(ssl,Hok Hhtml Hc0,"Password Error!",0);
    char tmp[]="Set-Cookie: id=1234567812345678; Max-Age=604800; Path=/; Secure; HttpOnly\r\n";
    memcpy(tmp+15,p,8);
    if(time(0)>p1->time){
        for(int i=0;i<8;i++)p1->cookie_rand[i]=rand()%26+'A';
        p1->time=time(0)+604800;
    }
    memcpy(tmp+15+8,p1->cookie_rand,8);
    return http_send(ssl,((std::string)Hok+tmp+Hc0+Htxt).c_str(),"OK",0);
}
void reg(http_para* ssl){
	char* name=ssl->get+ssl->n,*pwd=name+strlen(name)+1,*email=pwd+strlen(pwd)+1,*phone=email+strlen(email)+1,*check=phone+strlen(phone)+1;
	{//check
		ll n,m,a[CHECK_LEN],p;
		check=read_user(&n,check);
		check=read_user(&m,check);
		if(n!=CHECK_LEN)return http_send(ssl,Hok Hc0 Htxt,"Check robot error1!",0);
		p=CHECK_LEN+rand_base1;
		p=p*rand_base+m+rand_base1;
		for(int i=0;i<CHECK_LEN;i++){
			check=read_user(a+i,check);
			p=p*rand_base+a[i]+rand_base1;
		}
		if(!check_48_(p))return http_send(ssl,Hok Hc0 Htxt,"Check robot error2!",0);
		check=read_user(&p,check);
		n=0;
		for(int i=0;i<CHECK_LEN;i++)if(p>>i&1)n+=a[i];
		if(n!=m)return http_send(ssl,Hok Hc0 Htxt,"Check robot error3!",0);
	}
	if(strlen(name)<1||strlen(name)>23)return http_send(ssl,Hok Hhtml Hc0,"Username length too long!",0);
	for(int i=0;i<(int)strlen(name);i++){
		if('0'<=name[i]&&name[i]<='9')continue;
		if('a'<=name[i]&&name[i]<='z')continue;
		if('A'<=name[i]&&name[i]<='Z')continue;
		if(name[i]!='_')return http_send(ssl,Hok Hhtml Hc0,"用户名只能包含大小写字母、数字和下划线",0);
	}
	user_ a;
	ll* p;
	memset(&a,0,sizeof(a));
	memcpy(a.name,name,strlen(name));
	memcpy(a.pwd,pwd,min(strlen(pwd),23));
	memcpy(a.email,email,min(strlen(email),79));
	memcpy(a.phone,phone,min(strlen(phone),19));
	if((p=(ll*)ndb_create(&name2id,a.name,0)))return http_send(ssl,Hok Hhtml Hc0,"This user already exists.",0);
	if(!(p=(ll*)ndb_create(&name2id,a.name,8)))return http_send(ssl,Hok Htxt Hc0,"server error! D3F",0);
	while(1){
		char tmp[8]={0};
		for(int i=0;i<8;i++)tmp[i]=rand()%26+(rand()%2?'a':'A');
		*p=*(ll*)tmp;
		if(!ndb_create(&user,(char*)p,0))break;
	}
	user_* p1;
	if(!(p1=(user_*)ndb_create(&user,(char*)p,sizeof(user_))))return http_send(ssl,Hok Htxt Hc0,"server error! D2F",0);
	memcpy(p1,&a,sizeof(user_));
	http_send(ssl,Hok Hhtml Hc0,"OK",0);
}
void logout(http_para *ssl){
    return http_send(ssl,Hok Hc0 Hhtml "Set-Cookie: id=; Expires=Thu, 01 Jan 1970 00:00:00 GMT; Path=/;\r\n",USER_TOHOME,0);
}
void apiuser(http_para* ssl){
    user_* puser=getuser(ssl->get);
	if(puser)return http_send(ssl,Hok Hc0,puser->name,0);
	else return http_send(ssl,Hok Hc0,"Not_Logged_In",0);
}
void change_password(http_para* ssl){
    user_* puser=getuser(ssl->get);
	if(!puser)return http_send(ssl,Hok Hc0 Htxt,"Please log in first.",0);
	char* pwd=ssl->get+ssl->n,*npwd=pwd+strlen(pwd)+1;
	if(bncmp(pwd,puser->pwd))return http_send(ssl,Hok Hc0 Htxt,"Password error!",0);
    memset(puser->pwd,0,24);
    memcpy(puser->pwd,npwd,min(23,strlen(npwd)));
	for(int i=0;i<8;i++)puser->cookie_rand[i]=rand()%26+'A';
	return http_send(ssl,Hok Hc0 Htxt,"OK",0);
}
#endif