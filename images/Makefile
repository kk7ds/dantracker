
APRS_ICON_SIZE=300%

all: aprs_pri_big.png aprs_sec_big.png

aprs_pri_big.png: aprs_pri.png
	convert -adaptive-resize 300% $< $@

aprs_sec_big.png: aprs_sec.png
	convert -adaptive-resize 300% $< $@
