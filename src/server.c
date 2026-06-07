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
#include <signal.h>
#include <unistd.h>
#include <errno.h>
#include <syslog.h>


#define LISTENQ 5

 
int
main(int argc, char** argv){
    // Check args
    if(argc != 1){
        fprintf(stderr, "ERROR: usage: %s\n", argv[0]);  // TODO: modify after project is done :)
                return 1;
    }


        // TODO: handle SIGCHLD


    // Initialize listening socket
    int listenfd, connfd;
    struct sockaddr_in servaddr, cliaddr;
    size_t servlen, clilen;
    pid_t childpid;

    if(( listenfd = socket(AF_INET, SOCK_SEQPACKET, IPPROTO_SCTP)) < 0){
        perror("socket() error");  // debug info
        syslog(LOG_ERR, "socket() error : %s\n", strerror(errno));
        return -1;
    }

    bzero(&servaddr, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(2137);
    servaddr.sin_addr.s_addr = INADDR_ANY;
    servlen = sizeof(servaddr);
    
    if ( bind( listenfd, (struct sockaddr *) &servaddr, servlen) < 0){
            syslog (LOG_ERR,"bind() error : %s\n", strerror(errno));
            return -1;
    }

    if ( listen( listenfd, LISTENQ) < 0){
            syslog (LOG_ERR,"listen() error : %s\n", strerror(errno));
            return -1;
    }
    
    for ( ; ; ) {
		clilen = sizeof(cliaddr);
        if ( (connfd = accept(listenfd, (struct sockaddr *) &cliaddr, &clilen)) < 0) {
			if (errno == EINTR)
				continue;		/* back to for() */
			else
				syslog(LOG_ERR, "accept() error : %s\n", strerror(errno));
				return -1;
		}

		if ( (childpid = fork()) == 0) {	/* child process */
			close(listenfd);	/* close listening socket */
			/* process the request on connfd*/

                // TODO: process request

			exit(0);
		}
    }

    exit(0);
}