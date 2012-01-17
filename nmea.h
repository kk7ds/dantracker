/* Copyright 2012 Dan Smith <dsmith@danplanet.com> */

#ifndef __NMEA_H
#define __NMEA_H

struct posit {
	double lat;
	double lon;
	double alt;
	double course;
	double speed;
	int qual;
	int sats;
	time_t tstamp;
	int dstamp;
};

int valid_checksum(char *str);
int parse_gga(struct posit *mypos, char *str);
int parse_rmc(struct posit *mypos, char *str);

#endif
