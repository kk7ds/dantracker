
APRS_ICON_SIZE=500%

IMAGES = aprs_pri_big.png aprs_sec_big.png
RESIZE_TYPE=-scale

all: $(IMAGES)

aprs_pri_big.png: aprs_pri.png
	convert $(RESIZE_TYPE) $(APRS_ICON_SIZE) $< $@

aprs_sec_big.png: aprs_sec.png
	convert $(RESIZE_TYPE) $(APRS_ICON_SIZE) $< $@

clean:
	rm -f $(IMAGES)
