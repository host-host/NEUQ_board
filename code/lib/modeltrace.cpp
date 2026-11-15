#include"modeltrace.h"
#include"mylib.h"
#include<string>
#include<cstring>
#include<ctime>
#include<cerrno>
#include<unistd.h>
#include<fcntl.h>
#include<poll.h>
#include<signal.h>
#include<sys/wait.h>
#define MT_MAX_BODY (2*1024*1024)   // 请求体上限
#define MT_MAX_OUT  (8*1024*1024)   // 响应上限
#define MT_TIMEOUT  60              // 单次查询最长秒数，超时杀掉子进程
#define mt_error(a,code,message) http_send((a),Hok Hjson Hc0,"{\"error\":{\"message\":\"" message "\",\"code\":\"" code "\"}}",0)

static int modeltrace_lock;//不允许并发：同一时刻只跑一个 python 子进程

// 脚本路径按可执行文件位置推出：<根>/build/1001 -> <根>/code/modeltrace/modeltrace.py
static int mt_script(char* out,size_t cap){
    char exe[4096];
    ssize_t n=readlink("/proc/self/exe",exe,sizeof(exe)-1);
    if(n<=0)return 0;
    exe[n]=0;
    for(int i=0;i<2;i++){
        char* p=strrchr(exe,'/');
        if(!p)return 0;
        *p=0;
    }
    return snprintf(out,cap,"%s/code/modeltrace/modeltrace.py",exe)<(int)cap;
}
// 把 in 写给子进程的 stdin，读回 stdout。成功返回 1
static int mt_run(const char* in,size_t n,std::string& out){
    char script[4200];
    if(!mt_script(script,sizeof(script)))return 0;
    int pi[2],po[2];
    if(pipe2(pi,O_CLOEXEC))return 0;
    if(pipe2(po,O_CLOEXEC)){close(pi[0]);close(pi[1]);return 0;}
    long maxfd=sysconf(_SC_OPEN_MAX);
    if(maxfd<0||maxfd>65536)maxfd=65536;
    pid_t pid=fork();
    if(pid<0){close(pi[0]);close(pi[1]);close(po[0]);close(po[1]);return 0;}
    if(pid==0){
        dup2(pi[0],0);
        dup2(po[1],1);
        for(int fd=3;fd<maxfd;fd++)close(fd);//不把数据库、监听端口等句柄带给子进程
        execlp("python3","python3",script,"api",(char*)0);
        _exit(127);
    }
    close(pi[0]);close(po[1]);
    fcntl(pi[1],F_SETFL,O_NONBLOCK);
    fcntl(po[0],F_SETFL,O_NONBLOCK);
    time_t end=time(0)+MT_TIMEOUT;
    size_t w=0;
    int wfd=pi[1],ok=1;
    char buf[65536];
    if(n==0){close(wfd);wfd=-1;}
    while(1){
        struct pollfd fds[2]={{po[0],POLLIN,0},{wfd,POLLOUT,0}};
        int left=(int)(end-time(0));
        if(left<=0){ok=0;break;}
        int r=poll(fds,wfd<0?1:2,left*1000);
        if(r<0){if(errno==EINTR)continue;ok=0;break;}
        if(r==0)continue;
        if(wfd>=0&&fds[1].revents){
            ssize_t m=write(wfd,in+w,n-w);
            if(m>0)w+=m;
            if(w==n||(m<0&&errno!=EAGAIN&&errno!=EINTR)){close(wfd);wfd=-1;}
        }
        if(fds[0].revents){
            ssize_t m=read(po[0],buf,sizeof(buf));
            if(m>0){
                out.append(buf,m);
                if(out.size()>MT_MAX_OUT){ok=0;break;}
            }
            else if(m==0||(errno!=EAGAIN&&errno!=EINTR))break;
        }
    }
    if(wfd>=0)close(wfd);
    close(po[0]);
    if(!ok)kill(pid,SIGKILL);
    int st=0;
    while(waitpid(pid,&st,0)<0&&errno==EINTR);
    return ok&&WIFEXITED(st)&&WEXITSTATUS(st)==0&&!out.empty();
}
void modeltrace(http_para* a){
    if(a->m<=0)return mt_error(a,"bad_request","empty request body");
    if(a->m>MT_MAX_BODY)return mt_error(a,"bad_request","request body too large");
    if(TRY(&modeltrace_lock))return mt_error(a,"busy","another query is running, please retry later");
    std::string out;
    int ok=mt_run(a->get+a->n,a->m,out);
    UNLOCK(modeltrace_lock);
    if(!ok){
        LOG("modeltrace failed, output %zu bytes",out.size());
        return mt_error(a,"internal","modeltrace backend failed");
    }
    http_send(a,Hok Hjson Hc0,out.c_str(),out.size());
}
