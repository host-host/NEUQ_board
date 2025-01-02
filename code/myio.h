#ifndef MYIO_
#define MYIO_
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
#ifdef __cplusplus
}
#endif
#endif