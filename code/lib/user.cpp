#include<math.h>
#include<string>
#include"user.h"
#include"ndb.h"
#include"cppJSON.h"
#define USER_TOHOME "<script>window.location.href='/';</script>"
#define ll long long
#include"check48.h"
ndb user/*ll->user_*/,name2id/*char[24]->ll*/;
__attribute((constructor)) void user_init(){
	ndb_init(&user,"/web/res/pri/user.dat",8,0x100000000);
	ndb_init(&name2id,"/web/res/pri/name2id.dat",24,0x10000000);//id:8
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
static inline int min(int x,int y){
    return x<y?x:y;
}
user_* getuser(const char* get){
	char* tmpp=(char*)strstr(get,"ookie: id=");
	if(tmpp){
		user_* p1=(user_*)ndb_create_binary(&user,tmpp+10,0);
		if(p1&&memcmp(tmpp+10+8,p1->cookie_rand,8)==0&&time(0)<p1->time)return p1;
	}
    return 0;
}
void login(http_para* ssl){
	char* name=ssl->get+ssl->n,*pwd=name+strlen(name)+1;
    char c[24]={0};
    memcpy(c,name,min(23,strlen(name)));
    ll* p;
	user_* p1;
	if(!(p=(ll*)ndb_create_binary(&name2id,c,0)))return http_send(ssl,Hok Hhtml Hc0,"User does not exist!",0);
	if(!(p1=(user_*)ndb_create_binary(&user,(char*)p,0)))return http_send(ssl,Hok Htxt Hc0,"server error! D4F",0);
	if(memcmp(pwd,p1->pwd,strlen(p1->pwd)))return http_send(ssl,Hok Hhtml Hc0,"Password Error!",0);
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
	if((p=(ll*)ndb_create_binary(&name2id,a.name,0)))return http_send(ssl,Hok Hhtml Hc0,"This user already exists.",0);
	if(!(p=(ll*)ndb_create_binary(&name2id,a.name,8)))return http_send(ssl,Hok Htxt Hc0,"server error! D3F",0);
	while(1){
		char tmp[8]={0};
		for(int i=0;i<8;i++)tmp[i]=rand()%26+(rand()%2?'a':'A');
		*p=*(ll*)tmp;
		if(!ndb_create_binary(&user,(char*)p,0))break;
	}
	user_* p1;
	if(!(p1=(user_*)ndb_create_binary(&user,(char*)p,sizeof(user_))))return http_send(ssl,Hok Htxt Hc0,"server error! D2F",0);
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
	if(memcmp(pwd,puser->pwd,strlen(puser->pwd)))return http_send(ssl,Hok Hc0 Htxt,"Password error!",0);
    memset(puser->pwd,0,24);
    memcpy(puser->pwd,npwd,min(23,strlen(npwd)));
	for(int i=0;i<8;i++)puser->cookie_rand[i]=rand()%26+'A';
	return http_send(ssl,Hok Hc0 Htxt,"OK",0);
}
static std::string generate_file_id() {
    char buf[17] = {0};
    for(int i = 0; i < 16; i++) {
        int v = rand() % 36;
        buf[i] = v < 10 ? '0' + v : 'a' + v - 10;
    }
    return std::string(buf);
}
void uploads_file(http_para* ssl) {
    user_* puser=getuser(ssl->get);
    if(!puser)return http_send(ssl, Hok Hc0 Hjson,"{\"error\":\"Not_Logged_In\"}", 0);
    char* file_data=ssl->get+ssl->n;
    int file_size=ssl->m;
    if(file_size<=0)return http_send(ssl, Hok Hc0 Hjson, "{\"error\":\"Empty_File\"}", 0);
    std::string file_id = generate_file_id();
    std::string full_path = "/web/res/uploads/" + file_id;
    FILE* fp = fopen(full_path.c_str(), "wb");
    if (!fp)return http_send(ssl, Hok Hc0 Hjson, "{\"error\":\"Server_Write_Error\"}", 0);
    fwrite(file_data, 1, file_size, fp);
    fclose(fp);
    std::string res = "{\"file_id\":\"" + file_id + "\"}";
    http_send(ssl, Hok Hc0 Hjson, res.c_str(), 0);
}
void download_file(http_para* ssl) {
    char* json_str = ssl->get + ssl->n;
    cppJSON json(json_str);
    if(!json.has("file_id"))return http_send(ssl, Hok Hc0 Hjson, "{\"error\":\"Invalid_Request\"}", 0);
    std::string file_id = json["file_id"].valuestring();
    if (file_id.empty() || file_id.length() > 64) {
        return http_send(ssl, Hok Hc0 Hjson, "{\"error\":\"Invalid_File_ID\"}", 0);
    }
    for (char c : file_id) {
        if (!isalnum(c)) {
            return http_send(ssl, Hok Hc0 Hjson, "{\"error\":\"Access_Denied\"}", 0);
        }
    }

    std::string full_path = "/web/res/uploads/" + file_id;
    FILE* fp = fopen(full_path.c_str(), "rb");
    if (!fp) {
        return http_send(ssl, Hok Hc0 Hjson, "{\"error\":\"File_Not_Found\"}", 0);
    }

    // 获取文件大小
    fseek(fp, 0, SEEK_END);
    long file_size = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    char* buffer = (char*)malloc(file_size);
    if (!buffer) {
        fclose(fp);
        return http_send(ssl, Hok Hc0 Hjson, "{\"error\":\"Out_Of_Memory\"}", 0);
    }

    size_t read_bytes = fread(buffer, 1, file_size, fp);
    fclose(fp);

    // 以二进制流形式发送回前端
    http_send(ssl, Hok Hc0 "Content-Type: application/octet-stream\r\n", buffer, read_bytes);
    free(buffer);
}