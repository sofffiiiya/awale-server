# Awalé server

TP 2/3/4, 4IF Networks, INSA Lyon.

Client/server implementation of the Awalé game in C. Players connect to the server with a username, challenge each other and play. The server checks the legality of moves, keeps the score and sends the board to the players and to spectators. Players can also chat, write a bio and replay saved games.

## Compilation

Linux:

```
make
```

Windows (MSYS2 UCRT64, after `pacman -S mingw-w64-ucrt-x86_64-gcc make`):

```
make
```

Socket code specific to each system is kept in `net.c`.

## Usage

```
./server <port>
./client <server_ip> <port>
```

Client commands will be listed here
