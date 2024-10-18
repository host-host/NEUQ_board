#ifndef SENDFILE_
#define SENDFILE_
int ifac[256];
INIT void sendfile_init(){
    for(char i='0';i<='9';i++)ifac[i]=1;
    for(char i='A';i<='Z';i++)ifac[i]=1;
    for(char i='a';i<='z';i++)ifac[i]=1;
    ifac['/']=ifac['_']=ifac['-']=ifac['.']=1;
}
int mysendfile(void* ssl, const char* a){
    FILE* file=fopen(a,"rb");
    if(!file)return 0;
    fseek(file,0,SEEK_END);
    int fileSize=ftell(file);
    fseek(file,0,SEEK_SET);
    const char* head=Htxt;
    if(strstr(a,".html"))head=Hhtml;
    if(strstr(a,".js"))head=Hjs;
    if(strstr(a,".json"))head=Hjson;
    if(strstr(a,".css"))head=Hcss;
    if(strstr(a,".ico"))head=Hico;
    if(strstr(a,".webp"))head=Hwebp;
    if(strstr(a,".mp4"))head=Hmp4;
    char* content=(char*)malloc(fileSize+400);
    int n=sprintf(content,"%s%s%s",Hok,Hc1h,head);
    if(strstr(a,"/gzip/"))n+=sprintf(content+n,Hgzip);
    else if(strstr(a,"/zstd/"))n+=sprintf(content+n,Hzstd);
    else if(strstr(a,"/br/"))n+=sprintf(content+n,Hbr);
    if(strstr(a,"/file/"))n+=sprintf(content+n,Hdown);
    if(fread(content+n+1,1,fileSize,file)!=fileSize) {
        free(content);
        fclose(file);
        return 0;
    }
    mysend(ssl,content,content+n+1,fileSize);
    free(content);
    fclose(file);
    return 1;
}
#else
string root="/443/www";
if(strstr(get,": free.neuqboard.cn"))root="/443/free";
if(strstr(get,": chat.neuqboard.cn"))root="/443/chat";
if(strstr(get,": dev.neuqboard.cn"))root="/443/dev";
if(strstr(get,": file.neuqboard.cn"))root="/443/file";
if(bncmp(get,"GET ")==0){
    char file[128];
    for(n=1;n<127&&((file[n]=get[n+3])!=46||file[n-1]!=46)&&ifac[(unsigned char)file[n]];n++);
    file[n]=0;
    const char* a=file+1;
    if(a[0]){
        string b=root+"/html"+a+(a[strlen(a)-1]=='/'?"index.html":".html");
        // printf("?1\n");
        if(mysendfile(ssl,b.c_str()))return;
        b=root+a;
        // printf("?2\n");
        if(mysendfile(ssl,b.c_str()))return;
    }
        // printf("?3\n");
    mysend(ssl,H404,"",0);
        // printf("?4\n");
}
#endif