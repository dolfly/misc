
CC=gcc
FLAGS=-W -Wall -O2 `glib-config --cflags` `xmms-config --cflags`
LIBS=`xmms-config --libs`
PLUGINDIR=/home/shd/.xmms/Plugins/Output
OBJS=xmms-output.lo net.lo

libxmms-netaudio.la:	$(OBJS)
	libtool --mode=link $(CC) $(FLAGS) $(LIBS) $(OBJS) -o libxmms-netaudio.la -rpath $(PLUGINDIR) -module -avoid-version -pthread

xmms-output.lo:	xmms-output.c
	libtool --mode=compile $(CC) $(FLAGS) -c xmms-output.c

net.lo:	net.c net.h
	libtool --mode=compile $(CC) $(FLAGS) -c net.c

install:	libxmms-netaudio.la
	mkdir -p $(PLUGINDIR) || true
	install .libs/libxmms-netaudio.so $(PLUGINDIR)/

clean:	
	rm -f *.o *.lo *.la *.so
