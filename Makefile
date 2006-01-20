CC = gcc
CFLAGS = -Wall -O2 -g
LDFLAGS = 

poissonrun:	poissonrun.c
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $<


