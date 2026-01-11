#ifndef MYIO_
#define MYIO_
#include<stdio.h>
#include<stdlib.h>
#ifdef __cplusplus
extern "C"{
#endif
#define INIT __attribute((constructor))
#define LOG(a,...) printf("<%s : %d>%s : " a "\n",__FILE__,__LINE__,__func__,##__VA_ARGS__)
#define ll long long
int min(int x,int y){
    return x<y?x:y;
}
int max(int x,int y){
    return x>y?x:y;
}
int ncmp(const char* a,const char* b,int n){
    for(int i=0;i<n;i++)if(a[i]!=b[i])return a[i]-b[i];
    return 0;
}
int bncmp(const char* a,const char* b){
    for(int i=0;b[i];i++)if(a[i]!=b[i])return a[i]-b[i];
    return 0;
}
ll readll(const char* a){
    ll x=0;
	while(*a!='-'&&(*a<'0'||*a>'9'))a++;
	if(*a=='-') {
		for(a++;*a>='0'&&*a<='9'; a++)x=x*10+*a-'0';
		return -x;
	}
    while(*a>='0'&&*a<='9')x=x*10+*a++-'0';
	return x;
}
void myJSON(const char* p,char* a){
    int i=0,bj=0;
    if(p[0]=='\"')p++;
    a[i++]='\"';
    while(*p){
        if((*p)=='\\'){
            char nex=*(p+1);
            if(nex=='\"'||nex=='\\'||nex=='/')a[i++]=*p++;
            else a[i++]='\\';
            a[i++]=*p++;
        }else{
            if(*p=='\"'){
                if(*(p+1))a[i++]='\\';
                else bj=1;
            }
            a[i++]=*p++;
        }
    }
    if(bj==0)a[i++]='\"';
    a[i]=0;
}
char* readfile(const char* filename) {
    FILE* fp = fopen(filename, "r");
    if (!fp)return 0;
    fseek(fp, 0, SEEK_END);
    long fileSize = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    char* buffer = (char*)malloc(fileSize + 1);
    if (!buffer) {
        fclose(fp);
        return NULL;
    }
    size_t bytesRead = fread(buffer, 1, fileSize, fp);
    if ((long )bytesRead != fileSize) {
        free(buffer);
        fclose(fp);
        return NULL;
    }
    buffer[fileSize] = '\0';
    fclose(fp);
    return buffer;
}
int writefile(const char* filename, const char* data, int n) {
    FILE* fp = fopen(filename, "w");
    if (!fp) {
        return 1;
    }
    size_t bytesWritten = fwrite(data, 1, n, fp);
    if (bytesWritten != (size_t)n) {
        fclose(fp);
        return 1;
    }

    fclose(fp);
    return 0; // 成功
}
#ifdef __cplusplus
}
#endif
#endif