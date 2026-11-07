#ifndef COMMANDS_H
#define COMMANDS_H

/* first line sent by a client: its username. Returns 0 if refused (the client is then removed) */
int command_login(int u, char *line);

/* any other line sent by a logged in client */
void command_handle(int u, char *line);

/* the client closed the connection (or typed /quit) */
void command_disconnect(int u);

#endif /* guard */
