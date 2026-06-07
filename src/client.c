#define _GNU_SOURCE

#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <net/if.h>
#include <netinet/tcp.h>
#include <strings.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <syslog.h>


int
main(int argc, char** argv){
    // Check args
    if(argc != 2){
        fprintf(stderr, "ERROR: usage: %s <IPv4 address> \n", argv[0]);  // TODO: modify after project is done :)
                return 1;
    }

    // Initialize socket
    int local_sockfd;
    if((local_sockfd = socket(AF_INET, SOCK_SEQPACKET, IPPROTO_SCTP)) < 0){
        perror("socket() error");
        syslog(LOG_ERR, "%s: socket() error", argv[0]);
        return -1;
    }

    

    exit(0);
}