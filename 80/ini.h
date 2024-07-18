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
int fil;
void init(){
    // char c[35];
    // memset(c,0,sizeof(c));
    // *(int*)(c+1)=1;
    // FILE* fout=fopen("/80/data.dat","w");
    // fwrite(c,1,35,fout);fclose(fout);
    // fout=fopen("/80/cont.dat","w");
    // fwrite(c,1,10,fout);fclose(fout);
    // exit(0);
    fil=open("/80/user.txt",O_RDWR|O_CREAT);
    user=(char(*)[128])mmap(0,128ll*26*26*26*26*26,PROT_READ|PROT_WRITE,MAP_SHARED,fil,0);
    for(int i=0;i<200000;i++){
        if(user[i][0]==0)break;
        users[(std::string)user[i]]=i;
    }
    printf("user:%d\n",(int)users.size());
    ldata=lseek(fdata=open("/80/data.dat",O_RDWR|O_APPEND|O_CREAT),0,SEEK_END);
    data=(char*)mmap(0,4ll*1024*1024*1024,PROT_READ|PROT_WRITE,MAP_SHARED,fdata,0);
    printf("data:%lld\n",ldata);
    lcont=lseek(fcont=open("/80/cont.dat",O_RDWR|O_APPEND|O_CREAT),0,SEEK_END);
    cont=(char*)mmap(0,20ll*1024*1024*1024,PROT_READ|PROT_WRITE,MAP_SHARED,fcont,0);
    printf("content:%lld\n",lcont);
    // ll t=lcont+337
	add("/80/.html","GET / ",Head1);
	add("/80/main.html","GET /m",Head1);
	add("/80/login.html","GET /login.html",Head1);
	add("/80/reg.html","GET /reg.html",Head1);
	add("/80/favicon.ico","GET /favicon.ico",Head1);
	add("/80/board.html","GET /board.html",Head1);
	add("/80/profile.html","GET /profile.html",Head1);
	add("/80/common.js","GET /common.js",Head1);
	e.push_back((point){(char*)login,"POST /api/login",0});
	e.push_back((point){(char*)reg,"POST /api/register",0});
	e.push_back((point){(char*)check_cookie_js,"GET /api/user",0});
	e.push_back((point){(char*)change_password,"POST /api/change_password",0});
	e.push_back((point){(char*)getp,"GET /p=",0});
	e.push_back((point){(char*)getcon,"GET /con=",0});
	e.push_back((point){(char*)postmsg,"POST /api/sendmessage",0});
	e.push_back((point){Head2,"OPTIONS",strlen(Head2)});
	e.push_back((point){Head4,"GET /logout.html",strlen(Head4)});
    srand(time(0));
}