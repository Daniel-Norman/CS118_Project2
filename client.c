#include <sys/socket.h>
#include <netinet/in.h> //for sockaddr_in struct
#include <stdio.h> //for print
#include <stdlib.h> // for exit
#include <string.h> // for memset

#define PORT 5555
#define DATA_HEADER_SIZE 6

int create_ack_header(char* buf, int seqnum) {
    return sprintf(buf, "%02d", seqnum);
}

int read_data_header(char* buf, int* seqnum, int* size, int* last) {
    char subbuf[4];
    int i = 0;
    
    memcpy(subbuf, buf + i, 2);
    subbuf[2] = 0;
    *seqnum = atoi(subbuf);
    i += 2;
    
    memcpy(subbuf, buf + i, 3);
    subbuf[3] = 0;
    *size = atoi(subbuf);
    i += 3;
    
    memcpy(subbuf, buf + i, 1);
    subbuf[1] = 0;
    *last = atoi(subbuf);
    i += 1;
    
    return 1;
}

int read_packet(char* input, char* data, int* seqnum, int* size, int* last) {
    read_data_header(input, seqnum, size, last);
    memcpy(data, input + DATA_HEADER_SIZE, *size);
    return 1;
}

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
    if (sendto(sockfd, buf, strlen(buf), 0, (struct sockaddr*)&serv_addr, addrlen) < 0) {
        perror("Sendto failed");
        exit(1);
    } else printf("Sendto succeeded\n");
    
    while (1) {
        int recvlen = recvfrom(sockfd, buf, 1024, 0, (struct sockaddr*)&serv_addr, &addrlen);
        if (recvlen > 0) {
            buf[recvlen] = 0;
            printf("Received message: %s\n", buf);
        }
    }
}
