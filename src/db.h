#include <stddef.h>
#ifndef DB_H
#define DB_H

#define MAX_NICK 64
#define DB_PATH "./ranking.csv"

//inicjalizacja db
void db_init(void);

//wynik pojedynczej gry do zapisania w db
int db_record_result(const char *winner, const char *loser, int points_win);

//zwraca punkty gracza

int db_get_points(const char *nick);

//zwraca ranking
void db_get_leaderboard(char *buf, size_t bufsize);

#endif