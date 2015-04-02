#ifndef __BATTLESERVER_H__
#define __BATTLESERVER_H__

#include "battleserver.h"

/* Bind and listen, abort on error. Returns FD of listening socket */
int bindandlisten(void);

/* Add a client into the server */
static struct client *addclient(struct client *top, int fd, struct in_addr addr);

/* Remove a client from the server */
static struct client *removeclient(struct client *top, int fd);

/* Broadcast a message to all the clients except for the client with fd */
static void broadcast(struct client *top, int fd, char *s, int size);

/* Match client with another client that meets the requirements */
int handleclient(struct client *p, struct client *top);

void status_message(struct client *a, struct client *b);

int name(struct client *p, struct client *top);

int find_newline(char *buf, int inbuf);

void match(struct client *p, struct client *top);

void attack(struct client *a, struct client *b);

void powermove(struct client *a, struct client *b);

#endif
