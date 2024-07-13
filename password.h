std::vector<char*>user;
void login(const char*a,int cl,const char* id){
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
        if(c[j]==0&&user[i][j]=='\n'){
            char X4[]="HTTP/1.1 200 OK\r\ncache-control: max-age=0, public\r\nContent-Length:75\r\nSet-Cookie: id=                    ; Max-Age=604800; Domain=121.36.103.216; Path=/; HttpOnly\r\n\r\n<script>window.location.href = 'http://121.36.103.216/board.html';</script>";
            for(int k=0,len=strlen(X4);k<20;k++)X4[len-20-136+k]=user[i][j+1+k];
            free(c);
            write(cl,X4,strlen(X4));
            return;
        }
    }
    out2:
    free(c);
    send(cl,"<script>alert('Login failed, please try again.');</script>");
    return;
}
volatile int u=0,u1=0;
void reg(const char*a,int cl,const char* id){
    char *c=(char*)malloc(4196*2);//a不会超过4000
    int n=0,bj=0;
    for(int i=4;a[i];i++)if(a[i]=='&'){
            if(bj==0&&n==0)break;
            if(bj==1&&c[n-1]=='\n')break;
            if(++bj==2)break;
            i+=4;
            c[n++]='\n';
        }else c[n++]=a[i];
    for(int i=0;c[i]!='\n';i++)if(c[i]==' '){
        send(cl,"<script>alert('ERROR: The username should not contain spaces');</script>");
        free(c);
        return;
    }
    if(n>50||bj!=2){
        send(cl,"<script>alert('ERROR: Something wrong.Code WD32');</script>");
        free(c);
        return;
    }
    c[n++]='\n';
    for(int i=1;i<=20;i++)c[n++]=rand()%26+'A';
    c[n++]='\n';
    memcpy(c+n,a,strlen(a));
    c[n+=strlen(a)]=0;
    for(int i=0,j;i<user.size();i++){
        for(j=0;user[i][j]&&c[j]!='\n';j++)if(user[i][j]!=c[j])break;
        if(c[j]=='\n'&&user[i][j]=='\n'){
            send(cl,"<script>alert('ERROR: This user already exists.');</script>");
            free(c);
            return;
        }
    }
    while(u)sleep(1);
    u=1;
    user.push_back(c);
    FILE*fout=fopen("/user.txt","a");
    fwrite(c,1,n+1,fout);
    fclose(fout);
    u=0;
    send(cl,"<script>alert('Success, please log in.');window.location.href = 'http://121.36.103.216/login.html';</script>");
    return;
}
void check_cookie_js(const char*a,int cl,const char* id){
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
            send(cl,user[i],n);
            return;
        }
    }
    send(cl,"Not_Logged_In");
}
void* savedata(void* a){
    while(1){
        sleep(60);
        if(u1){
            while(u)sleep(1);
            u=1;
            FILE*fout=fopen("/user.txt","w");
            for(int i=0;i<user.size();i++)fwrite(user[i],1,strlen(user[i])+1,fout);
            fclose(fout);
            u=u1=0;
        }
    }
}
void change_password(const char* a,int cl,const char* id){
    if(id[0]=='0')return send(cl,"<script>alert('ERROR: Not Logged In');</script>");
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
            const char* p=user[i];
            while(*p++!='\n');
            int j=0;
            for(;p[j]!='\n'&&a[12+j];j++)if(a[12+j]!=p[j])break;
            if(p[j]=='\n'&&a[12+j]=='&'){
                a=a+12+j+9;
                if(*a=='&')return send(cl,"<script>alert('ERROR: Something wrong.Code WD33');</script>");
                char* newd=(char*)malloc(strlen(user[i])+50);
                int tn=p-user[i],k=0;
                memcpy(newd,user[i],tn);
                for(;k<=50&&a[k]!='&'&&a[k];k++)newd[tn++]=a[k];
                if(tn>50){
                    free(newd);
                    return send(cl,"<script>alert('ERROR: Something wrong.Code WD34');</script>");
                }
                newd[tn++]='\n';
                for(int w=1;w<=20;w++)newd[tn++]=rand()%26+'A';
                newd[tn++]='\n';
                while(*p&&*p++!='\n');
                while(*p&&*p++!='\n');
                memcpy(newd+tn,p,strlen(p)+1);
                free(user[i]);
                user[i]=newd;
                u1=1;
                return send(cl,"<script>alert('Success!');window.location.href = 'http://121.36.103.216/board.html';</script>");
            }
            return send(cl,"<script>alert('Password error!');</script>");
        }
    }
    send(cl,"<script>alert('ERROR: Something wrong.Code WD20');</script>");
}