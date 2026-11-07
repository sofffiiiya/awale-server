/*
 * RPC interface of the Awale server (compiled with rpcgen).
 *
 * The server can not call the clients, so every client has a "mailbox" on the server:
 * board updates (pushed after every move), chat messages, challenges... are queued in it,
 * and the client collects them with POLL every couple of seconds.
 */

const LINE_LEN_MAX = 1023;

/* any length ("string" alone is limited to 9000 bytes by libtirpc) */
typedef string text_t<>;

struct login_reply
{
   int token;      /* 0 = refused, otherwise to be sent with every next call */
   text_t text;    /* welcome or error message */
};

struct command_args
{
   int token;
   string line<LINE_LEN_MAX>;   /* one command typed by the user, e.g. "/move 3" */
};

program AWALE_PROG
{
   version AWALE_VERS
   {
      login_reply LOGIN(string) = 1;        /* username */
      text_t      COMMAND(command_args) = 2; /* returns the answer (mailbox content) */
      text_t      POLL(int) = 3;            /* returns and empties the mailbox */
      void        LOGOUT(int) = 4;
   } = 1;
} = 0x20000197;
