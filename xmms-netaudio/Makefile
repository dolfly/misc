
CC=gcc
FLAGS=-W -Wall -O2
LIBS=
OBJS=main.o net.o

xmms-output.o:	xmms-output.c
	$(CC) $(FLAGS) -c xmms-output.c

net.o:	net.c net.h
	$(CC) $(FLAGS) -c net.c

clean:	
	rm -f netdd *.o
