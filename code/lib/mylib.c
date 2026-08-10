#include "mylib.h"

#include <openssl/sha.h>
#include <openssl/rand.h>
#include <string.h>

size_t mylib_random_string(char* out,size_t n){
    static const char chars[]="ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789";
    unsigned char tmp[256];
    for(size_t l=0;l<n;) {
        if(RAND_bytes(tmp,sizeof(tmp))!=1)return l;
        for(size_t i=0;i<256&&l<n;i++)if(tmp[i]<248)out[l++]=chars[tmp[i]%62];
    }
    return n;
}
void mylib_sha256(const void* data,size_t n,char out[44]){
    static const char chars[]="ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";
    unsigned char c[SHA256_DIGEST_LENGTH];
    SHA256(data,n,c);
    size_t s=0,bits=0,l=0;
    for(size_t i=0;i<SHA256_DIGEST_LENGTH;i++){
        s=s<<8|c[i];
        bits+=8;
        while(bits>=6)out[l++]=chars[s>>(bits-=6)&63];
    }
    out[l++]=chars[(s<<(6-bits))&63];
    out[l]=0;
}
size_t utf8_substr(const char *c,size_t n) {
    if(strlen(c)<=n)return strlen(c);
    while(n&&(c[n]&0xC0)==0x80)n--;
    return n;
}
cJSON* file2j(const char*file){
    if(!file)return 0;
    FILE* f=fopen(file,"rb");
    if(!f)return 0;
    if(fseek(f,0,SEEK_END)!=0) {
        fclose(f);
        return 0;
    }
    long size=ftell(f);
    if(size<0||fseek(f,0,SEEK_SET)!=0) {
        fclose(f);
        return 0;
    }
    char* buffer=(char*)malloc((size_t)size+1);
    if(!buffer) {
        fclose(f);
        return 0;
    }
    size_t read_size=fread(buffer,1,(size_t)size,f);
    fclose(f);
    if (read_size != (size_t)size) {
        free(buffer);
        return 0;
    }
    buffer[size] = '\0';
    cJSON_Minify(buffer);
    cJSON* a=cJSON_Parse(buffer);
    free(buffer);
    return a;
}
