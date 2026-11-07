/*
 * Awale RPC client.
 * Usage: ./client <server host> <username>
 *
 * Every line typed is sent to the server with the COMMAND procedure, and the answer is printed.
 * Every POLL_SECONDS seconds the client calls POLL by itself to get what happened meanwhile:
 * the new board after each move of the opponent, chat messages, challenges, ...
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <sys/select.h>

#include "awale_rpc.h"

#define POLL_SECONDS 2

static CLIENT *clnt;
static int token;

/* prints a text returned by the server and frees it */
static void print_and_free(char **text)
{
   if(text == NULL)
   {
      clnt_perror(clnt, "RPC call failed");
      exit(EXIT_FAILURE);
   }
   fputs(*text, stdout);
   fflush(stdout);
   xdr_free((xdrproc_t) xdr_text_t, (char *) text);
}

static void do_poll(void)
{
   print_and_free(poll_1(&token, clnt));
}

/* sends one typed line. Returns 0 when the user wants to quit */
static int do_command(char *line)
{
   command_args args;

   if(strcmp(line, "/quit") == 0)
   {
      return 0;
   }
   args.token = token;
   args.line = line;
   print_and_free(command_1(&args, clnt));
   return 1;
}

static void login(char *name)
{
   login_reply *reply = login_1(&name, clnt);
   if(reply == NULL)
   {
      clnt_perror(clnt, "LOGIN failed");
      exit(EXIT_FAILURE);
   }
   fputs(reply->text, stdout);
   token = reply->token;
   xdr_free((xdrproc_t) xdr_login_reply, (char *) reply);
   if(token == 0)
   {
      exit(EXIT_FAILURE); /* name refused */
   }
}

int main(int argc, char **argv)
{
   char input[1024];
   int input_len = 0;
   int running = 1;
   time_t next_poll;

   if(argc < 3)
   {
      printf("Usage: %s <server host> <username>\n", argv[0]);
      return EXIT_FAILURE;
   }

   clnt = clnt_create(argv[1], AWALE_PROG, AWALE_VERS, "tcp");
   if(clnt == NULL)
   {
      clnt_pcreateerror(argv[1]);
      return EXIT_FAILURE;
   }

   login(argv[2]);
   next_poll = time(NULL) + POLL_SECONDS;

   while(running)
   {
      fd_set rdfs;
      struct timeval timeout;
      time_t now = time(NULL);

      /* wait for the keyboard, but at most until the next automatic poll */
      timeout.tv_sec = next_poll > now ? next_poll - now : 0;
      timeout.tv_usec = 0;
      FD_ZERO(&rdfs);
      FD_SET(STDIN_FILENO, &rdfs);

      if(select(STDIN_FILENO + 1, &rdfs, NULL, NULL, &timeout) == -1)
      {
         perror("select()");
         break;
      }

      if(FD_ISSET(STDIN_FILENO, &rdfs))
      {
         char *start, *end;
         int n = read(STDIN_FILENO, input + input_len, sizeof input - 1 - input_len);
         if(n <= 0)
         {
            break; /* Ctrl-D */
         }
         input_len += n;
         input[input_len] = 0;

         /* send every complete line */
         start = input;
         while(running && (end = strchr(start, '\n')) != NULL)
         {
            *end = 0;
            running = do_command(start);
            start = end + 1;
         }
         input_len = strlen(start);
         memmove(input, start, input_len + 1);
         if(input_len >= (int)sizeof input - 1)
         {
            input_len = 0; /* line too long: dropped */
         }
      }

      if(running && time(NULL) >= next_poll)
      {
         do_poll();
         next_poll = time(NULL) + POLL_SECONDS;
      }
   }

   logout_1(&token, clnt);
   printf("Bye!\n");
   clnt_destroy(clnt);
   return EXIT_SUCCESS;
}
