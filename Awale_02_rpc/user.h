#ifndef USER_H
#define USER_H

#include <time.h>
#include <stddef.h>

#define MAX_CLIENTS  100
#define BUF_SIZE     1024
#define NAME_LEN     16
#define BIO_LINES    10
#define BIO_LINE_LEN 80
#define MAX_FRIENDS  20
#define PROFILE_DIR  "profiles"
#define USER_TIMEOUT 30          /* seconds without POLL = disconnected */

/* one logged in client */
typedef struct
{
   int in_use;       /* 1 if this slot is used */
   int token;        /* secret number given at LOGIN, identifies the client in each RPC */
   time_t last_seen; /* time of the last RPC call of this client */
   int logged_in;    /* 1 once the username is accepted */
   char name[NAME_LEN];

   /* messages waiting to be collected by the client (POLL or COMMAND) */
   char *mailbox;
   size_t mailbox_len;

   char bio[BIO_LINES][BIO_LINE_LEN];
   int bio_lines;

   char friends[MAX_FRIENDS][NAME_LEN];
   int nb_friends;
   int private_mode; /* 1: only friends may watch my games */

   char challenge_to[NAME_LEN]; /* pending challenge sent to this user, "" if none */
   int game;                    /* index of the game I play, -1 if none */
   int watching;                /* index of the game I watch, -1 if none */
} User;

extern User users[MAX_CLIENTS];

/* returns the index of a new user slot, -1 if the server is full */
int user_add(void);
void user_remove(int u);

/* index of the logged in user with this name, -1 if not found */
int user_find(const char *name);

/* index of the user with this token, -1 if not found */
int user_find_token(int token);

/* printf-like: adds a message in the mailbox of user u */
void user_send(int u, const char *fmt, ...);

/* returns the mailbox content (to be freed by the caller) and empties it */
char *user_take_mailbox(int u);

int user_is_friend(int u, const char *name);
int user_add_friend(int u, const char *name);
int user_del_friend(int u, const char *name);

/* bio and friends are kept in PROFILE_DIR/<name>.txt between connections */
void user_load_profile(int u);
void user_save_profile(int u);

#endif /* guard */
