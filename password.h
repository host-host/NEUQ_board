void login(int cl,const char* re,const char* con,int n,const char* id){
    char c[72];
    memset(c,0,72);
    int i=4;
    for(;con[i];i++)if(con[i]=='&'){
            if(i-4<1||i-4>35)return mysend(cl,"<script>alert('Username length error!');</script>");
            memcpy(c,con+4,i-4);
            if(!users.count((std::string)c))return mysend(cl,"<script>alert('User does not exist!');</script>");
            break;
        }
    memcpy(c+36,con+i+5,min(35,n-(i+5)));
    char* w=user[users[(std::string)c]];
    for(int j=36;j<72;j++)if(w[j]!=c[j])i=-1;
    if(i==-1)return mysend(cl,"<script>alert('Password error!');</script>");
    char X4[]="HTTP/1.1 200 OK\r\ncache-control: max-age=0, public\r\nContent-Length:75\r\nSet-Cookie: id=          ; Max-Age=604800; Domain=121.36.103.216; Path=/; HttpOnly\r\n\r\n<script>window.location.href = 'http://121.36.103.216/board.html';</script>";
    memcpy(X4+85,w+72,10);
    write(cl,X4,strlen(X4));
}
void reg(int cl,const char* re,const char* con,int n,const char* id){
    char c[128];
    memset(c,0,128);
    int i=4,j;
    for(;con[i];i++)if(con[i]=='&'){
            if(i-4<1||i-4>35)return mysend(cl,"<script>alert('Username length error!');</script>");
            memcpy(c,con+4,i-4);
            if(users.count((std::string)c))return mysend(cl,"<script>alert('This user already exists.');</script>");
            break;
        }
    for(j=i+5;con[j]&&con[j]!='&';j++);
    memcpy(c+36,con+i+5,min(35,j-(i+5)));
    while(con[j]&&con[j]!='=')j++;
    if(con[j])j++;
    while(con[j]&&con[j]!='=')j++;
    memcpy(c+82,con+j,min(46,strlen(con+j)));
    for(i=77;i<82;i++)c[i]=rand()%26+(rand()%2?'a':'A');
    int x=users.size(),tx=x;
    users.insert(std::pair<std::string,int>(c,x));
    for(i=76;i>=72;tx/=26)c[i--]='A'+tx%26;
    memcpy(user[x],c,128);
    mysend(cl,"<script>alert('Success, please log in.');window.location.href = 'http://121.36.103.216/login.html';</script>");
}
void check_cookie_js(int cl,const char* re,const char* con,int n,const char* id){
    int x=0,i=0;
    for(;i<5;i++)x=x*26+id[i]-'A';
    if(0<=x&&x<users.size()){
        for(int j=5;j<10;j++)if(id[j]!=user[x][72+j])i=0;
        if(i==5)return mysend(cl,user[x]);
    }
    mysend(cl,"Not_Logged_In");
}
void change_password(int cl,const char* re,const char* con,int n,const char* id){
    if(id[0]=='0')return mysend(cl,"<script>alert('ERROR: Not Logged In');</script>");
    int x=0,i=0,j;
    for(;i<5;i++)x=x*26+id[i]-'A';
    if(0<=x&&x<users.size()){
        for(;i<10;i++)if(id[i]!=user[x][72+i])return mysend(cl,"<script>alert('ERROR: Something wrong.Code CU31');</script>");
        for(i=0;user[x][36+i];i++)if(con[12+i]!=user[x][36+i])break;
        if(user[x][36+i]||con[12+i]!='&')return mysend(cl,"<script>alert('Password error!');</script>");
        for(j=i+21;con[j]&&con[j]!='&';)j++;
        memset(user[x]+36,0,36);
        memcpy(user[x]+36,con+i+21,min(35,j-(i+21)));
        for(int i=77;i<82;i++)user[x][i]=rand()%26+(rand()%2?'a':'A');
        return mysend(cl,"<script>alert('Success!');window.location.href = 'http://121.36.103.216/login.html';</script>");
    }
    mysend(cl,"<script>alert('ERROR: Something wrong.Code WD20');</script>");
}