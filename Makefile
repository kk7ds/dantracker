CFLAGS += -g
APRS_CFLAGS = -Ilibfap-1.1/src
APRS_CFLAGS += -Iiniparser/src
GTK_CFLAGS = `pkg-config --cflags 'gtk+-2.0'`
GTK_LIBS = `pkg-config --libs 'gtk+-2.0'`

#GTK_CFLAGS = `pkg-config --cflags 'gtk+-2.0' | sed 's@-I/@-I$(ROOT)/@g'`
#GTK_LIBS = -L$(ROOT)/usr/lib -L$(ROOT)/lib `pkg-config --libs 'gtk+-2.0' | sed "s@-I/@-I$(ROOT)/@g"` usr-arm/lib/libc.so.6

DEST="root@beagle:carputer"

all: aprs ui uiclient

uiclient.o: uiclient.c ui.h

aprs: aprs.c uiclient.o
	echo $$((`cat .build` + 1)) > .build
	test -d .hg && hg id --id > .revision || true
	$(CC) $(CFLAGS) $(APRS_CFLAGS) -lfap -liniparser -o $@ $^ -DBUILD=`cat .build` -DREVISION=\"`cat .revision`\"

ui: ui.c uiclient.o
	$(CC) $(CFLAGS) $(GTK_CFLAGS) $(GTK_LIBS) $(GLIB_CFLAGS) $(GLIB_LIBS) $< -o $@

uiclient: uiclient.c ui.h
	$(CC) $(CFLAGS) -DMAIN $< -o $@

clean:
	rm -f aprs ui uiclient *.o *~

sync:
	scp -r *.c *.h Makefile tools images .revision .build $(DEST)
