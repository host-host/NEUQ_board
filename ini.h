const char Head1[]="HTTP/1.1 200 OK\r\ncache-control: max-age=0, public\r\nContent-Length:";
const char Head2[]="HTTP/1.1 200 OK\r\nAccess-Control-Allow-Origin: *\r\nAccess-Control-Allow-Methods: POST, GET, OPTIONS\r\nAccess-Control-Allow-Credentials: false";
const char Head4[]="HTTP/1.1 302 Found\r\nLocation: http://121.36.103.216/board.html\r\nContent-Length: 0\r\nSet-Cookie: id=; Expires=Thu, 01 Jan 1970 00:00:00 GMT; Path=/; Domain=121.36.103.216";
struct point{
	const char* content,*mat;
	int n;
};
std::vector<point>e;
void add(const char* file,const char* mat,const char* head){
	char *fil=(char*)malloc(102400);//文件长度没有超过102400
	FILE*fin=fopen(file,"r");
	int n=fread(fil,1,102400,fin);
    printf("%s:%d\n",file,n);
	fclose(fin);
	char* content=(char*)malloc(n+300);//head不会超过300
    if(head)sprintf(content,"%s%d\r\n\r\n",head,n);
    int m=strlen(content);
    for(int i=0;i<n;i++)content[i+m]=fil[i];
	free(fil);
	e.push_back((point){content,mat,m+n});
}
void ini(){
    srand(time(0));
    FILE*fin=fopen("/user.txt","r");
    char ch[10240],*p=ch,*p2=ch,tmp[10240];//一个用户不会超过10k
    int t_=0;
    if(fin){
        while(1){
            if(p==p2)p2=(p=ch)+fread(ch,1,10240,fin);
            if(p==p2)break;
            if((tmp[t_++]=*p++)=='\0'){
                char* t=(char*)malloc(t_);
                memcpy(t,tmp,t_);
                t_=0;
                user.push_back(t);
            }
        }
        fclose(fin);
    }
    printf("user:%d\n",(int)user.size());
	add("/main.html","GET / ",Head1);
	add("/login.html","GET /login.html",Head1);
	add("/reg.html","GET /reg.html",Head1);
	add("/favicon.ico","GET /favicon.ico",Head1);
	add("/board.html","GET /board.html",Head1);
	e.push_back((point){(char*)login,"POST /api/login",0});
	e.push_back((point){(char*)reg,"POST /api/register",0});
	e.push_back((point){(char*)check_cookie_js,"GET /api/user",0});
	e.push_back((point){Head2,"OPTIONS",strlen(Head2)});
	e.push_back((point){Head4,"GET /logout.html",strlen(Head4)});
}