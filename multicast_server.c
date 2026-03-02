#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <time.h>

#define bufsiz 512      //num_digit + datalen
#define num_digit 12    //the bits to store packet index in the front of packet
#define datalen 500     //fread() length

/*convert 8-bit char into 12-bit hamming code*/

void hamming_encode(unsigned char buffer[], unsigned char send_buffer[]);

/*convert a long integer to string*/

char *to_string(long int a) {
    static char tmp[bufsiz]="",str[bufsiz]="";
    int i=0,j;
    while(a!=0) {
        tmp[i]=a%10+'0';
        a/=10;
        i++;
    }
    for(i-=1,j=0; i>-1; --i,++j) {
        str[j]=tmp[i];
    }
    return str;
}

/*argv: <file_name>*/

int main(int argc, char *argv[]) {
    int sockfd, port;                           //server sockfd, port number
    sockfd = socket(AF_INET, SOCK_DGRAM, 0);    //set up UDP socket
    if(sockfd < 0) {
        perror("Error opening socket");
        exit(1);
    }
    printf("Opening datagram socket....OK.\n");
    printf("Setting the local interface...OK.\n");
    struct sockaddr_in group;                   //group address
    struct in_addr local_interface;             //server address
    port = 8888; //atoi(argv[2])                      //set up port, group, local interface...
    bzero((char*)&group, sizeof(group));
    group.sin_family = AF_INET;
    group.sin_addr.s_addr = inet_addr("226.1.1.1");
    group.sin_port = htons(port);
    local_interface.s_addr =inet_addr("10.0.2.15"); //inet_addr(argv[1])
    if(setsockopt(sockfd, IPPROTO_IP, IP_MULTICAST_IF, (char*)&local_interface, sizeof(local_interface)) < 0) {
        perror("Error setting local interface");
        exit(1);
    }
    printf("local ip: %s\n","10.0.2.15");
    printf("Group address: 226.0.0.1\tport: %d\n",port);

    FILE *fp;                           //file to send
    struct stat st;                     //file status, it will be used to get file size later
    fp = fopen(argv[1],"rb");           //open file as a binary file
    stat(argv[1],&st);                  //get file status
    if(!fp) {
        perror("Error opening file");
        exit(1);
    }
    double max_pkt = (double) st.st_size/datalen;   //calculate the amount of packet to send
    if(st.st_size % datalen) max_pkt += 1;          //round up
    int max_packet = (int) max_pkt;                 //convert amount of packet to int

    printf("Ready to transfer '%s' [%.2lf KB]...\n",argv[1],(double)st.st_size/1024);
    printf("Total packets: %d\n",max_packet);
    printf("Input \'y\' to continue \n");
    char input = ' ';
    while(input != 'y'){        //wait user to press 'y' so as to start transfer
        scanf("%c",&input);
    }
    printf("Sending datagram message...OK.\n");
    int sd; //return value of send();

    //send file name to group
    sd = sendto(sockfd, argv[1], bufsiz, 0, (struct sockaddr*)&group, sizeof(group));
    if(sd < 0) {
        perror("Error sending file name to clients");
        exit(1);
    }
    usleep(1);

    //send file size to group
    sd = sendto(sockfd, to_string(st.st_size), bufsiz, 0, (struct sockaddr*)&group, sizeof(group));
    if(sd < 0) {
        perror("Error sending file size to clients");
        exit(1);
    }

    int log=5, packet_ind = 1;  //transfer percentage, packet index
    time_t timep;               //use this to get system time
    int len;                    //return value of fread()
    unsigned char buffer[bufsiz], send_buffer[bufsiz*2];    //fread buffer, send buffer
    bzero(buffer, bufsiz);
    while(len = fread(buffer + num_digit, 1, datalen, fp)) {
        if(len < 0){
            perror("Error reading file");
            exit(1);
        }

        //encode packet index in the front of packet
        int now = num_digit-1, tmp = packet_ind;
        while(tmp){
            buffer[now] = tmp % 10;
            now--;
            tmp/=10;
        }

        //encode hamming code
        bzero(send_buffer,bufsiz*2);
        hamming_encode(buffer,send_buffer);

        //send encoded data to group
        sd = sendto(sockfd, send_buffer, bufsiz*2, 0, (struct sockaddr*)&group, sizeof(group));	//send data to client
        if(sd<0) {
            perror("Error sending packet");
            exit(1);
        }

        time(&timep);   //get system time
        for(; (double)packet_ind/max_packet >= (double)log/100; log+=5) {   //count percentage
            printf("%3d%% %s",log,asctime(localtime(&timep)));              //print percentage and time
        }
        bzero(buffer,bufsiz);
        packet_ind++;   //update packet index
        usleep(1);     //slow down packet sending rate
    }

    close(sockfd);
    fclose(fp);
    return 0;
}

void hamming_encode(unsigned char buffer[], unsigned char send_buffer[]){
    int i,j;
    unsigned int encode_mask[8]={12,11,10,9,7,6,5,3};   //bit index to xor hamming redundancy bit
    unsigned int checkbit; //redundancy bit
    for(i=0; i<bufsiz; ++i){
        checkbit=0;
        for(j=7;j>=0;--j){
            if((buffer[i]>>j) & 1)          //if this bit stores 1
                checkbit ^= encode_mask[j]; //xor the index of this bit
        }

        /*convert 8-bit char into 12-bit hamming code*/
        /*use two char space (16-bit) to store hamming code*/
        int ind=i*2;
        send_buffer[ind] |= (checkbit & 1);
        send_buffer[ind] <<= 1;
        send_buffer[ind] |= ((checkbit>>1) & 1);
        send_buffer[ind] <<= 1;
        send_buffer[ind] |= ((buffer[i]>>7) & 1);
        send_buffer[ind] <<= 1;
        send_buffer[ind] |= ((checkbit>>2) & 1);
        send_buffer[ind] <<= 1;
        send_buffer[ind] |= ((buffer[i]>>6) & 1);
        send_buffer[ind] <<= 1;
        send_buffer[ind] |= ((buffer[i]>>5) & 1);
        send_buffer[ind] <<= 1;
        send_buffer[ind] |= ((buffer[i]>>4) & 1);
        send_buffer[ind] <<= 1;
        send_buffer[ind] |= ((checkbit>>3) & 1);
        send_buffer[ind+1] |= (buffer[i]<<4);
    }
}

