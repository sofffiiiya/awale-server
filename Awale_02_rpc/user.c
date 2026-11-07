#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <time.h>

#include "user.h"


User users[MAX_CLIENTS];

#define MAILBOX_MAX (512 * 1024)  /* a client that never polls can not eat all the memory */

int user_add(void)
{
   int i;
   for(i = 0; i < MAX_CLIENTS; i++)
   {
      if(!users[i].in_use)
      {
         memset(&users[i], 0, sizeof(User));
         users[i].in_use = 1;
         /* random token, unique among connected users */
         do
         {
            users[i].token = 1 + rand() % 1000000000;
         } while(user_find_token(users[i].token) != i);
         users[i].last_seen = time(NULL);
         users[i].game = -1;
         users[i].watching = -1;
         return i;
      }
   }
   return -1;
}

void user_remove(int u)
{
   free(users[u].mailbox);
   users[u].mailbox = NULL;
   users[u].in_use = 0;
}

int user_find(const char *name)
{
   int i;
   for(i = 0; i < MAX_CLIENTS; i++)
   {
      if(users[i].in_use && users[i].logged_in && strcmp(users[i].name, name) == 0)
      {
         return i;
      }
   }
   return -1;
}

int user_find_token(int token)
{
   int i;
   for(i = 0; i < MAX_CLIENTS; i++)
   {
      if(users[i].in_use && users[i].token == token)
      {
         return i;
      }
   }
   return -1;
}

void user_send(int u, const char *fmt, ...)
{
   char message[8192];
   size_t len;
   char *bigger;
   va_list args;

   if(u < 0 || !users[u].in_use)
   {
      return;
   }

   va_start(args, fmt);
   vsnprintf(message, sizeof message, fmt, args);
   va_end(args);

   len = strlen(message);
   if(users[u].mailbox_len + len > MAILBOX_MAX)
   {
      return; /* mailbox full: message lost */
   }
   bigger = realloc(users[u].mailbox, users[u].mailbox_len + len + 1);
   if(bigger == NULL)
   {
      return;
   }
   users[u].mailbox = bigger;
   memcpy(users[u].mailbox + users[u].mailbox_len, message, len + 1);
   users[u].mailbox_len += len;
}

char *user_take_mailbox(int u)
{
   char *text = users[u].mailbox;
   if(text == NULL)
   {
      text = strdup("");
   }
   users[u].mailbox = NULL;
   users[u].mailbox_len = 0;
   return text;
}

int user_is_friend(int u, const char *name)
{
   int i;
   for(i = 0; i < users[u].nb_friends; i++)
   {
      if(strcmp(users[u].friends[i], name) == 0)
      {
         return 1;
      }
   }
   return 0;
}

int user_add_friend(int u, const char *name)
{
   if(user_is_friend(u, name) || users[u].nb_friends >= MAX_FRIENDS)
   {
      return 0;
   }
   strncpy(users[u].friends[users[u].nb_friends], name, NAME_LEN - 1);
   users[u].friends[users[u].nb_friends][NAME_LEN - 1] = 0;
   users[u].nb_friends++;
   return 1;
}

int user_del_friend(int u, const char *name)
{
   int i;
   for(i = 0; i < users[u].nb_friends; i++)
   {
      if(strcmp(users[u].friends[i], name) == 0)
      {
         /* replace it by the last one */
         users[u].nb_friends--;
         strcpy(users[u].friends[i], users[u].friends[users[u].nb_friends]);
         return 1;
      }
   }
   return 0;
}

static void profile_path(int u, char *path, size_t size)
{
   snprintf(path, size, "%s/%s.txt", PROFILE_DIR, users[u].name);
}

/*
 * Profile file format:
 *   private 0
 *   friend bob
 *   bio Hello, I am alice
 */
void user_load_profile(int u)
{
   char path[64];
   char line[BUF_SIZE];
   FILE *f;

   profile_path(u, path, sizeof path);
   f = fopen(path, "r");
   if(f == NULL)
   {
      return; /* new user */
   }

   while(fgets(line, sizeof line, f) != NULL)
   {
      line[strcspn(line, "\n")] = 0;
      if(strncmp(line, "private ", 8) == 0)
      {
         users[u].private_mode = atoi(line + 8);
      }
      else if(strncmp(line, "friend ", 7) == 0)
      {
         user_add_friend(u, line + 7);
      }
      else if(strncmp(line, "bio ", 4) == 0 && users[u].bio_lines < BIO_LINES)
      {
         strncpy(users[u].bio[users[u].bio_lines], line + 4, BIO_LINE_LEN - 1);
         users[u].bio_lines++;
      }
   }
   fclose(f);
}

void user_save_profile(int u)
{
   char path[64];
   FILE *f;
   int i;

   profile_path(u, path, sizeof path);
   f = fopen(path, "w");
   if(f == NULL)
   {
      perror("fopen()");
      return;
   }

   fprintf(f, "private %d\n", users[u].private_mode);
   for(i = 0; i < users[u].nb_friends; i++)
   {
      fprintf(f, "friend %s\n", users[u].friends[i]);
   }
   for(i = 0; i < users[u].bio_lines; i++)
   {
      fprintf(f, "bio %s\n", users[u].bio[i]);
   }
   fclose(f);
}
