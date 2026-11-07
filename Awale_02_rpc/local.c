/*
 * Step 0: the Awale game alone, without network.
 * Two players on the same keyboard: ./local
 */
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include "awale.h"

int main(void)
{
   const char *names[2] = { "Player1", "Player2" };
   char text[2048];
   Board board;
   int k;

   srand(time(NULL));
   awale_init(&board, rand() % 2);

   while(!board.over)
   {
      const char *error;
      int pit;

      awale_print(&board, names[0], names[1], text, sizeof text);
      printf("%s%s, choose a pit (1-6): ", text, names[board.turn]);
      if(scanf("%d", &k) != 1)
      {
         return EXIT_SUCCESS;
      }
      pit = awale_pit_index(board.turn, k);
      error = awale_move_error(&board, pit);
      if(error != NULL)
      {
         printf("Illegal move: %s\n", error);
         continue;
      }
      printf("Captured: %d\n", awale_play(&board, pit));
   }

   awale_print(&board, names[0], names[1], text, sizeof text);
   printf("%s", text);
   k = awale_winner(&board);
   if(k == -1)
   {
      printf("Draw!\n");
   }
   else
   {
      printf("%s wins!\n", names[k]);
   }
   return EXIT_SUCCESS;
}
