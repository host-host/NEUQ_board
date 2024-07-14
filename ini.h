void add(const char* file,const char* mat,const char* head){
	char *fil=(char*)malloc(102400);//文件长度没有超过102400
	FILE*fin=fopen(file,"r");
	int n=fread(fil,1,102400,fin);
    printf("%s:%d\n",file,n);
	fclose(fin);
	char* content=(char*)malloc(n+300);//head不会超过300
    sprintf(content,"%s%d\r\n\r\n",head,n);
    int m=strlen(content);
    memcpy(content+m,fil,n);
	free(fil);
	e.push_back((point){content,mat,m+n});
}
void init(){
    int fil=open("/user.txt",O_RDWR|O_CREAT);
    user=(char(*)[128])mmap(0,128*200000,PROT_READ|PROT_WRITE,MAP_SHARED,fil,0);
    for(int i=0;i<200000;i++){
        if(user[i][0]==0)break;
        users[(std::string)user[i]]=i;
    }
    printf("user:%d\n",(int)users.size());
	add("/main.html","GET / ",Head1);
	add("/login.html","GET /login.html",Head1);
	add("/reg.html","GET /reg.html",Head1);
	add("/favicon.ico","GET /favicon.ico",Head1);
	add("/board.html","GET /board.html",Head1);
	add("/profile.html","GET /profile.html",Head1);
	add("/common.js","GET /common.js",Head1);
	e.push_back((point){(char*)login,"POST /api/login",0});
	e.push_back((point){(char*)reg,"POST /api/register",0});
	e.push_back((point){(char*)check_cookie_js,"GET /api/user",0});
	e.push_back((point){(char*)change_password,"POST /api/change_password",0});
	e.push_back((point){(char*)getp,"GET /p=",0});
	e.push_back((point){(char*)getcon,"GET /con=",0});
	e.push_back((point){Head2,"OPTIONS",strlen(Head2)});
	e.push_back((point){Head4,"GET /logout.html",strlen(Head4)});
    srand(time(0));
}