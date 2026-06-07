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
#include <netinet/sctp.h>
#include <signal.h>

// #define ... ...


int
main(int argc, char** argv){

    signal(SIGPIPE, SIG_IGN);
    // Check args
    if(argc != 2){
        fprintf(stderr, "ERROR: usage: %s <IPv4 address> \n", argv[0]);  // TODO: modify after project is done :)
                return -1;
    }

    // Initialize socket
    int sockfd;
    struct sockaddr_in servaddr;
    socklen_t servaddrlen = sizeof(servaddr);

    if(( sockfd = socket(AF_INET, SOCK_STREAM, IPPROTO_SCTP)) < 0){
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


    //simple test buffer
    char buf[1024];
    struct sctp_sndrcvinfo sri;
    int flags;
 

    if ( connect(sockfd, (struct sockaddr *) &servaddr, sizeof(servaddr)) < 0){
		syslog(LOG_ERR,"connect() error : %s\n", strerror(errno));
		return 1;
	}
    
    
        // TODO: Handle game logic
    for (;;) {
        flags = 0;
        int n = sctp_recvmsg(sockfd, buf, sizeof(buf) - 1,
                             NULL, NULL, &sri, &flags);
        if (n <= 0) {
            printf("Polaczenie zakonczone.\n");
            break;
        }
        if (flags & MSG_NOTIFICATION)
            continue;
 
        buf[n] = '\0';
        printf("%s", buf);
        fflush(stdout);
 
        if (strstr(buf, "Twoja tura 8==>") != NULL) {
            printf("> ");
            fflush(stdout);
 
            if (fgets(buf, sizeof(buf), stdin) == NULL)
                break;
 
            size_t len = strlen(buf);
            if (sctp_sendmsg(sockfd, buf, len, NULL, 0, 0, 0, 0, 0, 0) < 0) {
                perror("sctp_sendmsg() error");
                break;
            }
        }
    }
    exit(0);
}