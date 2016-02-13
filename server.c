#include <sys/socket.h>
#include <netinet/in.h> //for sockaddr_in struct
#include <stdio.h> //for print
#include <stdlib.h> // for exit
#include <string.h> // for memset

#define PORT 5555

int main(int argc, char *argv[]) {
    int sockfd;
    struct sockaddr_in serv_addr, client_addr;
    socklen_t addrlen = sizeof(client_addr);
    char buf[1024];
    
    if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        perror("Cannot create socket");
        exit(1);
    }
    
    memset((char*)&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK); // Listen on our local address 127.0.0.1
    serv_addr.sin_port = htons(PORT);

    // Bind server to this socket
    if (bind(sockfd, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0) {
        perror("Bind failed");
        exit(1);
    }
    
    // Receive messages (along with source location, saved in client_addr)
    while (1) {
        int recvlen = recvfrom(sockfd, buf, 1024, 0, (struct sockaddr*)&client_addr, &addrlen);
        if (recvlen > 0) {
            buf[recvlen] = 0;
            printf("Received message: %s\n", buf);
        }
    }
}
