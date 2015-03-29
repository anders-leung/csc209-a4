#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#ifndef PORT
    #define PORT 56092
#endif

#define MAXNAME 25


struct client {
    char name[MAXNAME];
    int fd;
    struct in_addr ipaddr;
    struct client *next;
    struct client *lastbattled;
    int inmatch;
    int hp;
    int powermoves;
};


int main(void) {
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
    struct client *t;
    for (t = top; t; t = t->next) {
        if ((!t->inmatch) && (t->lastbattled != p)) {
            match(p, t);
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

static struct client *addclient(struct client *top,  int fd) {
    struct client *p = malloc(sizeof(struct client));
    if (!p) {
        perror("malloc");
        exit(1);
    }

    sprintf(fd, "What is your name?");
    readmessage(p->name, fd, MAXNAME);


    char outbuf[MAXNAME + 30];
    sprintf(outbuf, "%s has entered the arena!", name);
    broadcast(top, fd, outbuf, MAXNAME + 30);

    p->name = name;
    p->fd = fd;

    struct client *t;
    for (t = top; t; t = t->next);
    if (t->next == NULL) {
        t->next = p;
    } else {
        perror("Could not add %s\n", name);

    return top;
}


static struct client *removeclient(struct client *top, int fd) {
    struct client **p;

    for (p = &top; *p && (*p)->fd != fd; p = &(*p)->next);

    if (*p) {
        char outbuf[MAXNAME + 30];
        sprintf(outbuf, "%s has left the arena!", (*p)->name);
        struct client *t = (*p)->next;
        broadcast(top, fd, outbuf, MAXNAME + 30);
        free(*p);
        *p = t;

    } else {
        fprintf(stderr, "Could not remove fd %d\n", fd);
    }

    return top;
}


static void broadcast(struct client *top, int fd, char *s, int size) {
    struct client *p;
    for (p = top; p; p = p->next) {
        if (p->fd != fd) {
            if (write(p->fd, s, size) == -1) {
                perror("write");
                exit(1);
            }
        }
    }
}


int match(struct client *a, struct client *b) {
    srand(time(NULL));
    a->hp = rand() % (30 - 20) + 20;
    b->hp = rand() % (30 - 20) + 20;
    a->powermoves = rand() % (3 - 1) + 1;
    b->powermove = rand() % (3 - 1) + 1;
    int first = rand() % (2 - 1) + 1;
    sprintf(a->fd, "You have been matched with %s\n", b->name);
    sprintf(b->fd, "You have been matched with %s\n", a->name);
    if (first == 1) {
        sprintf(a->fd, "You have the first strike!\nYou have %d hitpoints and
                %d powermoves\n", ahp, apowermove);
        sprintf(b->fd, "%s has the first strike!\nYou have %d hitpoints and
                %d powermoves\n", a->name, bhp, bpowermove);
        while ((a->hp > 0) || (b->hp > 0)) {
            battlemenu(a, b);
        }
            
            
            
    } else {
        sprintf(a->fd, "%s has the first strike!\nYou have %d hitpoints and
                %d powermoves\n", b->name, ahp, apowermove);
        sprintf(b->fd, "You have the first strike!\nYou have %d hitpoints and
                %d powermoves\n", bhp, bpowermove);
    }


int readmessage(dest, source, size) {
    int nbytes;
    int end;
    int inbuf;
    char buf[216];
    while ((nbytes = read(buf, source, size)) >= 0) {
        inbuf += nbytes;
        if ((end = find_network_newline(buf, inbuf)) >= 0) {
            buf[end] = '\0';
            write(dest, buf, strlen(buf) + 1);
        }
    }
}


int find_network_newline(char *buf, int inbuf) {
    int i;
    for (i = 0; i < inbuf; i++) {
        if (buf[i] == '\r') {
            return i;
        }
    }
    return -1;
}


int battle(struct client *a, struct client *b) {
    char buf[5];
    sprintf(a->fd, "You can press:\n(a)ttack\n(p)owermove\n(s)peak\n");
    sprintf(b->fd, "You can press:\n(a)ttack\n(p)owermove\n(s)peak\n");
    int nbytes;
    while ((nbytes = read(buf, a->fd, 1) >= 0)) {
        if (buf[0] == 'a') {
            b->hp -= rand() % (6 - 2) + 2;
        else if (buf[0] == 'p') {
            int attack = rand() % (6 - 2) + 2;
            attack = 3 * attack;
            if ((rand() % 1) == 0) {
                /*
            
}











