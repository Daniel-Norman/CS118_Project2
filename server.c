#include <sys/socket.h>
#include <netinet/in.h> //for sockaddr_in struct
#include <stdio.h> //for print
#include <stdlib.h> // for exit
#include <string.h> // for memset
#include <sys/stat.h>

#define PORT 5555

char* file_data (int start, int size, FILE* file);

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
            
            //printf("Sending reply...\n");
	    //        memcpy(buf, "Hi!", 4);
            //if (sendto(sockfd, buf, strlen(buf), 0, (struct sockaddr*)&client_addr, addrlen) < 0) {
	    // perror("Sendto failed");
	    // exit(1);
            //} else printf("Sendto succeeded\n");
	    break;
        }
    }

    char filename[10];
    memcpy(filename, "server.c", 9); //TODO: Testing
    FILE* fp;
    fp = fopen(filename, "rb");
 
    if (fp != NULL) {
	printf("Opened file: %s\n", filename);
	struct stat st;
	stat(filename, &st);
	unsigned int file_size;
	file_size = st.st_size;
	printf(buf, "File size: %u\n", file_size);
	//sendto(sockfd, buf, strlen(buf), 0, (struct sockaddr*)&client_addr, addrlen);
	int total_acks;
	total_acks = 0;

	int data_size;
	data_size = 512;

	int send_base;
	send_base = 0;

	int window_size;
	window_size = 5; //TODO: To be passed in
	
	int timers[30];
	int i;
	for (i = 0; i < 30; i++) {
	    timers[i] = 0;
	}

	int time_out;
	time_out = 5; //TODO: to be passed in - will need to change if TA wants smaller time interval

	int acks;
	acks = 0;


     	for (i = send_base; i < send_base + window_size; i++) {
	    //TODO
	    //Write header
	    //Write data
	    //memcpy(buf, header+data)
	    //sendto(sockfd, buf, strlen(buf), 0, (struct sockaddr*)&client_addr, addrlen);
	    timers[i] = (int)time(NULL) + time_out;
	}

	while (total_acks < (file_size / 1000)) {
	    //TODO
	    //	    int recvlen = recvfrom(sockfd, buf, 512, 0, (struct sockaddr*)&client_addr, &addrlen);

	    for (i = send_base; i < send_base + window_size; i++) {
		//TODO
		if (timers[i] < (int)time(NULL) & 1) { //!ack(i)
		    //Writer header
		    //write data
		    //memcpy(buf, header+data)
		    //sendto(sockfd, buf, strlen(buf),...
		    timers[i] = (int)time(NULL) + time_out;
		    printf("reset timer %d\n", i);
		}
	    }
	}
    }
    else {
	memcpy(buf, "File doesn't exist.", 20);
	sendto(sockfd, buf, strlen(buf), 0, (struct sockaddr*)&client_addr, addrlen);
    }
    
}

char* file_data (int start, int size, FILE* file) {
    //TODO: Write function
    return "Hello\n";
}
