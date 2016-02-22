#include <sys/socket.h>
#include <netinet/in.h> //for sockaddr_in struct
#include <stdio.h> //for print
#include <stdlib.h> // for exit
#include <string.h> // for memset
#include <sys/stat.h>

#define PORT 5555
#define DATA_HEADER_SIZE 6
#define DATA_SIZE 512
#define PACKET_SIZE DATA_HEADER_SIZE+DATA_SIZE
#define NUM_SLOTS 30

int create_data_header(char *buf, int seqnum, int size, int last) {
    char header[DATA_HEADER_SIZE];
    sprintf(header, "%02d%03d%d", seqnum, size, last);
    memcpy(buf, header, DATA_HEADER_SIZE);
}

int read_ack_header(char *buf, int* seqnum) {
    *seqnum = atoi(buf);
    return 1;
}

int create_packet(char *buffer, int seqnum, FILE* file, int file_pos) {
    int last = 0;
    int size = 0;
    size = read_file(buffer + DATA_HEADER_SIZE, file, file_pos, &last);
    create_data_header (buffer, seqnum, size, last);
    return 1;
}

int read_file(char* data, FILE* file, int start_pos, int* last) {
    int read = 0;
    fseek(file, sizeof(char) * start_pos, SEEK_SET);
    if ((read = fread(data, 1, DATA_SIZE, file)) == 0) error("Error reading file");
    *last = feof(file);
    return read;
}

int convert_seqnum_to_file_pos(int seqnum, int base, int wraparound_count) {
    int pos = wraparound_count * NUM_SLOTS * DATA_SIZE;
    
    // Handle when index wrapped around to 0 but base has not yet
    if (seqnum < base) {
        seqnum += NUM_SLOTS;
    }
    
    pos += seqnum * DATA_SIZE;
    
    return pos;
}

void update_ack(int* acks, int index, int value) {
    if (value) {
        *acks |= 1 << index;
    }
    else {
        *acks &= ~(1 << index);
    }
}

int check_ack(int acks, int index) {
    return (acks >> index) & 1;
}


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
        sprintf(buf, "File size: %u\n", file_size);

        int total_unique_acks = 0;

        int send_base = 0;
        
        int window_size = 5; //TODO: To be passed in
        
        int wraparound_count = 0; // Increment whenever send_base gets wrapped back to 0
        
        int timers[NUM_SLOTS] = {(int)time(NULL)};

        
        int time_out;
        time_out = 5; //TODO: to be passed in - will need to change if TA wants smaller time interval
        
        // Integer flag of bits of ACKs for the NUM_SPOTS spots
        int acks = 0;

        /*
         
         This is unnecessary, as it is handled in the first iteration of the following while loop

         This treats the first sends as being "resends due to timeout", but it works and removes
         need for duplicate code / special handling of first 5 packets
         
         
        // Send the first window_size-# of packets
        int i;
     	for (i = send_base; i < send_base + window_size; i++) {
            char buffer[DATA_SIZE + DATA_HEADER_SIZE];
            int file_pos = convert_seqnum_to_file_pos(i, send_base, wraparound_count);
            //printf("A\n");
            create_packet(buffer, i, fp, file_pos);
            //printf("B\n");

            sendto(sockfd, buffer, strlen(buffer), 0, (struct sockaddr*)&client_addr, addrlen);
            //printf("C\n");
            timers[i] = (int)time(NULL) + time_out;
            //printf("D\n");
        }*/

        char packet_buffer[PACKET_SIZE];
        
        while (total_unique_acks < (file_size / DATA_SIZE)) {
            //TODO handle receive ACKs logic
            //	    int recvlen = recvfrom(sockfd, buf, 512, 0, (struct sockaddr*)&client_addr, &addrlen);
            // use update_ack() to set acks
            // make sure to reset ack to 0 when moving send_base (if send base goes 4 to 5, set ack[4] to 0)

            int i;
            for (i = send_base; i < send_base + window_size; i++) {
                // Keep index in range 0-NUM_SLOTS
                int j = i % NUM_SLOTS;
                
                if (timers[j] < (int)time(NULL) && !check_ack(acks, j)) {
                    int file_pos = convert_seqnum_to_file_pos(j, send_base, wraparound_count);
                    
                    // Only send if this seqnum's fileposition doesn't correspond to outside our file
                    if (file_pos <= file_size) {
                        create_packet(packet_buffer, j, fp, file_pos);
                        sendto(sockfd, packet_buffer, PACKET_SIZE, 0, (struct sockaddr*)&client_addr, addrlen);
                        timers[j] = (int)time(NULL) + time_out;
                        printf("reset timer %d\n", j);
                    }
                }
            }
        }
    }
    else {
        memcpy(buf, "File doesn't exist.", 20);
        sendto(sockfd, buf, strlen(buf), 0, (struct sockaddr*)&client_addr, addrlen);
    }
}


