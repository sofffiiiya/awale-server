/*
 * Text protocol between client and server: one command per line.
 * A line that does not start with '/' is a chat message.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "commands.h"
#include "user.h"
#include "game.h"

static const char *HELP =
   "Commands:\n"
   "  /help                     this help\n"
   "  /list                     online users\n"
   "  /challenge <user>         ask <user> for a game\n"
   "  /accept <user>            accept the challenge of <user>\n"
   "  /refuse <user>            refuse the challenge of <user>\n"
   "  /move <1-6>               play one of your pits (during a game)\n"
   "  /board                    show the board again\n"
   "  /abandon                  give up the current game\n"
   "  /games                    ongoing games\n"
   "  /watch <game>             watch a game, /unwatch to stop\n"
   "  <text>                    chat: to your game if you play/watch one, else to everybody\n"
   "  /all <text>               chat to everybody\n"
   "  /msg <user> <text>        private message\n"
   "  /bio [user]               show a bio (yours by default)\n"
   "  /bioadd <text>            add a line to your bio (max 10), /bioclear to erase it\n"
   "  /private on|off           only friends may watch your games\n"
   "  /friend add|del <user>    manage your friends, /friends to list them\n"
   "  /saved                    list saved games, /replay <number> to review one\n"
   "  /quit                     leave\n";

/* splits "word rest of line": returns the rest (may be "") and cuts 'line' after the word */
static char *next_word(char *line)
{
   char *p = line;
   while(*p && !isspace((unsigned char)*p))
   {
      p++;
   }
   if(*p)
   {
      *p = 0;
      p++;
      while(isspace((unsigned char)*p))
      {
         p++;
      }
   }
   return p;
}

static int valid_name(const char *name)
{
   int i;
   int len = strlen(name);
   if(len < 1 || len >= NAME_LEN)
   {
      return 0;
   }
   for(i = 0; i < len; i++)
   {
      if(!isalnum((unsigned char)name[i]) && name[i] != '_' && name[i] != '-')
      {
         return 0;
      }
   }
   return 1;
}

/* sends a message to every logged in user except 'except' (-1 = nobody) */
static void send_to_all(int except, const char *message)
{
   int i;
   for(i = 0; i < MAX_CLIENTS; i++)
   {
      if(users[i].in_use && users[i].logged_in && i != except)
      {
         user_send(i, "%s", message);
      }
   }
}

int command_login(int u, char *line)
{
   char message[128];

   if(!valid_name(line))
   {
      user_send(u, "Invalid username (1-15 letters, digits, '_' or '-').\n");
      return 0;
   }
   if(user_find(line) != -1)
   {
      user_send(u, "The username %s is already used.\n", line);
      return 0;
   }

   strcpy(users[u].name, line);
   users[u].logged_in = 1;
   user_load_profile(u);

   user_send(u, "Welcome %s! Type /help for the list of commands.\n", users[u].name);
   snprintf(message, sizeof message, "* %s is now online\n", users[u].name);
   send_to_all(u, message);
   return 1;
}

/* ---------- 2. list of users ---------- */

static void cmd_list(int u)
{
   int i;
   user_send(u, "Online users:\n");
   for(i = 0; i < MAX_CLIENTS; i++)
   {
      if(!users[i].in_use || !users[i].logged_in)
      {
         continue;
      }
      if(users[i].game != -1)
      {
         user_send(u, "  %-15s playing (game %d)\n", users[i].name, users[i].game);
      }
      else if(users[i].watching != -1)
      {
         user_send(u, "  %-15s watching game %d\n", users[i].name, users[i].watching);
      }
      else
      {
         user_send(u, "  %-15s available\n", users[i].name);
      }
   }
}

/* ---------- 3. challenges ---------- */

static void cmd_challenge(int u, const char *name)
{
   int other = user_find(name);

   if(other == -1)
   {
      user_send(u, "Unknown user %s.\n", name);
   }
   else if(other == u)
   {
      user_send(u, "You cannot challenge yourself.\n");
   }
   else if(users[u].game != -1)
   {
      user_send(u, "You are already playing.\n");
   }
   else if(users[other].game != -1)
   {
      user_send(u, "%s is already playing.\n", name);
   }
   else
   {
      strcpy(users[u].challenge_to, name);
      user_send(u, "Challenge sent to %s.\n", name);
      user_send(other, "%s challenges you! Answer with /accept %s or /refuse %s\n",
                users[u].name, users[u].name, users[u].name);
   }
}

/* returns the user who challenged u, -1 if 'name' did not challenge u */
static int find_challenger(int u, const char *name)
{
   int other = user_find(name);
   if(other == -1 || strcmp(users[other].challenge_to, users[u].name) != 0)
   {
      return -1;
   }
   return other;
}

static void cmd_accept(int u, const char *name)
{
   int other = find_challenger(u, name);
   int g;

   if(other == -1)
   {
      user_send(u, "%s did not challenge you.\n", name);
      return;
   }
   if(users[u].game != -1 || users[other].game != -1)
   {
      user_send(u, "One of you is already playing.\n");
      return;
   }

   users[other].challenge_to[0] = 0;
   /* a player cannot watch a game while playing */
   if(users[u].watching != -1) game_remove_observer(users[u].watching, u);
   if(users[other].watching != -1) game_remove_observer(users[other].watching, other);

   g = game_create(other, u);
   if(g == -1)
   {
      user_send(u, "Too many games, try later.\n");
      return;
   }
   game_broadcast(g, "\nGame %d starts: %s vs %s. The server chose %s to start.\n",
                  g, games[g].names[0], games[g].names[1], games[g].names[games[g].first]);
   game_broadcast_board(g);
}

static void cmd_refuse(int u, const char *name)
{
   int other = find_challenger(u, name);
   if(other == -1)
   {
      user_send(u, "%s did not challenge you.\n", name);
      return;
   }
   users[other].challenge_to[0] = 0;
   user_send(other, "%s refused your challenge.\n", users[u].name);
   user_send(u, "Challenge refused.\n");
}

/* ---------- 4. moves ---------- */

static void cmd_move(int u, const char *arg)
{
   const char *error;
   int k;

   if(users[u].game == -1)
   {
      user_send(u, "You are not playing.\n");
      return;
   }
   k = atoi(arg);
   error = game_play(users[u].game, u, k);
   if(error != NULL)
   {
      user_send(u, "Illegal move: %s.\n", error);
   }
}

static void cmd_abandon(int u)
{
   int g = users[u].game;
   char reason[64];
   if(g == -1)
   {
      user_send(u, "You are not playing.\n");
      return;
   }
   games[g].abandoned_by = game_player_of(g, u);
   games[g].board.over = 1;
   snprintf(reason, sizeof reason, "%s abandoned", users[u].name);
   game_end(g, reason);
}

/* ---------- 5. games and observers ---------- */

static void cmd_games(int u)
{
   int g, count = 0;
   user_send(u, "Ongoing games:\n");
   for(g = 0; g < MAX_GAMES; g++)
   {
      if(games[g].active)
      {
         int p0 = games[g].players[0], p1 = games[g].players[1];
         user_send(u, "  %2d: %s (%d) vs %s (%d), %d observer(s)%s\n", g,
                   games[g].names[0], games[g].board.score[0],
                   games[g].names[1], games[g].board.score[1], games[g].nb_observers,
                   (users[p0].private_mode || users[p1].private_mode) ? " [private]" : "");
         count++;
      }
   }
   if(count == 0)
   {
      user_send(u, "  none\n");
   }
}

static void cmd_watch(int u, const char *arg)
{
   int g = atoi(arg);

   if(*arg == 0 || g < 0 || g >= MAX_GAMES || !games[g].active)
   {
      user_send(u, "No such game, see /games.\n");
      return;
   }
   if(users[u].game != -1)
   {
      user_send(u, "You are playing, you cannot watch another game.\n");
      return;
   }
   if(!game_can_watch(g, u))
   {
      user_send(u, "This game is private, only friends of the players may watch it.\n");
      return;
   }
   if(users[u].watching == g)
   {
      user_send(u, "You are already watching this game.\n");
      return;
   }
   if(users[u].watching != -1)
   {
      game_remove_observer(users[u].watching, u);
   }
   if(!game_add_observer(g, u))
   {
      user_send(u, "Too many observers.\n");
      return;
   }
   game_broadcast(g, "* %s is now watching game %d\n", users[u].name, g);
   game_send_board(g, u);
}

static void cmd_unwatch(int u)
{
   if(users[u].watching == -1)
   {
      user_send(u, "You are not watching any game.\n");
      return;
   }
   user_send(u, "You stopped watching game %d.\n", users[u].watching);
   game_remove_observer(users[u].watching, u);
}

/* ---------- 6. chat ---------- */

static void cmd_all(int u, const char *text)
{
   char message[BUF_SIZE + NAME_LEN + 16];
   snprintf(message, sizeof message, "[all] %s: %s\n", users[u].name, text);
   send_to_all(u, message);
}

static void cmd_chat(int u, const char *text)
{
   int g = users[u].game != -1 ? users[u].game : users[u].watching;
   int i;

   if(g == -1)
   {
      cmd_all(u, text);
      return;
   }
   /* send to the people in the game, except the sender */
   for(i = 0; i < 2; i++)
   {
      if(games[g].players[i] != u)
      {
         user_send(games[g].players[i], "[game %d] %s: %s\n", g, users[u].name, text);
      }
   }
   for(i = 0; i < games[g].nb_observers; i++)
   {
      if(games[g].observers[i] != u)
      {
         user_send(games[g].observers[i], "[game %d] %s: %s\n", g, users[u].name, text);
      }
   }
}

static void cmd_msg(int u, char *args)
{
   char *text = next_word(args);
   int other = user_find(args);
   if(other == -1 || *text == 0)
   {
      user_send(u, "Usage: /msg <user> <text> (user must be online).\n");
      return;
   }
   user_send(other, "[private] %s: %s\n", users[u].name, text);
}

/* ---------- 7. bio ---------- */

static void cmd_bio(int u, const char *name)
{
   int other = *name ? user_find(name) : u;
   int i;

   if(other == -1)
   {
      user_send(u, "Unknown user %s (must be online).\n", name);
      return;
   }
   if(users[other].bio_lines == 0)
   {
      user_send(u, "%s has no bio.\n", users[other].name);
      return;
   }
   user_send(u, "--- Bio of %s ---\n", users[other].name);
   for(i = 0; i < users[other].bio_lines; i++)
   {
      user_send(u, "%s\n", users[other].bio[i]);
   }
   user_send(u, "-----------------\n");
}

static void cmd_bioadd(int u, const char *text)
{
   int i;
   if(users[u].bio_lines >= BIO_LINES)
   {
      user_send(u, "Your bio is full (%d lines), use /bioclear.\n", BIO_LINES);
      return;
   }
   strncpy(users[u].bio[users[u].bio_lines], text, BIO_LINE_LEN - 1);
   users[u].bio[users[u].bio_lines][BIO_LINE_LEN - 1] = 0;
   /* keep the bio in plain ASCII */
   for(i = 0; users[u].bio[users[u].bio_lines][i]; i++)
   {
      if(!isprint((unsigned char)users[u].bio[users[u].bio_lines][i]))
      {
         users[u].bio[users[u].bio_lines][i] = '?';
      }
   }
   users[u].bio_lines++;
   user_save_profile(u);
   user_send(u, "Bio line %d/%d saved.\n", users[u].bio_lines, BIO_LINES);
}

static void cmd_bioclear(int u)
{
   users[u].bio_lines = 0;
   user_save_profile(u);
   user_send(u, "Bio erased.\n");
}

/* ---------- 8. private mode and friends ---------- */

static void cmd_private(int u, const char *arg)
{
   if(strcmp(arg, "on") == 0)
   {
      users[u].private_mode = 1;
   }
   else if(strcmp(arg, "off") == 0)
   {
      users[u].private_mode = 0;
   }
   else
   {
      user_send(u, "Private mode is %s. Usage: /private on|off\n", users[u].private_mode ? "on" : "off");
      return;
   }
   user_save_profile(u);
   user_send(u, "Private mode %s.\n", arg);
   if(users[u].game != -1)
   {
      game_check_observers(users[u].game);
   }
}

static void cmd_friend(int u, char *args)
{
   char *name = next_word(args);

   if(!valid_name(name))
   {
      user_send(u, "Usage: /friend add|del <user>\n");
   }
   else if(strcmp(args, "add") == 0)
   {
      if(user_add_friend(u, name))
      {
         user_save_profile(u);
         user_send(u, "%s is now your friend.\n", name);
      }
      else
      {
         user_send(u, "Cannot add %s (already a friend or list full).\n", name);
      }
   }
   else if(strcmp(args, "del") == 0)
   {
      if(user_del_friend(u, name))
      {
         user_save_profile(u);
         user_send(u, "%s removed from your friends.\n", name);
         if(users[u].game != -1)
         {
            game_check_observers(users[u].game);
         }
      }
      else
      {
         user_send(u, "%s is not your friend.\n", name);
      }
   }
   else
   {
      user_send(u, "Usage: /friend add|del <user>\n");
   }
}

static void cmd_friends(int u)
{
   int i;
   user_send(u, "Your friends (private mode %s):\n", users[u].private_mode ? "on" : "off");
   for(i = 0; i < users[u].nb_friends; i++)
   {
      user_send(u, "  %s%s\n", users[u].friends[i],
                user_find(users[u].friends[i]) != -1 ? " (online)" : "");
   }
   if(users[u].nb_friends == 0)
   {
      user_send(u, "  none\n");
   }
}

/* ---------- dispatcher ---------- */

void command_handle(int u, char *line)
{
   char *args;

   if(line[0] == 0)
   {
      return;
   }
   if(line[0] != '/')
   {
      cmd_chat(u, line);
      return;
   }

   args = next_word(line); /* line = command, args = the rest */

   if(strcmp(line, "/help") == 0)              user_send(u, "%s", HELP);
   else if(strcmp(line, "/list") == 0)         cmd_list(u);
   else if(strcmp(line, "/challenge") == 0)    cmd_challenge(u, args);
   else if(strcmp(line, "/accept") == 0)       cmd_accept(u, args);
   else if(strcmp(line, "/refuse") == 0)       cmd_refuse(u, args);
   else if(strcmp(line, "/move") == 0)         cmd_move(u, args);
   else if(strcmp(line, "/board") == 0)
   {
      int g = users[u].game != -1 ? users[u].game : users[u].watching;
      if(g == -1) user_send(u, "You are not in a game.\n");
      else game_send_board(g, u);
   }
   else if(strcmp(line, "/abandon") == 0)      cmd_abandon(u);
   else if(strcmp(line, "/games") == 0)        cmd_games(u);
   else if(strcmp(line, "/watch") == 0)        cmd_watch(u, args);
   else if(strcmp(line, "/unwatch") == 0)      cmd_unwatch(u);
   else if(strcmp(line, "/all") == 0)          cmd_all(u, args);
   else if(strcmp(line, "/msg") == 0)          cmd_msg(u, args);
   else if(strcmp(line, "/bio") == 0)          cmd_bio(u, args);
   else if(strcmp(line, "/bioadd") == 0)       cmd_bioadd(u, args);
   else if(strcmp(line, "/bioclear") == 0)     cmd_bioclear(u);
   else if(strcmp(line, "/private") == 0)      cmd_private(u, args);
   else if(strcmp(line, "/friend") == 0)       cmd_friend(u, args);
   else if(strcmp(line, "/friends") == 0)      cmd_friends(u);
   else if(strcmp(line, "/saved") == 0)        game_list_saved(u);
   else if(strcmp(line, "/replay") == 0)       game_replay(u, atoi(args));
   else user_send(u, "Unknown command %s, type /help.\n", line);
}

void command_disconnect(int u)
{
   char message[128];
   int i;

   if(users[u].logged_in)
   {
      /* leaving during a game = abandon */
      if(users[u].game != -1)
      {
         cmd_abandon(u);
      }
      if(users[u].watching != -1)
      {
         game_remove_observer(users[u].watching, u);
      }
      /* cancel the challenges sent to this user */
      for(i = 0; i < MAX_CLIENTS; i++)
      {
         if(users[i].in_use && strcmp(users[i].challenge_to, users[u].name) == 0)
         {
            users[i].challenge_to[0] = 0;
         }
      }
      snprintf(message, sizeof message, "* %s disconnected\n", users[u].name);
      send_to_all(u, message);
   }
   user_remove(u);
}
