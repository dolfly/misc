
CC=gcc
CFLAGS= -W -Wall -O2
PFLAGS= $(CFLAGS) `glib-config --cflags` `xmms-config --cflags`
LIBS=`xmms-config --libs`
PLUGINDIR=/home/shd/.xmms/Plugins/Output
OBJS=xmms-output.lo net.lo ring_buf.lo

all:	libxmms-netaudio.la xmms-netaudio

libxmms-netaudio.la:	$(OBJS)
	libtool --mode=link $(CC) $(PFLAGS) $(LIBS) $(OBJS) -o libxmms-netaudio.la -rpath $(PLUGINDIR) -module -avoid-version -pthread

xmms-output.lo:	xmms-output.c
	libtool --mode=compile $(CC) $(PFLAGS) -c xmms-output.c

net.lo:	net.c net.h
	libtool --mode=compile $(CC) $(CFLAGS) -c net.c

ring_buf.lo:	ring_buf.c ring_buf.h
	libtool --mode=compile $(CC) $(CFLAGS) -c ring_buf.c


xmms-netaudio:	server.o net.o ring_buf.o event.o
	$(CC) $(CFLAGS) -o xmms-netaudio server.o net.o ring_buf.o event.o

server.o:	server.c meta.h
	$(CC) $(CFLAGS) -c server.c

net.o:	net.c net.h
	$(CC) $(CFLAGS) -c net.o

ring_buf.o:	ring_buf.c ring_buf.h
	$(CC) $(CFLAGS) -c ring_buf.c

event.o:	event.c event.h
	$(CC) $(CFLAGS) -c event.c

install:	libxmms-netaudio.la xmms-netaudio
	mkdir -p $(PLUGINDIR) || true
	install .libs/libxmms-netaudio.so $(PLUGINDIR)/

clean:	
	rm -f *.o *.lo *.la *.so xmms-netaudio
