
CC=gcc
CFLAGS= -W -Wall -O2
PFLAGS= $(CFLAGS) `glib-config --cflags` `xmms-config --cflags`
LIBS=`xmms-config --libs`
PLUGINDIR=/home/shd/.xmms/Plugins/Output
OBJS=xmms-output.lo net.lo

all:	libxmms-netaudio.la xmms-netaudio

libxmms-netaudio.la:	$(OBJS)
	libtool --mode=link $(CC) $(FLAGS) $(LIBS) $(OBJS) -o libxmms-netaudio.la -rpath $(PLUGINDIR) -module -avoid-version -pthread

xmms-output.lo:	xmms-output.c
	libtool --mode=compile $(CC) $(FLAGS) -c xmms-output.c

net.lo:	net.c net.h
	libtool --mode=compile $(CC) $(FLAGS) -c net.c

xmms-netaudio:	server.o net.o
	$(CC) $(CFLAGS) -o xmms-netaudio server.o net.o

server.o:	server.c meta.h
	$(CC) $(CFLAGS) -c server.c

net.o:	net.c net.h
	$(CC) $(CFLAGS) -c net.o

install:	libxmms-netaudio.la
	mkdir -p $(PLUGINDIR) || true
	install .libs/libxmms-netaudio.so $(PLUGINDIR)/

clean:	
	rm -f *.o *.lo *.la *.so
