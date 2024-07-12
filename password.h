char X1[]="<script>alert('ERROR: This user already exists.');</script>";
char X2[]="<script>alert('Login failed, please try again.');</script>";
char X3[]="<script>alert('ERROR: Something wrong.');</script>";
char X5[]="<script>alert('Success, please log in.');window.location.href = 'http://121.36.103.216/login.html';</script>";
std::vector<char*>user;
void login(char*a,int cl){
    char *c=(char*)malloc(4096);//a不会超过4000
    int n=0,bj=0;
    for(int i=4;a[i];i++)if(a[i]=='&'){
            if(++bj==2)break;
            i+=4;
            c[n++]='\n';
        }else c[n++]=a[i];
    if(bj!=1)goto out2;
    for(int i=c[n]=0,j;i<user.size();i++){
        for(j=0;user[i][j]&&c[j];j++)if(user[i][j]!=c[j])break;
        if(c[j]==0){
            char X4[]="HTTP/1.1 302 Found\r\nLocation: http://121.36.103.216/board.html\r\nContent-Length: 0\r\nSet-Cookie: id=                    ; Max-Age=604800; Domain=121.36.103.216; Path=/; HttpOnly";
            for(int k=0,len=strlen(X4);k<20;k++)X4[len-20-57+k]=user[i][j+1+k];
            free(c);
            write(cl,X4,strlen(X4));
            return;
        }
    }
    out2:
    free(c);
    write(cl,X2,strlen(X2));
    return;
}
volatile int u=0;
void reg(char*a,int cl){
    char *c=(char*)malloc(4196*2);//a不会超过4000
    int n=0,bj=0;
    for(int i=4;a[i];i++)if(a[i]=='&'){
            if(++bj==2)break;
            if(n>20){
                write(cl,X3,strlen(X3));
                free(c);
                return;
            }
            i+=4;
            c[n++]='\n';
        }else c[n++]=a[i];
    if(bj!=2){
        write(cl,X3,strlen(X3));
        free(c);
        return;
    }
    c[n++]='\n';
    for(int i=1;i<=20;i++)c[n++]=rand()%26+'A';
    c[n++]='\n';
    memcpy(c+n,a,strlen(a));
    n+=strlen(a);
    c[n]=0;
    for(int i=0,j;i<user.size();i++){
        for(j=0;user[i][j]&&c[j]!='\n';j++)if(user[i][j]!=c[j])break;
        if(c[j]=='\n'){
            write(cl,X1,strlen(X1));
            free(c);
            return;
        }
    }
    while(u)usleep(100000);
    u=1;
    user.push_back(c);
    FILE*fout=fopen("/user.txt","a");
    fwrite(c,1,n+1,fout);
    fclose(fout);
    u=0;
    write(cl,X5,strlen(X5));
    return;
}
void check_cookie_js(char*a,int cl){
    char id[25]="01234567890123456789",MAT[]="Cookie: id=";
    for(int j=0,k;a[j];j++){
        for(k=0;MAT[k];k++)if(MAT[k]!=a[j+k])break;
        if(!MAT[k]){
            for(int w=0;w<20;w++)id[w]=a[j+k+w];
            break;
        }
    }
    for(int i=0;i<user.size();i++){
        char* c=user[i];
        int bj=0;
        for(;*c&&bj!=2;c++)if(*c=='\n')bj++;
        bj=1;
        for(int j=0;j<20;j++)if(id[j]!=c[j]){
            bj=0;
            break;
        }
        if(bj==1){
            int n=0;
            while(user[i][n]!='\n')n++;
            write(cl,user[i],n);
            return;
        }
    }
    write(cl,"Not_Logged_In",13);
}