int readint(char*a){
    int x=0;
    while(*a<'0'||'9'<*a)a++;
    while('0'<=*a&&*a<='9')x=x*10+(*a++)-'0';
    return x;
}
int writeint(char* a,int x){
    if(x>9){
        int tmp=writeint(a,x/10);
        a[tmp]=x%10+'0';
        return tmp+1;
    }
    *a=x+'0';
    return 1;
}
int writestr(char*a,char*b){
    int n=0;
    for(;b[n];n++)a[n]=b[n];
    return n;
}