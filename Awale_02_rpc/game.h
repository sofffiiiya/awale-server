#ifndef GAME_H
#define GAME_H

#include "awale.h"
#include "user.h"

#define MAX_GAMES     50
#define MAX_OBSERVERS 20
#define MAX_MOVES     1000
#define SAVE_DIR      "games"

/* one match between two connected users */
typedef struct
{
   int active;                 /* 1 if this slot is used */
   int players[2];             /* user indexes, players[0] owns pits 0..5 */
   char names[2][NAME_LEN];
   Board board;
   int first;                  /* who started (0 or 1) */
   int moves[MAX_MOVES];       /* board indexes played, for saving */
   int nb_moves;
   int observers[MAX_OBSERVERS];
   int nb_observers;
   int abandoned_by;           /* player (0 or 1) who gave up, -1 if nobody */
} Game;

extern Game games[MAX_GAMES];

/* creates a game between two users, the first player is chosen randomly.
 * Returns the game index or -1 if there is no free slot. */
int game_create(int u0, int u1);

/* printf-like send to both players and every observer */
void game_broadcast(int g, const char *fmt, ...);

/* sends the board and whose turn it is to one user */
void game_send_board(int g, int u);
/* ... and to everybody in the game */
void game_broadcast_board(int g);

/* player 0 or 1 of user u in game g, -1 if u is not a player */
int game_player_of(int g, int u);

/* plays pit k (1..6) for user u. Returns NULL if ok, or an error message */
const char *game_play(int g, int u, int k);

/* observers and private mode */
int game_can_watch(int g, int u);
int game_add_observer(int g, int u);
void game_remove_observer(int g, int u);
void game_check_observers(int g);   /* removes observers who are no more allowed */

/* ends the game: shows the result, saves it in SAVE_DIR and frees the slot.
 * 'reason' is shown to everybody, e.g. "bob abandoned". */
void game_end(int g, const char *reason);

/* saved games */
void game_list_saved(int u);
void game_replay(int u, int number);

#endif /* guard */
