#ifndef __myio
#define __myio
#define Hok   "HTTP/1.1 200 OK\r\n"
#define H404  "HTTP/1.1 404 Not Found\r\n"

#define Hc0   "Cache-Control: no-cache\r\n"
#define Hc1h  "Cache-Control: max-age=3600, public\r\n"
#define Hc7d  "Cache-Control: max-age=604800, public\r\n"

#define Hhtml "Content-Type: text/html\r\n"
#define Hjson "Content-Type: application/json\r\n"
#define Hjs   "Content-Type: application/javascript\r\n"
#define Hcss  "Content-Type: text/css\r\n"
#define Hico  "Content-Type: image/x-icon\r\n"
#define Hwebp "Content-Type: image/webp\r\n"
#define Htxt  "Content-Type: text/plain\r\n"
#define Hmp4  "Content-Type: video/mp4\r\n"

#define Hgzip "Content-Encoding: gzip\r\n"
#define Hbr   "Content-Encoding: br\r\n"
#define Hzstd "Content-Encoding: zstd\r\n"

#define Hdown "Content-Disposition: form-data\r\n"

#define IO_EXIT 1
#define IO_RESTART 2

#ifdef __cplusplus
extern "C"{
#endif

void myJSON(const char* p,char* a);
void mystart(int port,void (*work)(void* ssl,char* get,int n,int m,int ip));
void mysend(void* ssl,const char* head,const char* a,int n);

#ifdef __cplusplus
}
#endif

int ncmp(const char* a,const char* b,int n){
    for(int i=0;i<n;i++)if(a[i]!=b[i])return a[i]-b[i];
    return 0;
}
int bncmp(const char* a,const char* b){
    for(int i=0;b[i];i++)if(a[i]!=b[i])return a[i]-b[i];
    return 0;
}
#endif