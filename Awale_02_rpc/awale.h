#ifndef AWALE_H
#define AWALE_H

#include <stddef.h>

/*
 * Awale board.
 *
 * 12 pits, indexes 0..11, seeds move counter-clockwise (index + 1).
 * Player 0 owns pits 0..5, player 1 owns pits 6..11.
 *
 *            player 1
 *     11  10   9   8   7   6
 *      0   1   2   3   4   5
 *            player 0
 *
 * A player names his own pits 1..6 (see awale_pit_index()).
 */

#define PITS          12
#define PITS_PER_SIDE 6
#define SEEDS_PER_PIT 4
#define WIN_SCORE     25   /* more than half of the 48 seeds */

typedef struct
{
   int pits[PITS];
   int score[2];
   int turn;   /* 0 or 1: who has to play */
   int over;   /* 1 when the game is finished */
} Board;

/* start a new game, 'first' (0 or 1) plays first */
void awale_init(Board *b, int first);

/* owner (0 or 1) of a pit index */
int awale_owner(int pit);

/* convert "pit number k (1..6) of player p" to a board index, -1 if k is invalid */
int awale_pit_index(int player, int k);

/* number of seeds on the side of a player */
int awale_side_seeds(const Board *b, int player);

/* NULL if the current player may play 'pit', otherwise a message explaining why not */
const char *awale_move_error(const Board *b, int pit);

/* play a move for b->turn. Returns the number of captured seeds, or -1 if illegal.
 * Also checks the end of game (b->over). */
int awale_play(Board *b, int pit);

/* 0 or 1 = winner, -1 = draw. Only meaningful when b->over */
int awale_winner(const Board *b);

/* writes an ASCII drawing of the board in 'out' */
void awale_print(const Board *b, const char *name0, const char *name1, char *out, size_t size);

#endif /* guard */
