#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>

#define bufsiz 512      //num_digit + datalen
#define num_digit 12    //the bits to store packet index in the front of packet
#define datalen 500     //fwrite() length

/*convert 12-bit hamming code into 8-bit raw data*/

void hamming_decode(unsigned char buffer[], unsigned char recv_buffer[]);

/*arguments: <client_IP> <port>*/

int main(int argc, char *argv[]){
    int sockfd, reuse_flag=1, port;             //client sockfd, ,port number
    sockfd = socket(AF_INET, SOCK_DGRAM, 0);    //set up UDP socket
    printf("Opening datagram socket....OK.\n");
    if(sockfd < 0){
        perror("Error opening socket");
        exit(1);
    }
    printf("Setting SO_REUSEADDR...OK.\n");
    if(setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &reuse_flag, sizeof(reuse_flag)) < 0){
        perror("Error setting SO_REUSE");
        exit(1);
    }

    struct sockaddr_in local;   //client address
    port = 8888;       //set up port, group, local interface...
    bzero((char*)&local, sizeof(local));
    local.sin_family = AF_INET;
    local.sin_port = htons(port);
    local.sin_addr.s_addr = INADDR_ANY;
    printf("Binding datagram socket...OK.\n");
    if(bind(sockfd, (struct sockaddr*)&local, sizeof(local))<0){
        perror("Error binding");
        exit(1);
    }


    struct ip_mreq group;       //multicast group structure
    group.imr_multiaddr.s_addr = inet_addr("226.1.1.1");
    group.imr_interface.s_addr = inet_addr("10.0.2.15");
    printf("Adding multicast group...OK.\n");
    if(setsockopt(sockfd, IPPROTO_IP, IP_ADD_MEMBERSHIP, (char*)&group, sizeof(group)) < 0){
        perror("Error adding multicast group");
        exit(1);
    }
    printf("Add into multicast group successfully.\n");
    printf("Server ip: %s\n","10.0.2.15");
    printf("Group address: 226.0.0.1	port: %s\n","8888");

    char fname[bufsiz], fsiz_str[bufsiz];   //buffers of file name and file size
    int r,fsiz;                             //return value of read(), file size integer

    //receive file name from group
    r = read(sockfd, fname, bufsiz);
    if(r < 0){
        perror("Error reading file name");
        exit(1);
    }

    //receive file size string from group
    r = read(sockfd, fsiz_str, bufsiz);
    if(r < 0){
        perror("Error reading file size");
        exit(1);
    }

    fsiz = atoi(fsiz_str);                  //convert file size string into integer
    double max_pkt = (double)fsiz/datalen;  //calculate the amount of packet to send
    if(fsiz%datalen) max_pkt+=1;            //round up
    int max_packet =  (int) max_pkt;        //convert amount of packet to int
    printf("Ready to download \'%s\' [%.2lfKB]...\n", fname, (double)fsiz/1024);
    printf("Total packets: %d\n",max_packet);

    FILE *fp = fopen(fname, "ab");          //open file as an append binary file
    if(!fp) {
        perror("Error opening file");
        exit(1);
    }

    int log = 5, packet_ind = 0, lost_packet = 0;
    //transfer percentage, packet index, amount of dropped packets
    time_t timep;   //use this to get system time
    unsigned char buffer[bufsiz], recv_buffer[bufsiz*2];    //fwrite buffer, receive buffer
    bzero(recv_buffer,bufsiz*2);
    printf("Reading datagram message...OK.\n");
    while(r = read(sockfd,recv_buffer,bufsiz*2)){
        if(r<0){
            perror("Error reading packet");
            exit(1);
        }

        //decode hamming code
        bzero(buffer,bufsiz);
        hamming_decode(buffer,recv_buffer);
        bzero(recv_buffer,bufsiz*2);

        //decode packet index in the front of packet
        int now = 0, ind = 0;
        while(buffer[now] == 0 && now < num_digit){
            now++;
        }
        while(now < num_digit){
            ind = ind*10 + buffer[now];
            now++;
        }

        fwrite(buffer + num_digit, 1, r/2 - num_digit, fp);     //write data into binary file
        time(&timep);                                           //get system time
        for(;(double)ind/max_packet >= (double)log/100; log+=5){//count percentage
            printf("%3d%% %s",log,asctime(localtime(&timep)));  //print percentage and time
        }
        packet_ind++;               //update packet index
        while(packet_ind < ind){    //count dropped packet depending on index continuity
            lost_packet++;
            packet_ind++;
        }
        if(ind == max_packet){      //if this is the last packet, stop receiving
            break;
        }
    }
    printf("Compeleted.\n");
    printf("Packet drop rate: %d/%d (%d%%)\n",lost_packet,max_packet,100*lost_packet/max_packet);
    close(sockfd);
    fclose(fp);
    return 0;
}

void hamming_decode(unsigned char buffer[], unsigned char recv_buffer[]){
    unsigned int check_mask[8]={0,7,6,5,4,3,2,1}; //index used to do modify
    unsigned int checkbit;
    int i,j,k;
    for(i=0; i<bufsiz; ++i){
        int ind = i*2;
        checkbit = 0;
        for(j=7,k=1; j>=0; --j,++k){
            if((recv_buffer[ind]>>j) & 1){  //if this bit stores 1
                checkbit ^= k;              //xor the index of this bit
            }
        }
        for(j=7; j>3; --j,++k){
            if((recv_buffer[ind+1]>>j) & 1){//if this bit stores 1
                checkbit ^= k;              //xor the index of this bit
            }
        }

        //if check bit is not zero, modify a specific bit
        if(checkbit){
            recv_buffer[ind+checkbit/9] ^= (1<<check_mask[checkbit%8]);
        }

        /*convert 12-bit hamming code into 8-bit char and store them in buffer*/
        buffer[i] |= ((recv_buffer[ind]>>5)&1);
        buffer[i] <<=1;
        buffer[i] |= ((recv_buffer[ind]>>3)&1);
        buffer[i] <<=1;
        buffer[i] |= ((recv_buffer[ind]>>2)&1);
        buffer[i] <<=1;
        buffer[i] |= ((recv_buffer[ind]>>1)&1);
        buffer[i] <<=4;
        ind++;
        buffer[i] |= (recv_buffer[ind]>>4);
    }
}

