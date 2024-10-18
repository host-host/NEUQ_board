/* 
g++ -o /443/server2 /443/server.cpp -L /443/lib -lmyio -lNitDB -lssl
g++ -o /443/server /443/server.cpp -L /443/lib -lmyio -lNitDB -lssl
*/
#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<sys/types.h>
#include<sys/mman.h>
#include<unistd.h>
#include<fcntl.h>
#include<errno.h>
#include<pthread.h>
#include<string>
#include<map>
using std::string;
#define INIT __attribute((constructor))
#define ll long long
#include"/443/myio.h"
#include"/443/NitDB.h"
#include"/443/word.h"
#include"/443/log.h"
#include"/443/user.h"
#include"/443/chat.h"
#include"/443/sendfile.h"
void work(void* ssl,char* get,int n,int m,int ip){
    memset(get+n+m,0,200);
    #include"/443/user.h"
    #include"/443/log.h"
    #include"/443/word.h"
    #include"/443/chat.h"
    #include"/443/sendfile.h"
}
int main() {
    srand(time(0));
    mystart(999,work);
    return 0;
}