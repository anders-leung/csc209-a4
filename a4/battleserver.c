#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <time.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "battleserver.h"

#ifndef PORT
    #define PORT 56092
#endif

#define MAXNAME 25

#include "battleserver.h"

struct client {
    int has_name;
    char name[MAXNAME];
    int fd;
    struct in_addr ipaddr;
    struct client *next;
    struct client *lastbattled;
    struct client * opponent;
    int turn;
    int hp;
    int powermoves;
};

int main(void) {
    srand(time(NULL));
    int clientfd, maxfd, nready;
    struct client *p;
    struct client *head = NULL;
    socklen_t len;
    struct sockaddr_in q;
    struct timeval tv;
    fd_set allset;
    fd_set rset;

    int i;
    
    int listenfd = bindandlisten();
    FD_ZERO(&allset);
    FD_SET(listenfd, &allset);
    maxfd = listenfd;

    while(1) {
        rset = allset;
        tv.tv_sec = 10;
        tv.tv_usec = 0;

        nready = select(maxfd + 1, &rset, NULL, NULL, &tv);
        if (nready == 0) {
            printf("No response from clients in %ld seconds\n", tv.tv_sec);
            continue;
        }

        if (nready == -1) {
            perror("select");
            continue;
        }

        if (FD_ISSET(listenfd, &rset)) {
            printf("a new client is connecting\n");
            len = sizeof(q);
            if ((clientfd = accept(listenfd, (struct sockaddr *)&q, &len)) < 0) {
                perror("accept");
                exit(1);
            }
            FD_SET(clientfd, &allset);
            if (clientfd > maxfd) {
                maxfd = clientfd;
            }
            printf("connection from %s\n", inet_ntoa(q.sin_addr));
            head = addclient(head, clientfd, q.sin_addr);
        }

        for (i = 0; i <= maxfd; i++) {
            if (FD_ISSET(i, &rset)) {
                for (p = head; p != NULL; p = p->next) {
                    if (p->fd == i) {
                        int result = handleclient(p, head);
                        if (result == -1) {
                            int tmp_fd = p->fd;
                            head = removeclient(head, p->fd);
                            FD_CLR(tmp_fd, &allset);
                            close(tmp_fd);
                        }
                        break;
                    }
                }
            }
        }
    }
    return 0;
}


int handleclient(struct client *p, struct client *top) {
    char buf[256];
    char outbuf[512];
    char message[25];
    int len = read(p->fd, buf, sizeof(buf) - 1);

    if (len > 0) {
        buf[len] = '\0';
        if (!p->has_name) {
            strncpy(p->name, buf, strlen(buf));
            sprintf(message, "Welcome %s! Awaiting opponent...\n", p->name);
            write(p->fd, message, strlen(message));
            sprintf(message, "*** %s enters the arena **\n", p->name);
            broadcast(top, p->fd, message);
            return 0;

        } else if (p->opponent == NULL) {
            match(p, top);
            return 0;

        } else if (p->opponent) {
            if (p->turn) {
                if (buf[0] == 'a') {
                    attack(p, p->opponent);
                    return 0;

                } else if (buf[0] == 'p') {
                    powermove(p, p->opponent);
                    return 0;
                }
            }
        }

    } else if (len == 0) {
        printf("Disconnect from %s\n", inet_ntoa(p->ipaddr));
        sprintf(outbuf, "%s left the arena\n", p->name);
        broadcast(top, p->fd, outbuf);
        return -1;

    } else {
        perror("read");
        return -1;
    }
}


void match(struct client *p, struct client *top) {
    char message[25];
    struct client *t;
    for (t = top; t; t = t->next) {
        if ((t != p)
            && (t->opponent == NULL)
            && (t->lastbattled != p)
            && (p->lastbattled != t)) {
            p->opponent = t;
            t->opponent = p;
            p->hp = rand() % (30 - 20) + 20;
            p->powermoves = rand() % (3 - 1) + 1;
            t->hp = rand() % (30 - 20) + 20;
            t->powermoves = rand() % (3 - 1) + 1;
            int first = rand() % 2;
            if (first) {
                p->turn = 1;
                t->turn = 0;
            } else {
                p->turn = 0;
                t->turn = 1;
            }
            sprintf(message, "You engage %s!\n", t->name);
            write(p->fd, message, strlen(message));
            sprintf(message, "You engage %s!\n", p->name);
            write(t->fd, message, strlen(message));
        }
    }
}
 

void attack(struct client *a, struct client *b) {
    char message[25];
    int dmg = rand() % (6 - 2) + 2;
    b->hp -= dmg;
    a->turn = 0;
    b->turn = 1;
    sprintf(message, "You hit %s for %d damage!\n", b->name, dmg);
    write(a->fd, message, strlen(message));
    sprintf(message, "%s hit you for %d damage!\n", a->name, dmg);
    write(b->fd, message, strlen(message));
}


void powermove(struct client *a, struct client *b) {
    char message[25];
    if (a->powermoves > 0) {
        int hit = rand() % 2;
        if (hit) {
            int dmg = (rand() % (6 - 2) + 2) * 3;
            b->hp -= dmg;
            a->powermoves -= 1;
            a->turn = 0;
            b->turn = 1;
            sprintf(message, "You hit %s for %d damage!\n", b->name, dmg);
            write(a->fd, message, strlen(message));
            sprintf(message, "%s powermoves you for %d damage!\n", a->name, dmg);
            write(b->fd, message, strlen(message));
        } else {
            write(a->fd, "You missed!\n", 12);
            sprintf(message, "%s missed you!\n", a->name);
            write(b->fd, message, strlen(message));
            a->turn = 0;
            b->turn = 1;
        }
    }
}

int bindandlisten(void) {
    struct sockaddr_in r;
    int listenfd;

    if ((listenfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("socket");
        exit(1);
    }

    int yes = 1;
    if ((setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int))) == -1) {
        perror("setsockopt");
    }

    memset(&r, '\0', sizeof(r));
    r.sin_family = AF_INET;
    r.sin_addr.s_addr = INADDR_ANY;
    r.sin_port = htons(PORT);

    if (bind(listenfd, (struct sockaddr *)&r, sizeof r)) {
        perror("bind");
        exit(1);
    }

    if (listen(listenfd, 5)) {
        perror("listen");
        exit(1);
    }

    return listenfd;
}

static struct client *addclient(struct client *top, int fd, struct in_addr addr) {
    struct client *p = malloc(sizeof(struct client));
    if (!p) {
        perror("malloc");
        exit(1);
    }

    write(fd, "What is your name? ", 20);
    
    p->fd = fd;
    p->ipaddr = addr;
    p->next = NULL;
    struct client *t;
    if (top == NULL) {
        top = p;
    } else {
        for (t = top; t; t = t->next) {
            if (t->next == NULL) {
                break;
            }
        }
        if (t->next == NULL) {
            t->next = p;
        } else {
            perror("addclient");
        }
    }
    return top;
}


static struct client *removeclient(struct client *top, int fd) {
    struct client **p;

    for (p = &top; *p && (*p)->fd != fd; p = &(*p)->next);

    if (*p) {
        char outbuf[MAXNAME + 30];
        sprintf(outbuf, "%s has left the arena!", (*p)->name);
        struct client *t = (*p)->next;
        broadcast(top, fd, outbuf);
        free(*p);
        *p = t;

    } else {
        fprintf(stderr, "Could not remove fd %d\n", fd);
    }

    return top;
}


static void broadcast(struct client *top, int fd, char *s) {
    struct client *p;
    for (p = top; p; p = p->next) {
        if (p->fd != fd) {
            if (write(p->fd, s, strlen(s) + 1) == -1) {
                perror("write");
                exit(1);
            }
        }
    }
}

   
