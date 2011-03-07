CFLAGS += -Ilibfap-1.1/src -Llibfap-1.1/src/.libs -g
GTK_CFLAGS = `pkg-config --cflags 'gtk+-2.0'`
GTK_LIBS = `pkg-config --libs 'gtk+-2.0'`

#GTK_CFLAGS = `pkg-config --cflags 'gtk+-2.0' | sed 's@-I/@-I$(ROOT)/@g'`
#GTK_LIBS = -L$(ROOT)/usr/lib -L$(ROOT)/lib `pkg-config --libs 'gtk+-2.0' | sed "s@-I/@-I$(ROOT)/@g"` usr-arm/lib/libc.so.6

all: aprs ui uiclient

uiclient.o: uiclient.c ui.h

aprs: aprs.c uiclient.o
	$(CC) $(CFLAGS) -lfap -o $@ $^

ui: ui.c uiclient.o
	$(CC) $(CFLAGS) $(GTK_CFLAGS) $(GTK_LIBS) $(GLIB_CFLAGS) $(GLIB_LIBS) $< -o $@

uiclient: uiclient.c ui.h
	$(CC) $(CFLAGS) -DMAIN $< -o $@

clean:
	rm -f aprs ui uiclient *.o *~
