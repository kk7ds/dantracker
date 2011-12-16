CFLAGS = -g -Wall
GTK_CFLAGS = `pkg-config --cflags 'gtk+-2.0'`
GTK_LIBS = `pkg-config --libs 'gtk+-2.0'`

DEST="root@beagle:carputer"

TARGETS = aprs ui uiclient fakegps

all: $(TARGETS)

uiclient.o: uiclient.c ui.h
serial.o: serial.c serial.h
nmea.o: nmea.c nmea.h
aprs-is.o: aprs-is.c aprs-is.h

aprs: aprs.c uiclient.o serial.o nmea.o aprs-is.o
	test -d .hg && hg id --id > .revision || true
	echo $$((`cat .build` + 1)) > .build
	$(CC) $(CFLAGS) $(APRS_CFLAGS) -o $@ $^ -DBUILD=`cat .build` -DREVISION=\"`cat .revision`\" -lfap -liniparser

ui: ui.c uiclient.o
	$(CC) $(CFLAGS) $(GTK_CFLAGS) $(GLIB_CFLAGS) $^ -o $@ $(GTK_LIBS) $(GLIB_LIBS)

uiclient: uiclient.c ui.h
	$(CC) $(CFLAGS) -DMAIN $< -o $@

fakegps: fakegps.c
	$(CC) $(CFLAGS) -lm -o $@ $< -lm

clean:
	rm -f $(TARGETS) *.o *~

sync:
	scp -r *.c *.h Makefile tools images .revision .build $(DEST)
