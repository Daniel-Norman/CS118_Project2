#include <sys/socket.h>
#include <netinet/in.h> //for sockaddr_in struct
#include <stdio.h> //for print
#include <stdlib.h> // for exit
#include <string.h> // for memset

#define PORT 5555
#define DATA_HEADER_SIZE 6
#define DATA_SIZE 512
#define PACKET_SIZE DATA_HEADER_SIZE+DATA_SIZE
#define NUM_SLOTS 30
#define WINDOW_SIZE 5
#define FREE_BUFFER_SEQNUM -1

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
    
    // TODO get this from program argument
    memcpy(buf, "server.c", 9);
    
    // Send messages (give destination on each call, because UDP just sends, doesn't do a TWH to secure a connection)
    if (sendto(sockfd, buf, strlen(buf), 0, (struct sockaddr*)&serv_addr, addrlen) < 0) {
        perror("Sendto failed");
        exit(1);
    } else printf("Sendto succeeded\n");
    
    
    // The current in-order packet's seqnum we expect to receive
    int rcvbase = 0;
    
    // Buffers for storing out-of-order packets, along with an array
    //  to keep track of which seqnum each buffer corresponds to
    int seqnums_for_ooo_buffers[WINDOW_SIZE];
    memset(seqnums_for_ooo_buffers, FREE_BUFFER_SEQNUM, WINDOW_SIZE * sizeof(int));
    char ooo_buffers[WINDOW_SIZE][DATA_SIZE];
    
    // Variables to hold info for the current receieved packet
    char data[DATA_SIZE];
    int seqnum, size, last;
    
    // TODO use 'last' variable to break out of this when we've written last packet to file
    while (1) {
        int recvlen = recvfrom(sockfd, buf, 1024, 0, (struct sockaddr*)&serv_addr, &addrlen);
        if (recvlen > 0) {
            // Parse the received packet into it header variables and data
            read_packet(buf, data, &seqnum, &size, &last);
            
            // Put the in-order data into the file, and then check buffers to see if next
            //  in-order data has already been receieved
            if (seqnum == rcvbase) {
                // TODO Write to file (printing right now)
                data[DATA_SIZE] = 0;
                printf("%s", data);
                
                // Check data in the out-of-order buffers, writing to file as necessary
                int found_next = 0;
                do {
                    // Increase rcvbase to move to accepting the next seqnum
                    rcvbase = (rcvbase + 1) % NUM_SLOTS;

                    // Loop through our buffers' seqnums, seeing if one matches the next
                    //  seqnum that we expect
                    int i;
                    for (i = 0; i < WINDOW_SIZE; ++i) {
                        if (seqnums_for_ooo_buffers[i] == rcvbase) {
                            // TODO Write buffer i to file (printing right now)
                            ooo_buffers[i][DATA_SIZE] = 0;
                            printf("%s", ooo_buffers[i]);
                            
                            // Save the fact that we used this buffer, by setting it's seqnum to free
                            //  (thus enabling a future packet to use it)
                            seqnums_for_ooo_buffers[i] = FREE_BUFFER_SEQNUM;
                            found_next = 1;
                            break;
                        }
                    }
                } while (found_next);
            }
            
            // Put the out-of-order data in a buffer
            if (seqnum != rcvbase) {
                int already_in_buffer = 0;
                int free_buffer_index = -1;
                
                int i;
                for (i = 0; i < WINDOW_SIZE; ++i) {
                    // If this packet is already in the buffers, don't do anything
                    if (seqnums_for_ooo_buffers[i] == seqnum) {
                        already_in_buffer = 1;
                        break;
                    }
                    
                    // If we find a free buffer slot, use it
                    if (seqnums_for_ooo_buffers[i] == FREE_BUFFER_SEQNUM) {
                        free_buffer_index = i;
                    }
                }
                
                // Only save to buffer if not already in a buffer, and we have a free slot
                // If no free slot, this packet is lost (which may happen if server resends
                //  too many packets) but that is OK
                if (!already_in_buffer && free_buffer_index != -1) {
                    seqnums_for_ooo_buffers[free_buffer_index] = seqnum;
                    memcpy(ooo_buffers[free_buffer_index], data, DATA_SIZE);
                }
            }
        }
    }
}
