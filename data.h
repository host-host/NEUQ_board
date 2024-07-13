void getp(const char* a,int cl,const char* id){
    send(cl,"[{\"name\":\"1\",\"date\":\"123\",\"title\":\"1234\",\"id\":\"1\"},{\"name\":\"A\",\"date\":\"B\",\"title\":\"C\",\"id\":\"1\"}]");
}
void getcon(const char* a,int cl,const char* id){
    send(cl,"这是内容");
}