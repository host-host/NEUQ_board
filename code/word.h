#ifndef WORD_
#define WORD_
#include<vector>
using std::string;
int lastword=-1;
// #define path "./res/pri/"
struct point{
    string a,b;
    int n;
};
std::vector<point>vv;
#define ll long long
string Getword(){
    vv.clear();
    FILE*fin=fopen("./res/pri/data2.txt","r");
    if(fin==0)return "OPEN ERROR";
    int n=0;
    char c[256],c1[256];
    while(fscanf(fin,"%s%d%s",c,&n,c1)==3){
        point e;
        e.a=c;
        e.b=c1;
        e.n=n;
        vv.push_back(e);
    }
    fclose(fin);
    double all=0,s=0;
    for(int i=0;i<(int)vv.size();i++)all+=pow(0.4,vv[i].n);
    double now=all*rand()/RAND_MAX;
    for(int i=0;i<(int)vv.size();i++){
        s+=pow(0.4,vv[i].n);
        if(s>=now)return (string)"{\"id\":\""+std::to_string(i)+"\",\"name\":\"" +vv[i].a+"\",\"value\":\""+std::to_string(vv[i].n)+" "+vv[i].b+ "\"}";
    }
    return "";
}
string Setword(const char* a){
    int u=readll(a);
    if(strstr(a,"subtract\"}")){
        vv[u].n++;
        if(vv[u].n==0)vv[u].n=1;
    }
    if(strstr(a,"add\"}")){
        vv[u].n--;
        if(vv[u].n>-7)vv[u].n=-7;
    }
    FILE*fout=fopen("./res/pri/data2.txt","w");
    if(fout){
        for(int i=0;i<(int)vv.size();i++)fprintf(fout,"%s %d %s\n",vv[i].a.c_str(),vv[i].n,vv[i].b.c_str());
        fclose(fout);
        return "OK";
    }
    return "cannot save";
}
void getword(http_para *ssl){
    user_ *puser=getuser(ssl->get);
    if(puser&&puser->admin)return http_send(ssl,Hok Hc0 Hjson,Getword().c_str(),0);
}
void setword(http_para *ssl){
    user_ *puser=getuser(ssl->get);
    if(puser&&puser->admin)return http_send(ssl,Hok Hc0 Htxt,Setword(ssl->get+ssl->n).c_str(),0);
}
#endif