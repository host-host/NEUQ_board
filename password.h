char X1[]="<script>alert('ERROR: This user already exists.');</script>";
char X2[]="<script>alert('Login failed, please try again.');</script>";
char X3[]="<script>document.cookie='";
char X4[]="';window.location.href = 'http://121.36.103.216/board.html';</script>";
char X5[]="<script>alert('Success, please log in.');window.location.href = 'http://121.36.103.216/login.html';</script>";
std::vector<char*>user;
void login(char*a,int cl){
    char c[4196];
    int n=0,bj=0;
    for(int i=4;a[i];i++){
        if(a[i]=='&'){
            if(bj==1)break;
            bj++;
            i+=4;
            c[n++]='\n';
            continue;
        }
        c[n++]=a[i];
    }
    c[n]=0;
    for(int i=0;i<user.size();i++){
        int bj=1,j=0;
        for(;user[i][j]&&c[j];j++)if(user[i][j]!=c[j]){bj=0;break;}
        if(bj==1){
            write(cl,X3,strlen(X3));
            write(cl,user[i]+j+1,20);
            write(cl,X4,strlen(X4));
            return;
        }
    }
    write(cl,X2,strlen(X2));
}
volatile int u=0;
void reg(char*a,int cl){
    char *c=(char*)malloc(4196);
    int n=0,bj=0;
    for(int i=4;a[i];i++){
        if(a[i]=='&'){
            if(bj==1)break;
            bj++;
            i+=4;
            c[n++]='\n';
            continue;
        }
        c[n++]=a[i];
    }
    c[n]='\n';
    memcpy(c+n+1,a,strlen(a));
    for(int i=0;i<user.size();i++){
        int bj=1;
        for(int j=0;user[i][j]&&c[j]!='\n';j++)if(user[i][j]!=c[j]){bj=0;break;}
        if(bj){
            write(cl,X1,strlen(X1));
            free(c);
            return;
        }
    }
    while(u)usleep(100000);
    u=1;
    user.push_back(c);
    FILE*fout=fopen("/user.txt","a");
    fwrite(c,1,strlen(c)+1,fout);
    fclose(fout);
    u=0;
    write(cl,X5,strlen(X5));
    return;
}
