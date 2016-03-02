#include <sys/socket.h>
#include <netinet/in.h> //for sockaddr_in struct
#include <stdio.h> //for print
#include <stdlib.h> // for exit
#include <string.h> // for memset

#define DATA_HEADER_SIZE 6
#define DATA_SIZE 512
#define PACKET_SIZE (DATA_HEADER_SIZE+DATA_SIZE)
#define ACK_SIZE 2
#define NUM_SLOTS 30
#define SEQNUM_UNUSED -1

int create_ack(char* buf, int seqnum) {
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

void error(char* err) {
    perror(err);
    exit(1);
}

int write_to_file(FILE* file, char* data, int size) {
    return fwrite(data, 1, size, file);
}

int close_file(FILE* file) {
    if (fclose(file) == 0) {
        printf("File successfully transferred!\n");
        exit(0);
    }
    else {
        error("Error closing file");
    }
}


int main(int argc, char *argv[]) {
    if (argc < 6) {
        error("Error: not all fields provided. Sender hostname, Sender port number, Filename, Probability Loss, Probability Corruption");
    }
    
    char* sender_hostname = argv[1];
    int sender_port = atoi(argv[2]);
    char *filename = argv[3];
    int loss_rate = atoi(argv[4]);
    int corruption_rate = atoi(argv[5]);
    
    int sockfd;
    int i;
    int recvlen;
    struct sockaddr_in serv_addr;
    socklen_t addrlen = sizeof(serv_addr);
    char buf[PACKET_SIZE];
    
    if ((sockfd = socket(AF_INET, SOCK_DGRAM | SOCK_NONBLOCK, 0)) < 0) {
        error("Cannot create socket");
    }
    
    memset((char*)&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    inet_aton(sender_hostname, &serv_addr.sin_addr.s_addr);
    serv_addr.sin_port = htons(sender_port);
    
    printf("Filename: %s\n", filename);
    
    // Send message (give destination on each call, because UDP just sends, doesn't do a TWH to secure a connection)
    // Send filename
    if (sendto(sockfd, filename, strlen(filename), 0, (struct sockaddr*)&serv_addr, addrlen) < 0) {
        error("Sendto failed");
    }
    
    int window_size = 0;
    while (window_size == 0) {
        recvlen = recvfrom(sockfd, buf, 10, 0, (struct sockaddr*)&serv_addr, &addrlen);
        if (recvlen > 0) {
            window_size = atoi(buf);
        }
    }

    // Open (and create) the file we will be writing into
    // TODO use the actual name of the file we're requesting
    FILE* fp = fopen("TRANSFERRED_FILE", "wb");
    
    // The current in-order packet's seqnum we expect to receive
    int rcvbase = 0;
    
    // The seqnum for the last packet of the file.
    // This gets set when client receives a packet with last=true in the header
    int last_seqnum = SEQNUM_UNUSED;
    
    // Buffers for storing out-of-order packets, along with an array
    //  to keep track of which seqnum each buffer corresponds to
    int* seqnums_for_ooo_buffers = (int*) malloc(sizeof(int) * window_size);
    if (seqnums_for_ooo_buffers == 0) error("Memory alloc failed");
    for (i = 0; i < window_size; ++i) seqnums_for_ooo_buffers[i] = SEQNUM_UNUSED;
    char** ooo_buffers = (char**) malloc(sizeof(char*) * window_size);
    if (ooo_buffers == 0) error("Memory alloc failed");
    for (i = 0; i < window_size; ++i) {
        ooo_buffers[i] = (char*) malloc(sizeof(char) * DATA_SIZE);
        if (ooo_buffers[i] == 0) error("Memory alloc failed");
    }
    
    // Variables to store info for the current receieved packet
    char data[DATA_SIZE];
    int seqnum, size, last;
    
    
    int rand_loss, rand_corruption;
    srand(time(NULL) + rand()); //Seeded differently from server.c
    
    
    // Continually receieve packets.
    // When we realize we've written the last packet, the program exits
    while (1) {
        int recvlen = recvfrom(sockfd, buf, PACKET_SIZE, 0, (struct sockaddr*)&serv_addr, &addrlen);
        if (recvlen > 0) {
            rand_loss = rand() % 100;
            rand_corruption = rand() % 100;
            
            if (rand_loss < loss_rate) {
                printf("Packet is lost.\n");
            }
            else if (rand_corruption < corruption_rate) {
                printf("Packet is corrupted. Not sending ACK so server resends this packet.\n");
            }
            else {
                // Parse the received packet into it header variables and data
                read_packet(buf, data, &seqnum, &size, &last);
                
                // Create and send an ACK back to the server
                char ack[ACK_SIZE];
                create_ack(ack, seqnum);
                if (sendto(sockfd, ack, ACK_SIZE, 0, (struct sockaddr*)&serv_addr, addrlen) < 0) {
                    error("Sendto failed");
                }
                else {
                    printf("Sent ACK: %c%c\n", ack[0], ack[1]);
                }
                
                // If we're told this is the last packet, set last_seqnum accordingly
                if (last) {
                    last_seqnum = seqnum;
                }
                
                // Put the in-order data into the file, and then check buffers to see if next
                //  in-order data has already been receieved
                if (seqnum == rcvbase) {
                    // Write to file
                    if (write_to_file(fp, data, size) != size) error("Error writing to file");
                    
                    // If this is the last packet, we're done! Close the file
                    if (rcvbase == last_seqnum) {
                        close_file(fp);
                    }
                    
                    // Check data in the out-of-order buffers, writing to file as necessary
                    int found_next;
                    do {
                        found_next = 0;
                        
                        // Increase rcvbase to move to accepting the next seqnum
                        rcvbase = (rcvbase + 1) % NUM_SLOTS;
                        
                        // Loop through our buffers' seqnums, seeing if one matches the next
                        //  seqnum that we expect
                        for (i = 0; i < window_size; ++i) {
                            if (seqnums_for_ooo_buffers[i] == rcvbase) {
                                // Write buffer i to file
                                if (write_to_file(fp, ooo_buffers[i], DATA_SIZE) != DATA_SIZE) error("Error writing to file");
                                
                                // If we just wrote the last packet, we're done! Close the file
                                if (rcvbase == last_seqnum) {
                                    close_file(fp);
                                }
                                
                                // Save the fact that we used this buffer, by setting it's seqnum to free
                                //  (thus enabling a future packet to use it)
                                seqnums_for_ooo_buffers[i] = SEQNUM_UNUSED;
                                found_next = 1;
                                break;
                            }
                        }
                    } while (found_next);
                }
                
                // If we've received a packet from the future, put the out-of-order data in a buffer
                else if ((seqnum > rcvbase && (seqnum - rcvbase) < window_size) || (seqnum < rcvbase && (seqnum + NUM_SLOTS - rcvbase) < window_size)) {
                    int already_in_buffer = 0;
                    int free_buffer_index = -1;
                    
                    for (i = 0; i < window_size; ++i) {
                        // If this packet is already in the buffers, don't do anything
                        if (seqnums_for_ooo_buffers[i] == seqnum) {
                            already_in_buffer = 1;
                            break;
                        }
                        
                        // If we find a free buffer slot, use it
                        if (seqnums_for_ooo_buffers[i] == SEQNUM_UNUSED) {
                            free_buffer_index = i;
                        }
                    }
                    
                    // Only save to buffer if not already in a buffer and if we have a free slot.
                    // If no free slot, this packet is lost (which may happen if server resends
                    //  too many packets) but that is OK
                    if (!already_in_buffer && free_buffer_index != -1) {
                        seqnums_for_ooo_buffers[free_buffer_index] = seqnum;
                        memcpy(ooo_buffers[free_buffer_index], data, size);
                    }
                }
            }
        }
    }
}
