
void add(const char* file,const char* mat,const char* head){
	char *fil=(char*)malloc(102400);
	FILE*fin=fopen(file,"r");
	int n=fread(fil,1,102400,fin);
    printf("%s:%d\n",file,n);
	fclose(fin);
	char* content=(char*)malloc(n+300);
    if(head)sprintf(content,"%s%d\r\n\r\n",head,n);
    int m=strlen(content);
    for(int i=0;i<n;i++)content[i+m]=fil[i];
	free(fil);
	e.push_back((point){content,mat,m+n});
}
void ini(){
    FILE*fin=fopen("/user.txt","r");
    char ch[10240],*p=ch,*p2=ch;
    char tmp[1024],t_;
    if(fin){
        while(1){
            if(p==p2)p2=(p=ch)+fread(ch,1,10240,fin);
            if(p==p2)break;
            char temp=*p++;
            if(temp=='\0'){
                char* t=(char*)malloc(t_+1);
                memcpy(t,tmp,t_);
                t_=0;
                user.push_back(t);
            }else tmp[t_++]=temp;
        }
        fclose(fin);
    }
    printf("user:%d\n",(int)user.size());
	add("/main.html","GET / ",Head1);
	add("/login.html","GET /login.html",Head1);
	add("/reg.html","GET /reg.html",Head1);
	add("/favicon.ico","GET /favicon.ico",Head3);
	add("/board.html","GET /board.html",Head1);
	e.push_back((point){(char*)login,"POST /api/login",0});
	e.push_back((point){(char*)reg,"POST /api/register",0});
	e.push_back((point){Head2,"OPTIONS",strlen(Head2)});
	// e.push_back((point){Head404,"GET",strlen(Head404)});
}
