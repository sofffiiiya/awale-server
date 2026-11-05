/*
 * Awale rules (abapa variant, as on the French Wikipedia page):
 *  - a player takes all seeds of one of his non-empty pits and sows them one by one
 *    in the following pits, counter-clockwise; the starting pit is skipped
 *    when there are 12 seeds or more;
 *  - if the last seed lands on the opponent's side and makes 2 or 3 seeds,
 *    those seeds are captured, then the previous pit is checked, and so on,
 *    as long as we stay on the opponent's side with 2 or 3 seeds;
 *  - a move that would capture ALL the opponent's seeds may be played but captures nothing;
 *  - if the opponent has no seed, the player must play a move that gives him seeds;
 *    if no such move exists, the game ends and each player takes the seeds on his side;
 *  - the first player with 25 seeds or more wins (24-24 is a draw).
 */
#include <stdio.h>
#include <string.h>

#include "awale.h"

void awale_init(Board *b, int first)
{
   int i;
   for(i = 0; i < PITS; i++)
   {
      b->pits[i] = SEEDS_PER_PIT;
   }
   b->score[0] = 0;
   b->score[1] = 0;
   b->turn = first;
   b->over = 0;
}

int awale_owner(int pit)
{
   return pit < PITS_PER_SIDE ? 0 : 1;
}

int awale_pit_index(int player, int k)
{
   if(k < 1 || k > PITS_PER_SIDE)
   {
      return -1;
   }
   return player * PITS_PER_SIDE + (k - 1);
}

int awale_side_seeds(const Board *b, int player)
{
   int i, sum = 0;
   for(i = 0; i < PITS_PER_SIDE; i++)
   {
      sum += b->pits[player * PITS_PER_SIDE + i];
   }
   return sum;
}

/* sow the seeds of 'pit', returns the index of the pit that got the last seed */
static int sow(Board *b, int pit)
{
   int seeds = b->pits[pit];
   int pos = pit;
   b->pits[pit] = 0;
   while(seeds > 0)
   {
      pos = (pos + 1) % PITS;
      if(pos == pit)
      {
         continue; /* never sow in the starting pit */
      }
      b->pits[pos]++;
      seeds--;
   }
   return pos;
}

/* capture for 'player' starting from the pit 'last', returns the captured seeds */
static int capture(Board *b, int last, int player)
{
   int opponent = 1 - player;
   Board copy = *b;
   int taken = 0;
   int pos = last;

   while(awale_owner(pos) == opponent && (copy.pits[pos] == 2 || copy.pits[pos] == 3))
   {
      taken += copy.pits[pos];
      copy.pits[pos] = 0;
      pos = (pos + PITS - 1) % PITS; /* previous pit */
   }

   /* "grand slam": taking every seed of the opponent is not allowed -> no capture */
   if(taken > 0 && awale_side_seeds(&copy, opponent) == 0)
   {
      return 0;
   }

   *b = copy;
   b->score[player] += taken;
   return taken;
}

const char *awale_move_error(const Board *b, int pit)
{
   int opponent;

   if(b->over)
   {
      return "the game is over";
   }
   if(pit < 0 || pit >= PITS)
   {
      return "invalid pit number (use 1 to 6)";
   }
   if(awale_owner(pit) != b->turn)
   {
      return "this pit is not yours";
   }
   if(b->pits[pit] == 0)
   {
      return "this pit is empty";
   }

   /* the opponent has no seed: we have to feed him */
   opponent = 1 - b->turn;
   if(awale_side_seeds(b, opponent) == 0)
   {
      Board copy = *b;
      sow(&copy, pit);
      if(awale_side_seeds(&copy, opponent) == 0)
      {
         return "your opponent has no seed, you must play a move that gives them some";
      }
   }
   return NULL;
}

/* does the current player have at least one legal move ? */
static int has_legal_move(const Board *b)
{
   int k;
   for(k = 1; k <= PITS_PER_SIDE; k++)
   {
      if(awale_move_error(b, awale_pit_index(b->turn, k)) == NULL)
      {
         return 1;
      }
   }
   return 0;
}

/* sets b->over if the game is finished */
static void check_end(Board *b)
{
   int i;

   if(b->score[0] >= WIN_SCORE || b->score[1] >= WIN_SCORE
      || (b->score[0] == 24 && b->score[1] == 24))
   {
      b->over = 1;
      return;
   }

   if(!has_legal_move(b))
   {
      /* nobody can feed anybody: each player takes the seeds on his side */
      for(i = 0; i < PITS; i++)
      {
         b->score[awale_owner(i)] += b->pits[i];
         b->pits[i] = 0;
      }
      b->over = 1;
   }
}

int awale_play(Board *b, int pit)
{
   int last, taken;

   if(awale_move_error(b, pit) != NULL)
   {
      return -1;
   }

   last = sow(b, pit);
   taken = capture(b, last, b->turn);
   b->turn = 1 - b->turn;
   check_end(b);
   return taken;
}

int awale_winner(const Board *b)
{
   if(b->score[0] > b->score[1])
   {
      return 0;
   }
   if(b->score[1] > b->score[0])
   {
      return 1;
   }
   return -1;
}

void awale_print(const Board *b, const char *name0, const char *name1, char *out, size_t size)
{
   int i;
   size_t len = 0;
   const char *line = "   +----+----+----+----+----+----+\n";

#define ADD(...) do { if(len < size) len += snprintf(out + len, size - len, __VA_ARGS__); } while(0)

   ADD("\n   %s : %d seeds captured\n", name1, b->score[1]);
   ADD("     6    5    4    3    2    1\n");
   ADD("%s", line);
   ADD("   |");
   for(i = PITS - 1; i >= PITS_PER_SIDE; i--)
   {
      ADD(" %2d |", b->pits[i]);
   }
   ADD("  <- %s\n", name1);
   ADD("%s", line);
   ADD("   |");
   for(i = 0; i < PITS_PER_SIDE; i++)
   {
      ADD(" %2d |", b->pits[i]);
   }
   ADD("  <- %s\n", name0);
   ADD("%s", line);
   ADD("     1    2    3    4    5    6\n");
   ADD("   %s : %d seeds captured\n", name0, b->score[0]);

#undef ADD
}
