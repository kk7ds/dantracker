CFLAGS = -g -Wall -Ilibfap-1.1/src -Iiniparser/src
GTK_CFLAGS = `pkg-config --cflags 'gtk+-2.0'`
GTK_LIBS = `pkg-config --libs 'gtk+-2.0'`

DEST="root@beagle:carputer"

TARGETS = aprs ui uiclient fakegps

all: $(TARGETS)

uiclient.o: uiclient.c ui.h
serial.o: serial.c serial.h
nmea.o: nmea.c nmea.h

aprs: aprs.c uiclient.o serial.o nmea.o
	test -d .hg && hg id --id > .revision || true
	echo $$((`cat .build` + 1)) > .build
	$(CC) $(CFLAGS) $(APRS_CFLAGS) -lfap -liniparser -o $@ $^ -DBUILD=`cat .build` -DREVISION=\"`cat .revision`\"

ui: ui.c uiclient.o
	$(CC) $(CFLAGS) $(GTK_CFLAGS) $(GTK_LIBS) $(GLIB_CFLAGS) $(GLIB_LIBS) $^ -o $@

uiclient: uiclient.c ui.h
	$(CC) $(CFLAGS) -DMAIN $< -o $@

fakegps: fakegps.c
	$(CC) $(CFLAGS) -lm -o $@ $<

clean:
	rm -f $(TARGETS) *.o *~

sync:
	scp -r *.c *.h Makefile tools images .revision .build $(DEST)
