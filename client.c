#include <sys/socket.h>
#include <netinet/in.h> //for sockaddr_in struct
#include <stdio.h> //for print
#include <stdlib.h> // for exit
#include <string.h> // for memset

#define PORT 5555

int main(int argc, char *argv[]) {
    int sockfd;
    struct sockaddr_in serv_addr;
    socklen_t addrlen = sizeof(serv_addr);
    char buf[1024];
    
    if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        perror("Cannot create socket");
        exit(1);
    }
    
    memset((char*)&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK); // Server is at local address 127.0.0.1
    serv_addr.sin_port = htons(PORT);
    
    memcpy(buf, "Hello, World!", 14);
    
    // Send messages (give destination on each call, because UDP just sends, doesn't do a TWH to secure a connection)
    if (sendto(sockfd, buf, strlen(buf), 0, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0) {
        perror("Sendto failed");
        exit(1);
    } else printf("Sendto succeeded\n");
}
