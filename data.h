void getp(int cl,const char* re,const char* con,int n,const char* id){
    mysend(cl,"[{\"name\":\"1\",\"date\":\"123\",\"title\":\"1234\",\"id\":\"1\"},{\"name\":\"A\",\"date\":\"B\",\"title\":\"C\",\"id\":\"1\"}]");
}
void getcon(int cl,const char* re,const char* con,int n,const char* id){
    mysend(cl,"这是内容");
}