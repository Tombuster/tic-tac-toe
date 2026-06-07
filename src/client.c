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
#include <errno.h>
#include <syslog.h>


// #define ... ...


int
main(int argc, char** argv){
    // Check args
    if(argc != 2){
        fprintf(stderr, "ERROR: usage: %s <IPv4 address> \n", argv[0]);  // TODO: modify after project is done :)
                return -1;
    }

    // Initialize socket
    int sockfd;
    struct sockaddr_in servaddr;

    if(( sockfd = socket(AF_INET, SOCK_SEQPACKET, IPPROTO_SCTP)) < 0){
        perror("socket() error");
        syslog(LOG_ERR, "socket() error : %s\n", strerror(errno)); // TODO: Does the client need syslog?
        return -1;
    }

    bzero(&servaddr, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(2137);
    if ( inet_pton(AF_INET, argv[1], &servaddr.sin_addr) <= 0){
		syslog(LOG_ERR,"inet_pton() error : %s\n", strerror(errno));
		return -1;
	}

    if ( connect(sockfd, (struct sockaddr *) &servaddr, sizeof(servaddr)) < 0){
		syslog(LOG_ERR,"connect() error : %s\n", strerror(errno));
		return 1;
	}

        // TODO: Handle game logic

    exit(0);
}