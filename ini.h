
void add(const char* file,const char* mat,const char* head){
	char *fil=(char*)malloc(102400);
	FILE*fin=fopen(file,"r");
	int n=fread(fil,1,102400,fin);
    printf("%s:%d\n",file,n);
	fclose(fin);
	char* content=(char*)malloc(n+100);
    if(head)sprintf(content,"%s%d\r\n\r\n",head,n);
    int m=strlen(content);
    for(int i=0;i<n;i++)content[i+m]=fil[i];
	free(fil);
	e.push_back((point){content,mat,m+n});
}
void ini(){
	// add("/index_error.html","ERROR",0);
	add("/main.html","GET / ",Head1);
	add("/login.html","GET /login.html",Head1);
	add("/reg.html","GET /reg.html",Head1);
	add("/favicon.ico","GET /favicon.ico",Head3);
	add("/board.html","GET /board.html",Head1);
	e.push_back((point){0,"POST /api/login",0});
	e.push_back((point){Head2,"OPTIONS",strlen(Head2)});
}