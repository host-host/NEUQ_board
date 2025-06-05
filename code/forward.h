/**
author NIT
v2025.5.17
*/
#ifndef FORWARD_
#define FORWARD_
#include"https.h"
#include<arpa/inet.h>
#ifdef __cplusplus
extern "C"{
#endif

void forward_to_1598(https_para *ssl) {
    // LOG("1");
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0)return https_send(ssl, H404, "Failed to create socket", 0);
    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(1598);
    server_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
    if (connect(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        close(sockfd);
        return https_send(ssl, H404, "Failed to connect to 1598", 0);
    }
    struct timeval timehttps={60,0};
    setsockopt(sockfd,SOL_SOCKET,SO_RCVTIMEO,(char*)&timehttps,sizeof(struct timeval));
    if (write(sockfd, ssl->get, ssl->n+ssl->m) < 0) {
        close(sockfd);
        return https_send(ssl, H404, "Failed to send request", 0);
    }
    char response[4096];
    while(1){
        int bytes_read = read(sockfd, response, sizeof(response));
        if(bytes_read<=0)break;
        SSL_write(ssl->ssl, response, bytes_read);
    }
    close(sockfd);
}

#ifdef __cplusplus
}
#endif
#endif