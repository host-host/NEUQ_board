#ifndef SENDFILE_
#define SENDFILE_



// #ifdef __cplusplus
// extern "C"{
// #endif
// #include"log.h"
using std::string;
int ifac[256];
INIT void sendfile_init(){
    for(int i='0';i<='9';i++)ifac[i]=1;
    for(int i='A';i<='Z';i++)ifac[i]=1;
    for(int i='a';i<='z';i++)ifac[i]=1;
    ifac['/']=ifac['_']=ifac['-']=ifac['.']=1;
}
int mysendfile(https_para* ssl, const char* a){
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
    if((int)fread(content+n+1,1,fileSize,file)!=fileSize) {
        free(content);
        fclose(file);
        return 0;
    }
    https_send(ssl,content,content+n+1,fileSize);
    free(content);
    fclose(file);
    return 1;
}
void sendfile(https_para *ssl){
    string root="./res/www";
    if(strstr(ssl->get,": free.neuqboard.cn"))root="./res/free";
    if(strstr(ssl->get,": chat.neuqboard.cn"))root="./res/chat";
    if(strstr(ssl->get,": dev.neuqboard.cn"))root="./res/dev";
    if(strstr(ssl->get,": file.neuqboard.cn"))root="./res/file";
    if(bncmp(ssl->get,"GET ")==0){
        int n;
        char file[128];
        for(n=1;n<127&&((file[n]=ssl->get[n+3])!=46||file[n-1]!=46)&&ifac[(unsigned char)file[n]];n++);
        file[n]=0;
        const char* a=file+1;
        if(a[0]){
            string b=root+"/html"+a+(a[strlen(a)-1]=='/'?"index.html":".html");
            if(mysendfile(ssl,b.c_str()))goto https;
            b=root+a;
            if(mysendfile(ssl,b.c_str()))goto https;
        }
        https_send(ssl,H404,"",0);
    }
    return;
    https://free.neuqboard.cn
    char tmp[60]={0};
    memcpy(tmp,ssl->get,59);
    for(int i=0;i<59;i++)if(tmp[i]=='\r'||tmp[i]=='\n')tmp[i]=0;
    // addlog(tmp);
}

// #ifdef __cplusplus
// }
// #endif
#endif