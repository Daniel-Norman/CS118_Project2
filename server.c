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
#define ACK_SIZE 2
#define NUM_SLOTS 30
#define SEQNUM_UNUSED -1

int create_data_header(char *buf, int seqnum, int size, int last) {
    char header[DATA_HEADER_SIZE];
    sprintf(header, "%02d%03d%d", seqnum, size, last);
    memcpy(buf, header, DATA_HEADER_SIZE);
}

int read_ack_packet(char *buf, int* seqnum) {
    *seqnum = atoi(buf);
    return 1;
}

int create_packet(char *buffer, int seqnum, FILE* file, int file_pos) {
    int size = 0;
    int last = 0;
    size = read_file(buffer + DATA_HEADER_SIZE, file, file_pos, &last);
    create_data_header (buffer, seqnum, size, last);
    return 1;
}

int read_file(char* data, FILE* file, int start_pos, int* last) {
    int read = 0;
    fseek(file, sizeof(char) * start_pos, SEEK_SET);
    if ((read = fread(data, 1, DATA_SIZE, file)) == 0) error("Error reading file");
    *last = feof(file);
    if (*last) printf("Sending last packet\n");
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
    if (argc < 4) {
	printf("Error: not all fields provided. error rate, corruption, windowsize");
	exit(1);
    }

    int error_rate;
    error_rate = atoi(argv[1]);
    printf("Error rate %d\n", error_rate);

    int corruption;
    corruption = atoi(argv[2]);
    printf("Corruption rate %d\n", corruption);

    int sockfd;
    int i;
    struct sockaddr_in serv_addr, client_addr;
    socklen_t addrlen = sizeof(client_addr);
    char buf[1024];
    
    if ((sockfd = socket(AF_INET, SOCK_DGRAM | SOCK_NONBLOCK, 0)) < 0) {
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
    
    int recvlen;
    // Receive messages (along with source location, saved in client_addr)
    while (1) {
        recvlen = recvfrom(sockfd, buf, 1024, 0, (struct sockaddr*)&client_addr, &addrlen);
        if (recvlen > 0) {
            buf[recvlen] = 0;
            printf("Received message: %s\n", buf);
            break;
        }
    }

    char ws_msg[10];
    memcpy(ws_msg, argv[2], strlen(argv[2]));

    sendto(sockfd, ws_msg, 10, 0, (struct sockaddr*)&client_addr, addrlen);

    char filename[100];
    printf("recvlen %d\n", recvlen);
    memcpy(filename, buf, recvlen);
    filename[recvlen] = 0;
    printf("filename: %s\n", filename);

    FILE* fp;
    fp = fopen(filename, "rb");
 
    if (fp != NULL) {
        printf("Opened file: %s\n", filename);
        struct stat st;
        stat(filename, &st);
        unsigned int file_size;
        file_size = st.st_size;
        printf("File size: %u\n", file_size);

        int total_unique_acks = 0;

        int send_base = 0;
        
        int window_size = atoi(argv[3]); 
        
        int wraparound_count = 0; // Increment whenever send_base gets wrapped back to 0
        
        int timers[NUM_SLOTS] = {0}; // Start at 0 so the first window_size packets are sent immediately

        int time_out = 5;
        
        // Integer flag of bits of ACKs for the NUM_SLOTS spots
        int acks = 0;

        char packet_buffer[PACKET_SIZE];
        
        int unique_acks_required = file_size / DATA_SIZE;
        if (file_size % DATA_SIZE != 0) unique_acks_required++;

	int rand;
	rand = srand(time(NULL));

        while (total_unique_acks != unique_acks_required) {
            char ack[2];

	    int recvlen = recvfrom(sockfd, ack, ACK_SIZE, 0, (struct sockaddr*)&client_addr, &addrlen);

	    if(recvlen > 0) {
		randnum = rand() % 100;
		if (randnum <= error_rate) printf("ACK is lost\n");
		printf("randnum %d\n", randnum);
	    }
            if((recvlen > 0) & (randnum > error_rate)) {
                // Retrieve the ACK's seqnum
                int seqnum;
                read_ack_packet(ack, &seqnum);
                //printf("Got ACK %d\n", seqnum);
                
                // Increase total_unique_acks if this was the first ACK for this slot
                if (!check_ack(acks, seqnum)) {
                    ++total_unique_acks;
                }
                
                // Set ACK as true in the acks bitflags
                update_ack(&acks, seqnum, 1);
                
                // Increment send_base as long as check_ack(acks, send_base) is true
                int limit = send_base + window_size;
                for (i = send_base; i < limit; ++i) {
                    int j = i % NUM_SLOTS;
                    if (check_ack(acks, j)) {
                        update_ack(&acks, j, 0);
                        timers[j] = 0;
                        send_base = (send_base + 1) % NUM_SLOTS;
                        if (send_base == 0) ++wraparound_count;
                    }
                    else break;
                }
            }


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
                        //printf("reset timer %d\n", j);
                    }
                }
            }
        }
    }
    else {
        memcpy(buf, "File doesn't exist.", 20);
        printf("file doesn't exist!!");
        sendto(sockfd, buf, strlen(buf), 0, (struct sockaddr*)&client_addr, addrlen);
    }
}


