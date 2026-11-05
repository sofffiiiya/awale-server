#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <time.h>
#include <dirent.h>

#include "game.h"

Game games[MAX_GAMES];

int game_create(int u0, int u1)
{
   int g;
   for(g = 0; g < MAX_GAMES; g++)
   {
      if(!games[g].active)
      {
         break;
      }
   }
   if(g == MAX_GAMES)
   {
      return -1;
   }

   memset(&games[g], 0, sizeof(Game));
   games[g].active = 1;
   games[g].players[0] = u0;
   games[g].players[1] = u1;
   strcpy(games[g].names[0], users[u0].name);
   strcpy(games[g].names[1], users[u1].name);
   games[g].abandoned_by = -1;
   games[g].first = rand() % 2;   /* the server decides who starts */
   awale_init(&games[g].board, games[g].first);

   users[u0].game = g;
   users[u1].game = g;
   return g;
}

void game_broadcast(int g, const char *fmt, ...)
{
   char message[8192];
   va_list args;
   int i;

   va_start(args, fmt);
   vsnprintf(message, sizeof message, fmt, args);
   va_end(args);

   user_send(games[g].players[0], "%s", message);
   user_send(games[g].players[1], "%s", message);
   for(i = 0; i < games[g].nb_observers; i++)
   {
      user_send(games[g].observers[i], "%s", message);
   }
}

void game_send_board(int g, int u)
{
   char text[2048];
   Game *game = &games[g];

   awale_print(&game->board, game->names[0], game->names[1], text, sizeof text);
   user_send(u, "[game %d] %s vs %s%s", g, game->names[0], game->names[1], text);

   if(game->board.over)
   {
      return;
   }
   if(game->players[game->board.turn] == u)
   {
      user_send(u, "It is YOUR turn: /move <1-6>\n");
   }
   else
   {
      user_send(u, "It is %s's turn.\n", game->names[game->board.turn]);
   }
}

void game_broadcast_board(int g)
{
   int i;
   game_send_board(g, games[g].players[0]);
   game_send_board(g, games[g].players[1]);
   for(i = 0; i < games[g].nb_observers; i++)
   {
      game_send_board(g, games[g].observers[i]);
   }
}

int game_player_of(int g, int u)
{
   if(games[g].players[0] == u)
   {
      return 0;
   }
   if(games[g].players[1] == u)
   {
      return 1;
   }
   return -1;
}

const char *game_play(int g, int u, int k)
{
   Game *game = &games[g];
   int player = game_player_of(g, u);
   int pit, taken;
   const char *error;

   if(player != game->board.turn)
   {
      return "it is not your turn";
   }
   pit = awale_pit_index(player, k);
   error = awale_move_error(&game->board, pit);
   if(error != NULL)
   {
      return error;
   }

   taken = awale_play(&game->board, pit);
   if(game->nb_moves < MAX_MOVES)
   {
      game->moves[game->nb_moves++] = pit;
   }

   game_broadcast(g, "\n%s played pit %d", game->names[player], k);
   if(taken > 0)
   {
      game_broadcast(g, " and captured %d seeds", taken);
   }
   game_broadcast(g, ".\n");
   game_broadcast_board(g);

   if(game->board.over)
   {
      game_end(g, "the game is finished");
   }
   return NULL;
}

/* private mode: a player in private mode only accepts his friends as observers */
int game_can_watch(int g, int u)
{
   int p;
   for(p = 0; p < 2; p++)
   {
      int player = games[g].players[p];
      if(users[player].private_mode && !user_is_friend(player, users[u].name))
      {
         return 0;
      }
   }
   return 1;
}

int game_add_observer(int g, int u)
{
   if(games[g].nb_observers >= MAX_OBSERVERS)
   {
      return 0;
   }
   games[g].observers[games[g].nb_observers++] = u;
   users[u].watching = g;
   return 1;
}

void game_remove_observer(int g, int u)
{
   int i;
   for(i = 0; i < games[g].nb_observers; i++)
   {
      if(games[g].observers[i] == u)
      {
         games[g].nb_observers--;
         games[g].observers[i] = games[g].observers[games[g].nb_observers];
         break;
      }
   }
   users[u].watching = -1;
}

void game_check_observers(int g)
{
   int i = 0;
   while(i < games[g].nb_observers)
   {
      int u = games[g].observers[i];
      if(!game_can_watch(g, u))
      {
         user_send(u, "You are no longer allowed to watch game %d (private mode).\n", g);
         game_remove_observer(g, u); /* the last observer moved to position i */
      }
      else
      {
         i++;
      }
   }
}

/*
 * Saved game file format (one file per game in SAVE_DIR):
 *   players alice bob
 *   first 1
 *   moves 8 2 11 ...        (board indexes 0..11)
 *   result alice 26 - 20 bob : the game is finished
 */
static void game_save(int g, const char *result)
{
   Game *game = &games[g];
   char path[128];
   FILE *f;
   int i;

   snprintf(path, sizeof path, "%s/%ld_%s_vs_%s.txt", SAVE_DIR, (long)time(NULL),
            game->names[0], game->names[1]);
   f = fopen(path, "w");
   if(f == NULL)
   {
      perror("fopen()");
      return;
   }
   fprintf(f, "players %s %s\n", game->names[0], game->names[1]);
   fprintf(f, "first %d\n", game->first);
   fprintf(f, "moves");
   for(i = 0; i < game->nb_moves; i++)
   {
      fprintf(f, " %d", game->moves[i]);
   }
   fprintf(f, "\nresult %s\n", result);
   fclose(f);
}

void game_end(int g, const char *reason)
{
   Game *game = &games[g];
   char result[256];
   int winner = awale_winner(&game->board);
   int i;

   if(game->abandoned_by != -1)
   {
      winner = 1 - game->abandoned_by; /* giving up = losing */
   }

   if(winner == -1)
   {
      snprintf(result, sizeof result, "%s %d - %d %s : %s, draw",
               game->names[0], game->board.score[0], game->board.score[1], game->names[1], reason);
   }
   else
   {
      snprintf(result, sizeof result, "%s %d - %d %s : %s, %s wins",
               game->names[0], game->board.score[0], game->board.score[1], game->names[1], reason,
               game->names[winner]);
   }

   game_broadcast(g, "\n*** End of game %d: %s ***\n", g, result);
   game_save(g, result);

   users[game->players[0]].game = -1;
   users[game->players[1]].game = -1;
   for(i = 0; i < game->nb_observers; i++)
   {
      users[game->observers[i]].watching = -1;
   }
   game->active = 0;
}

/* scandir filter: only .txt files */
static int is_save_file(const struct dirent *entry)
{
   return strstr(entry->d_name, ".txt") != NULL;
}

void game_list_saved(int u)
{
   struct dirent **list;
   int n, i;

   n = scandir(SAVE_DIR, &list, is_save_file, alphasort);
   if(n <= 0)
   {
      user_send(u, "No saved game.\n");
      return;
   }
   user_send(u, "Saved games (use /replay <number>):\n");
   for(i = 0; i < n; i++)
   {
      user_send(u, "  %2d. %s\n", i + 1, list[i]->d_name);
      free(list[i]);
   }
   free(list);
}

void game_replay(int u, int number)
{
   struct dirent **list;
   char path[512];
   char line[8192];
   char name0[NAME_LEN] = "", name1[NAME_LEN] = "";
   char result[256] = "";
   char *moves = NULL;
   int n, i, first = 0, count = 0;
   FILE *f;
   Board board;

   n = scandir(SAVE_DIR, &list, is_save_file, alphasort);
   if(n <= 0 || number < 1 || number > n)
   {
      user_send(u, "No such saved game, see /saved.\n");
      for(i = 0; i < n; i++) free(list[i]);
      if(n > 0) free(list);
      return;
   }
   snprintf(path, sizeof path, "%s/%s", SAVE_DIR, list[number - 1]->d_name);
   for(i = 0; i < n; i++) free(list[i]);
   free(list);

   f = fopen(path, "r");
   if(f == NULL)
   {
      user_send(u, "Cannot open this game.\n");
      return;
   }
   while(fgets(line, sizeof line, f) != NULL)
   {
      line[strcspn(line, "\n")] = 0;
      if(strncmp(line, "players ", 8) == 0)
      {
         sscanf(line + 8, "%15s %15s", name0, name1);
      }
      else if(strncmp(line, "first ", 6) == 0)
      {
         first = atoi(line + 6);
      }
      else if(strncmp(line, "moves", 5) == 0)
      {
         moves = strdup(line + 5);
      }
      else if(strncmp(line, "result ", 7) == 0)
      {
         strncpy(result, line + 7, sizeof result - 1);
      }
   }
   fclose(f);

   /* play the game again with the awale code and show every position */
   awale_init(&board, first);
   user_send(u, "=== Replay of %s vs %s (%s starts) ===\n", name0, name1, first == 0 ? name0 : name1);
   if(moves != NULL)
   {
      char *token = strtok(moves, " ");
      while(token != NULL)
      {
         char text[2048];
         int pit = atoi(token);
         int player = board.turn;
         count++;
         awale_play(&board, pit);
         awale_print(&board, name0, name1, text, sizeof text);
         user_send(u, "\nMove %d: %s plays pit %d%s", count, player == 0 ? name0 : name1,
                   pit % PITS_PER_SIDE + 1, text);
         token = strtok(NULL, " ");
      }
      free(moves);
   }
   user_send(u, "=== Result: %s ===\n", result);
}
