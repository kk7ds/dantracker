/* Copyright 2012 Dan Smith <dsmith@danplanet.com> */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "nmea.h"

double parse_lat(char *str)
{
	int deg;
	float min;
	int ret;

	ret = sscanf(str, "%2i%f", &deg, &min);
	if (ret != 2)
		return 0;

	return deg + (min / 60.0);
}

double parse_lon(char *str)
{
	int deg;
	float min;
	int ret;

	ret = sscanf(str, "%3i%f", &deg, &min);
	if (ret != 2)
		return 0;

	return deg + (min / 60.0);
}

int valid_checksum(char *str)
{
	char *ptr = str;
	unsigned char c_cksum = 0;
	unsigned char r_cksum;

	if (str[0] != '$')
		return 0;

	str++; /* Past the $ */

	for (ptr = str; *ptr && (*ptr != '*'); ptr++)
		c_cksum ^= *ptr;

	sscanf(ptr, "*%02hhx", &r_cksum);

	return c_cksum == r_cksum;
}

int parse_gga(struct posit *mypos, char *str)
{
	int num = 0;
	char *field = strchr(str, ',');

	while (str && field) {
		*field = 0;

		switch (num) {
		case 1:
			mypos->tstamp = atoi(str);
			break;
		case 2:
			mypos->lat = parse_lat(str);
			break;
		case 3:
			if (*str == 'S')
				mypos->lat *= -1;
			break;
		case 4:
			mypos->lon = parse_lon(str);
			break;
		case 5:
			if (*str == 'W')
				mypos->lon *= -1;
			break;
		case 6:
			mypos->qual = atoi(str);
			break;
		case 7:
			mypos->sats = atoi(str);
			break;
		case 9:
			mypos->alt = atof(str);
			break;
		}

		num++;
		str = field + 1;
		field = strchr(str, ',');
	}

	return 1;
}

int parse_rmc(struct posit *mypos, char *str)
{
	int num = 0;
	char *field = strchr(str, ',');


	while (str && field) {
		*field = 0;

		switch (num) {
		case 2:
			if (*str != 'A') /* Not ACTIVE */
				return 1;
			break;
		case 7:
			mypos->speed = atof(str);
			break;
		case 8:
			mypos->course = atof(str);
			break;
		case 9:
			mypos->dstamp = atoi(str);
			break;
		};

		num++;
		str = field + 1;
		field = strchr(str, ',');
	}

	return 1;
}
