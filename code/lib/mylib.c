#include "mylib.h"

#include <openssl/sha.h>
#include <openssl/rand.h>
#include <stdlib.h>

void mylib_random_string(char* out,size_t n){
    static const char chars[]="ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789";
    unsigned char tmp[256];
    size_t l=0;
    while(l<n) {
        if(RAND_bytes(tmp,sizeof(tmp))!=1){
            while(l<n)out[l++]=chars[rand()%62];
            return;
        }
        for(size_t i=0;i<256&&l<n;i++)if(tmp[i]<248)out[l++]=chars[tmp[i]%62];
    }
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
