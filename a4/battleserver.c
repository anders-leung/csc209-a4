
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


struct client {
    int has_name;
    int speaking;
    char message[512];
    char name[MAXNAME];
    int fd;
    struct in_addr ipaddr;
    struct client *next;
    struct client *previous;
    struct client *lastbattled;
    struct client *opponent;
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
        tv.tv_sec = 1;
        tv.tv_usec = 0;

        struct client *k;
        for (k = head; k; k = k->next) {
            if (k->previous == NULL) {
                head = k;
            }
        }

        for (k = head; k; k = k->previous) {
            if (k->previous == NULL) {
                head = k;
            }
        }

        if (head != NULL) {
            struct client *k;
            struct client *t = head;
            for (k = head; k; k = k->next) {
                match(k, t);
            }
        }
 
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
 
    int len = read(p->fd, buf, sizeof(buf) - 1);

    if (len > 0) {
        if (!p->has_name) {
            strncpy(&(*(p->name + strlen(p->name))), buf, 1);
            name(p, top);
            return 0;

        } else if (p->speaking) {
            strncpy(&(*(p->message + strlen(p->message))), buf, 1);
            speak(p, p->opponent);
            return 0;

        } else if (p->opponent != NULL) {

            if (p->turn) {
                if (buf[0] == 'a') {
                    attack(p, p->opponent, top);
                    return 0;

                } else if (buf[0] == 'p') {
                    powermove(p, p->opponent, top);
                    return 0;

                } else if (buf[0] == 's') {
                    write(p->fd, "\nSpeak: ", 8);
		    p->speaking = 1;
		    return 0;
		}
            }
        }

    } else if (len == 0) {
        printf("Disconnect from %s\n", inet_ntoa(p->ipaddr));
        sprintf(outbuf, "%s has left the arena\n", p->name);
        broadcast(top, p->fd, outbuf, strlen(outbuf));
        return -1;

    } else {
        perror("read");
        return -1;
    }

    return 0;
}


int name(struct client *p, struct client *top) {
    int end;
    if ((end = find_newline(p->name, MAXNAME)) >= 0) {
        if (end >= MAXNAME) {
            write(p->fd, "Name was too long\n", 18);
        }

        char message[25];
        p->name[end] = '\0';      
        sprintf(message, "Welcome %s! Awaiting opponent...\n", p->name);
        write(p->fd, message, strlen(message));
        sprintf(message, "*** %s enters the arena ***\n", p->name);
        broadcast(top, p->fd, message, strlen(message));
        p->has_name = 1;
        return 0;
    }

    return -1;
}
 
void match(struct client *p, struct client *top) {
    char message[25];
    struct client *t;
    for (t = top; t; t = t->next) {
        if ((t != p)
            && (p->has_name)
            && (t->has_name)
            && (p->opponent == NULL)
            && (t->opponent == NULL)
            && ((t->lastbattled != p) || (p->lastbattled != t))) {

            p->opponent = t;
            t->opponent = p;
            p->hp = rand() % (30 - 20) + 20;
            p->powermoves = rand() % (3 - 1) + 1;
            t->hp = rand() % (30 - 20) + 20;
            t->powermoves = rand() % (3 - 1) + 1;

            sprintf(message, "You engage %s!\n", t->name);
            write(p->fd, message, strlen(message));
            sprintf(message, "You engage %s!\n", p->name);
            write(t->fd, message, strlen(message));

            int first = rand() % 2;
            if (first) {
                p->turn = 1;
                t->turn = 0;
                status_message(p, t);

            } else {
                p->turn = 0;
                t->turn = 1;
                status_message(t, p);
            }
            
       }
    }
}


void status_message(struct client *a, struct client *b) {
    char message[25];
    sprintf(message, "\nYou have %d hitpoints and %d powermoves\n",
            a->hp, a->powermoves);
    write(a->fd, message, strlen(message));
    sprintf(message, "\nYou have %d hitpoints and %d powermoves\n",
            b->hp, b->powermoves);
    write(b->fd, message, strlen(message));

    sprintf(message, "%s has %d hitpoints\n", a->name, a->hp);
    write(b->fd, message, strlen(message));
    sprintf(message, "%s has %d hitpoints\n", b->name, b->hp);
    write(a->fd, message, strlen(message));

    write(a->fd, "\nIt's your turn!\n", 17);
    sprintf(message, "\nIt's %s' turn!\n", a->name);
    write(b->fd, message, strlen(message));

    write(a->fd, "\n(a)ttack\n(p)owermove\n(s)peak\n", 30);
    sprintf(message, "Waiting for %s to end turn\n", a->name);
    write(b->fd, message, strlen(message));

}


struct client *move_to_bottom(struct client *p, struct client *top) {
    if (p == top) {
        struct client *newtop = top->next;
        struct client *cur = newtop;

        while (cur->next) {
            cur = cur->next;
        }
        top->next = NULL;
        top->previous = cur;
        cur->next = top;

        top = newtop;
        top->previous = NULL;

    } else if (p->next == NULL) {
        return top;

    } else {
        struct client *t = top;
        while (t->next) {
            if (t->next == p) {
                t->next = t->next->next;
                t->next->previous = t;
            }
            t = t->next;
        }
        t->next = p;
        p->previous = t;
        p->next = NULL;
    }

    return top;
}


void lost_battle(struct client *a, struct client *b, struct client *top) {
    char message[25];
    a->lastbattled = b;
    b->lastbattled = a;
    a->opponent = NULL;
    b->opponent = NULL;
    write(a->fd, "You won!\n", 10);
    write(a->fd, "Waiting for opponent...\n", 24);

    sprintf(message, "You were no match for %s!\n", a->name);
    write(b->fd, message, strlen(message));
    write(b->fd, "Waiting for opponent...\n", 24);
    
    top = move_to_bottom(a, top);
    move_to_bottom(b, top);
}


void speak(struct client *a, struct client *b) {
    int end;
    if ((end = find_newline(a->message, sizeof(a->message))) >= 0) {
        if (end >= 512) {
            write(a->fd, "message was too long\n", 18);
        }
        char message[MAXNAME + 7];
        a->message[end] = '\0';
        write(a->fd, "You said: ", 10 );
        write(a->fd, a->message, strlen(a->message));
        write(a->fd, " \n\n", 2);
   
        sprintf(message, "%s said: ", a->name);
        write(b->fd, message, strlen(message));
        write(b->fd, a->message, strlen(a->message));
        write(b->fd, " \n\n", 2);

        a->speaking = 0;
        memset(a->message, 0, sizeof(a->message));
    }
   
}


void attack(struct client *a, struct client *b, struct client *top) {
    char message[25];
    int dmg = rand() % (6 - 2) + 2;
    b->hp -= dmg;
    a->turn = 0;
    b->turn = 1;
    sprintf(message, "\nYou hit %s for %d damage!\n", b->name, dmg);
    write(a->fd, message, strlen(message));
    sprintf(message, "%s hit you for %d damage!\n", a->name, dmg);
    write(b->fd, message, strlen(message));
    if (b->hp <= 0) {
        lost_battle(a, b, top);
    } else {
        status_message(b, a);
    }
}


void powermove(struct client *a, struct client *b, struct client *top) {
    char message[25];
    if (a->powermoves > 0) {
        int hit = rand() % 2;
        if (hit) {
            int dmg = (rand() % (6 - 2) + 2) * 3;
            b->hp -= dmg;
            a->powermoves -= 1;
            a->turn = 0;
            b->turn = 1;
            sprintf(message, "\nYou hit %s for %d damage!\n", b->name, dmg);
            write(a->fd, message, strlen(message));
            sprintf(message, "%s powermoves you for %d damage!\n", a->name, dmg);
            write(b->fd, message, strlen(message));

            if (b->hp <= 0) {
                lost_battle(a, b, top);
            } else {
                status_message(b, a);
            }

        } else {
            a->powermoves -= 1;
            write(a->fd, "You missed!\n", 12);
            sprintf(message, "%s missed you!\n", a->name);
            write(b->fd, message, strlen(message));
            a->turn = 0;
            b->turn = 1;

            if (b->hp <= 0) {
                lost_battle(a, b, top);
            } else {
                status_message(b, a);
            }
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
    p->has_name = 0;
    p->opponent = NULL;
    p->next = NULL;

    struct client *t;
    if (top == NULL) { 
        p->previous = NULL;
        top = p;

    } else {
        for (t = top; t->next; t = t->next);
        p->previous = t;
        t->next = p;
    }

    if ((p->next == NULL) && (p->previous == NULL)) {
        perror("addclient");
    }

    return top;
}


static struct client *removeclient(struct client *top, int fd) {
    struct client **p;

    for (p = &top; *p && (*p)->fd != fd; p = &(*p)->next);

    if (*p) {
        if ((*p)->has_name) {
            memset((*p)->name, 0, sizeof((*p)->name));
            memset((*p)->message, 0, sizeof((*p)->message));
            (*p)->next = NULL;
            (*p)->lastbattled = NULL;
        }

        if ((*p)->opponent != NULL) {
            write((*p)->opponent->fd, "\nYou won!\n", 10);
            (*p)->opponent->opponent = NULL;
            (*p)->opponent = NULL;
        }

        struct client *t = (*p)->next;
        t->previous = t->previous->previous;
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


int find_newline(char *buf, int inbuf) {
    int i;
    for (i = 0; i < inbuf; i++) {
        if (buf[i] == '\n') {
            return i;
        }
    }
    return -1;

