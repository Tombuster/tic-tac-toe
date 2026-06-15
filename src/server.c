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

void format_board(const char board[9], char *out, size_t outsize){
    char cell[9];
    for (int i = 0; i < 9; i++)
        cell[i] = board[i] == ' ' ? ('1' + i) : board[i];

    snprintf(out, outsize,
             "\n %c | %c | %c \n"
             "-----------\n"
             " %c | %c | %c \n"
             "-----------\n"
             " %c | %c | %c \n\n",
             cell[0], cell[1], cell[2],
             cell[3], cell[4], cell[5],
             cell[6], cell[7], cell[8]);
}

char check_winner(const char board[9]){
    static const int win_lines[8][3] ={
        {0,1,2}, {3,4,5}, {6,7,8}, //wiersze
        {0,3,6}, {1,4,7}, {2,5,8}, //kolumny
        {0,4,8}, {2,4,6} //przekatne
    };

    for (int i = 0; i < 8; i++){
        int a = win_lines[i][0], c = win_lines[i][1], d = win_lines[i][2];
        if (board[a] != ' ' && board[a] == board[c] && board[c] == board[d]){
            return board[a];
        }
    }
    //brak zwyciezcy - czy plansza pelna?
    for (int i = 0; i < 9; i++){
        if (board[i] == ' '){
            return ' '; //gra trwa
        }
    }
    return 'D'; //remis
}

static void handle_game(int connA, int connB){
    char buf[1024];
    struct sctp_sndrcvinfo sri;
    int flags;
    int whose_turn = 0; // 0 zaczyna A, 1 zaczyna B
    char game_board[9];
    memset(game_board, ' ', sizeof(game_board));
    fprintf(stderr, "[SRV] start: connA=%d connB=%d\n", connA, connB);
    
    const char *welcome = "Polaczono, zaczynasz Alice, grasz X\n";
    const char *wait    = "Polaczono, ALE CZEKAJ NA SWOJA TURE BOB, grasz O\n";

    format_board(game_board, buf, sizeof(buf));
    int wa = sctp_sendmsg(connA, welcome, strlen(welcome), NULL, 0, 0, 0, 0, 0, 0);
    sctp_sendmsg(connA, buf, 1024, NULL, 0, 0, 0, 0, 0, 0); 
        
    int wb = sctp_sendmsg(connB, wait, strlen(wait), NULL, 0, 0, 0, 0, 0, 0);
    sctp_sendmsg(connB, buf, 1024, NULL, 0, 0, 0, 0, 0, 0);
    memset(buf, 0, sizeof(buf));
    fprintf(stderr, "[SRV] welcome wa=%d wb=%d\n", wa, wb);

    const char *start = "Twoja tura!\n";
    sctp_sendmsg(connA, start, strlen(start), NULL, 0, 0, 0, 0, 0, 0);
    for (;;) {
        int active, other;
        char *active_nick, *other_nick, active_mark, other_mark;
        if (whose_turn == 0){
            active = connA;
            active_nick = "Alice"; //testowy nick
            active_mark = 'X';
            other = connB;
            other_nick = "Bob"; //testowy nick
            other_mark = 'O';
        }
        else{
            active = connB;
            active_nick = "Bob";
            active_mark = 'O';
            other = connA;
            other_nick = "Alice";
            other_mark = 'X';
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
        
        buf[strcspn(buf, "\r\n")] = '\0';

        int pole = atoi(buf) - 1;
        //if (strcmp(buf, "wygralem\n\0") == 0){ //testowy blok
        //    char msg[128];
        //    char msg2[128];
        //    snprintf(msg, sizeof(msg), "Gratulacje! Wygrales/as %s\n", active_nick);
        //    int s = sctp_sendmsg(active, msg, 128, NULL, 0, 0, 0, 0, 0, 0);
        //    snprintf(msg2,sizeof(msg2), "Gratulacje! Przegrales/as %s\n", other_nick);
        //    int g = sctp_sendmsg(other, msg2, 128, NULL, 0, 0, 0, 0, 0, 0);
        //    db_record_result(active_nick, other_nick, 3);
        //    fprintf(stderr, "[SRV] koniec, zamykam %d i %d\n", connA, connB);
        //    printf("alice points: %d\n", db_get_points("alice"));
        //    printf("bob points:   %d\n", db_get_points("bob"));
       //     close(connA);
       //     close(connB);
      //  }
        game_board[pole] = active_mark;
        char winner = check_winner(game_board);
        if (winner == 'X' || winner == 'O'){
            char msg_win[128], msg_lose[128];
            snprintf(msg_win, sizeof(msg_win), "Gratulacje %s!\nWygrales zlote gacie!\n", active_nick);
            sctp_sendmsg(active, msg_win, 128, NULL, 0, 0, 0, 0, 0, 0);
            snprintf(msg_lose, sizeof(msg_lose), "Gratulacje %s!\nPrzegrales zlote gacie!\n", other_nick);
            sctp_sendmsg(other, msg_lose, 128, NULL, 0, 0, 0, 0, 0, 0);
            db_record_result(active_nick, other_nick, 3);
            fprintf(stderr, "[SRV] koniec, zamykam %d i %d\n", connA, connB);
            close(connA);
            close(connB);
        }
        else if (winner == 'D'){
            char msg_win[128], msg_lose[128];
            snprintf(msg_win, sizeof(msg_win), "Gratulacje %s!\nRemis o zlote gacie!\n", active_nick);
            sctp_sendmsg(active, msg_win, 128, NULL, 0, 0, 0, 0, 0, 0);
            snprintf(msg_lose, sizeof(msg_lose), "Gratulacje %s!\nRemis o zlote gacie!\n", other_nick);
            sctp_sendmsg(other, msg_lose, 128, NULL, 0, 0, 0, 0, 0, 0);
            fprintf(stderr, "[SRV] koniec, zamykam %d i %d\n", connA, connB);
            close(connA);
            close(connB);
        }
        
        memset(buf, 0, sizeof(buf));
        format_board(game_board, buf, sizeof(buf));
        int s = sctp_sendmsg(active, buf, 1024, NULL, 0, 0, 0, 0, 0, 0); 
        int g = sctp_sendmsg(other, buf, 1024, NULL, 0, 0, 0, 0, 0, 0);
        fprintf(stderr, "[SRV] fwd to fd=%d s=%d\n", active, g); 
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