
APRS_ICON_SIZE=400%

IMAGES = aprs_pri_big.png aprs_sec_big.png

all: $(IMAGES)

aprs_pri_big.png: aprs_pri.png
	convert -resize $(APRS_ICON_SIZE) $< $@

aprs_sec_big.png: aprs_sec.png
	convert -resize $(APRS_ICON_SIZE) $< $@

clean:
	rm -f $(IMAGES)
