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
#include <netinet/sctp.h>
#include <sys/wait.h>
#include <pthread.h>
#include "db.h"

#define LISTENQ 5

struct game_args {
    int connA;
    int connB;
};


static void handle_game(int connA, int connB){
    char buf[1024];
    struct sctp_sndrcvinfo sri;
    int flags;
    int whose_turn = 0; // 0 zaczyna A, 1 zaczyna B

    fprintf(stderr, "[SRV] start: connA=%d connB=%d\n", connA, connB);
    
    const char *welcome = "Twoja tura, Alice\n";
    const char *wait    = "Polaczono, ALE CZEKAJ NA SWOJA TURE BOB\n";


    int wa = sctp_sendmsg(connA, welcome, strlen(welcome), NULL, 0, 0, 0, 0, 0, 0);
    int wb = sctp_sendmsg(connB, wait,    strlen(wait),    NULL, 0, 0, 0, 0, 0, 0);
    fprintf(stderr, "[SRV] welcome wa=%d wb=%d\n", wa, wb);

    for (;;) {
        int active, other;
        char *active_nick, *other_nick;
        if (whose_turn == 0){
            active = connA;
            active_nick = "Alice"; //testowy nick
            other = connB;
            other_nick = "Bob"; //testowy nick
        }
        else{
            active = connB;
            active_nick = "Bob";
            other = connA;
            other_nick = "Alice";
        }
 
        flags = 0;
        int n = sctp_recvmsg(active, buf, sizeof(buf) - 1,
                             NULL, NULL, &sri, &flags);

        fprintf(stderr, "[SRV] recv fd=%d n=%d errno=%d(%s) flags=0x%x notif=%d\n",
                active, n, errno, strerror(errno), flags, !!(flags&MSG_NOTIFICATION));

        if (n <= 0)            /* rozlaczenie albo blad -> koniec rozmowy */
            break;
 
        if (flags & MSG_NOTIFICATION)   /* powiadomienie, nie dane */
            continue;
 
        buf[n] = '\0';
        
        if (strcmp(buf, "wygralem\n\0") == 0){ //testowy blok
            char msg[128];
            char msg2[128];
            snprintf(msg, sizeof(msg), "Gratulacje! Wygrales/as %s\n", active_nick);
            int s = sctp_sendmsg(active, msg, 128, NULL, 0, 0, 0, 0, 0, 0);
            snprintf(msg2,sizeof(msg2), "Gratulacje! Przegrales/as %s\n", other_nick);
            int g = sctp_sendmsg(other, msg2, 128, NULL, 0, 0, 0, 0, 0, 0);
            db_record_result(active_nick, other_nick, 3);
            fprintf(stderr, "[SRV] koniec, zamykam %d i %d\n", connA, connB);
            printf("alice points: %d\n", db_get_points("alice"));
            printf("bob points:   %d\n", db_get_points("bob"));
            close(connA);
            close(connB);
        }
        int s = sctp_sendmsg(other, buf, n, NULL, 0, 0, 0, 0, 0, 0);
        fprintf(stderr, "[SRV] fwd to fd=%d s=%d\n", other, s);            /* drugi sie rozlaczyl */
            
        const char *go = "Twoja tura\n";
        sctp_sendmsg(other, go, strlen(go), NULL, 0, 0, 0, 0, 0, 0);
 
        whose_turn = !whose_turn;
    }
    fprintf(stderr, "[SRV] koniec, zamykam %d i %d\n", connA, connB);
    close(connA);
    close(connB);
}


static void *game_thread(void *arg){
    struct game_args *ga = (struct game_args *)arg;
    int connA = ga->connA;
    int connB = ga->connB;
    free(ga);

    pthread_detach(pthread_self());
    handle_game(connA, connB);
    return NULL;
}



 
int
main(int argc, char** argv){
    // Check args
    if(argc != 1){
        fprintf(stderr, "ERROR: usage: %s\n", argv[0]);  // TODO: modify after project is done :)
                return 1;
    }

    signal(SIGPIPE, SIG_IGN);
    //handle SIGCHLD
    struct sigaction sa;
    //sa.sa_handler = child_handler;

    sa.sa_flags = SA_RESTART;
    sigemptyset(&sa.sa_mask);
    //sigaction(SIGCHLD, &sa, NULL);

    // Initialize listening socket
    int listenfd, connfd;
    struct sockaddr_in servaddr, cliaddr;
    socklen_t servlen, clilen;
    pid_t childpid;

    // sctp required variables
    char test_buff[1024];
    struct sctp_sndrcvinfo sndrcvinfo;
    int flags = 0;
    sctp_assoc_t waiting_assoc = 0;
    int have_waiting = 0;

    if(( listenfd = socket(AF_INET, SOCK_STREAM, IPPROTO_SCTP)) < 0){
        perror("socket() error");  // debug info
        syslog(LOG_ERR, "socket() error : %s\n", strerror(errno));
        return -1;
    }
    int opt = 1;
    setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));


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
    
    db_init();


    int waiting_fd = -1;        /* connfd pierwszego klienta czekajacego na pare */
 
    for (;;) {
        clilen = sizeof(cliaddr);
        connfd = accept(listenfd, (struct sockaddr *)&cliaddr, &clilen);
        if (connfd < 0) {
            if (errno == EINTR)
                continue;       /* przerwane przez SIGCHLD -> ponow */
            syslog(LOG_ERR, "accept() error : %s", strerror(errno));
            continue;
        }
 
        if (waiting_fd < 0) {
            /* pierwszy z pary -> czeka */
            waiting_fd = connfd;
            const char *msg = "Czekam na drugiego gracza...\n";
            sctp_sendmsg(connfd, msg, strlen(msg), NULL, 0, 0, 0, 0, 0, 0);
        } else {
            /* mamy pare -> fork i obsluga rozmowy */
            struct game_args *ga = malloc(sizeof(*ga));
            if (!ga){
                close(waiting_fd);
                close(connfd);
                waiting_fd = -1;
                continue;
            }
            ga->connA = waiting_fd;
            ga->connB = connfd;

            pthread_t tid;

            if (pthread_create(&tid, NULL, game_thread, ga) != 0) {
                syslog(LOG_ERR, "pthread_create() error : %s", strerror(errno));
                close(waiting_fd);
                close(connfd);
                free(ga);
            }
            waiting_fd = -1;
        }
    } 
    
}