# Awalé kliens-szerver – Sun RPC változat (rpcgen)

## Telepítés és fordítás (Ubuntu)

```
sudo apt install rpcbind libtirpc-dev     # az rpcgen az rpcsvc-proto csomagban van
make
```

Az `rpcgen` a `make` során generálja az `awale_rpc.x`-ből ezeket a fájlokat: `awale_rpc.h`, `awale_rpc_clnt.c`,
`awale_rpc_svc.c` (ebben van a szerver `main()`-je) és `awale_rpc_xdr.c`.

## Futtatás

```
./server                      # kell hozzá a futó rpcbind (sudo systemctl start rpcbind)
./client localhost alice      # másik terminálban
./client localhost bob        # harmadik terminálban
```

Ha a szerver azt írja, hogy `unable to register`, akkor nem fut az rpcbind, vagy `sudo`-val kell indítani.

## Hogyan működik

Az RPC kérés–válasz alapú, ezért a szerver magától nem tud a kliensnek üzenni. Emiatt minden
felhasználónak van egy „postafiókja” a szerveren:

- **Lépés után** a szerver az új táblát betolja (push) mindkét játékos és minden néző postafiókjába.
- Aki lépett, az a `COMMAND` válaszában azonnal megkapja.
- **A kliens 2 másodpercenként automatikusan meghívja a `POLL` eljárást**, ami visszaadja és kiüríti
  a postafiókot. Így jut el a kliensekhez az ellenfél lépése utáni tábla, a chat, a kihívás stb.
- Ha egy kliens 30 másodpercig nem jelentkezik, a szerver kiléptetettnek veszi (játék közben feladásnak).

RPC eljárások (`awale_rpc.x`):

| Eljárás | Mit csinál |
|---|---|
| `LOGIN(név)` | bejelentkezés, egy tokent ad vissza |
| `COMMAND(token, sor)` | egy parancs végrehajtása, a válasz szövege |
| `POLL(token)` | a postafiók tartalma (2 mp-enként) |
| `LOGOUT(token)` | kilépés |

## Fájlok

| Fájl | Tartalom |
|---|---|
| `awale_rpc.x` | az RPC interfész |
| `awale_server.c` | az RPC eljárások szerveroldali megvalósítása |
| `awale_client.c` | kliens: beolvassa a parancsokat, 2 mp-enként lekérdez |
| `awale.c/.h` | a játékszabályok és az ASCII tábla (változatlan) |
| `game.c/.h` | meccsek, nézők, privát mód, mentés és visszajátszás (változatlan) |
| `commands.c/.h` | a parancsok feldolgozása (változatlan) |
| `user.c/.h` | felhasználók; a socket helyett postafiók |
| `local.c` | hálózat nélküli játék két játékosnak |

A parancsok ugyanazok, mint a socketes változatban (`/help`): `/list`, `/challenge`, `/accept`, `/refuse`,
`/move <1-6>`, `/board`, `/abandon`, `/games`, `/watch`, `/unwatch`, `/all`, `/msg`, `/bio`, `/bioadd`,
`/bioclear`, `/private`, `/friend`, `/friends`, `/saved`, `/replay`, `/quit`.
