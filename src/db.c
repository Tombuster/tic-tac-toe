#define _GNU_SOURCE
#include "db.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>

#define MAX_USERS 32

#define POINTS_LOSS 0 

struct user {
    char nick[MAX_NICK];
    int wins;
    int losses;
    int points;
};

static pthread_mutex_t db_lock = PTHREAD_MUTEX_INITIALIZER;

void db_init(void){
    FILE *f = fopen(DB_PATH, "r");
    if (f) { fclose(f); return ;} //istnieje
    f = fopen(DB_PATH, "w");
    if (!f) { perror("db_init fopen"); return; }
    fprintf(f, "username,wins,losses,points\n"); //wpisujemy pierszy rzad do pliku csv
    fclose(f);
}


static int db_load(struct user *users, int max){
    FILE *f = fopen(DB_PATH, "r");
    if (!f) return -1;

    char line[256];
    int count = 0;

    if (!fgets(line, sizeof(line), f)) { fclose(f); return 0; } // pomijamy naglowek, sprawdzamy przy okazji czy wczytanie nie zwraca NULL -> jedyny przypadek gdy program wyjdzie

    while ( count < max && fgets(line, sizeof(line), f)) {
        struct user *u = &users[count];
        if (sscanf(line, "%63[^,],%d,%d,%d", u->nick, &u->wins, &u->losses, &u->points) == 4){
            count++;
        }
    }
    fclose(f);
    return count;
}


static int db_save(struct user *users, int count){
    char tmp[] = DB_PATH ".tmp";
    FILE *f = fopen(tmp, "w");
    if (!f) { perror("db_save fopen"); return -1; }

    fprintf(f, "username,wins,losses,points\n");
    for (int i = 0; i < count; i++){
        fprintf(f, "%s,%d,%d,%d\n", users[i].nick, users[i].wins, users[i].losses, users[i].points);
    }
    fclose(f);
    if (rename(tmp, DB_PATH) < 0) { perror("db_save rename"); return -1; }
    return 0;
}

static int find_user(struct user *users, int count, const char *nick){
    for (int i = 0; i < count; i++){
        if (strcmp(users[i].nick, nick) == 0){
            return i;
        }
    }
    return -1;
}

static int add_user(struct user *users, int *count, const char *nick){
    if (*count >= MAX_USERS) return -1;
    struct user *u = &users[*count];
    strncpy(u->nick, nick, MAX_NICK - 1);
    u->nick[MAX_NICK - 1] = '\0';
    u->wins = u->losses = u->points = 0;
    return (*count)++;
}

int db_record_result(const char *winner, const char *loser, int points_win){
    pthread_mutex_lock(&db_lock);

    static struct user users[MAX_USERS];
    int count = db_load(users, MAX_USERS);
    if (count < 0) count = 0;

    int winner_index = find_user(users, count, winner);
    if (winner_index < 0) {
        winner_index = add_user(users, &count, winner);
    }
    int loser_index = find_user(users, count, loser);
    if (loser_index < 0){
        loser_index = add_user(users, &count, loser);
    }

    int ret = -1;
    if (winner_index >= 0 && loser_index >= 0){
        users[winner_index].wins++;
        users[winner_index].points += points_win;
        users[loser_index].losses++;
        users[loser_index].points += POINTS_LOSS;
        ret = db_save(users, count);
    }

    pthread_mutex_unlock(&db_lock);
    return ret;
}

int db_get_points(const char *nick){
    pthread_mutex_lock(&db_lock);

    static struct user users[MAX_USERS];
    int count = db_load(users, MAX_USERS);
    int points = -1;

    if (count > 0){
        int i = find_user(users, count, nick);
        if (i>=0) points = users[i].points;
    }
    pthread_mutex_unlock(&db_lock);
    return points;
}



