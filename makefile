CC = gcc
PORT=29467
CFLAGS = -DPORT=\$(PORT) -g -Wall

all: battleserver

battleserver: battleserver.o
	${CC} ${CFLAGS} -o $@ battleserver.o

%.o: %.c
	${CC} ${CFLAGS}  -c $<

clean:
	rm *.o battleserver
