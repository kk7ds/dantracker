#ifndef __UTIL_H
#define __UTIL_H

#define KPH_TO_MPH(km) (km * 0.621371192)
#define MS_TO_MPH(m) (m * 2.23693629)
#define M_TO_FT(m) (m * 3.2808399)
#define C_TO_F(c) ((c * 9.0/5.0) + 32)
#define MM_TO_IN(mm) (mm * 0.0393700787)
#define KTS_TO_MPH(kts) (kts * 1.15077945)

#define STREQ(x,y) (strcmp(x, y) == 0)
#define STRNEQ(x,y,n) (strncmp(x, y, n) == 0)

#define HAS_BEEN(s, d) ((time(NULL) - s) > d)

#define PI 3.14159265
#define DEG2RAD(x) (x*(PI/180))
#define RAD2DEG(x) (x/(PI/180))

const char *CARDINALS[] = { "N", "NE", "E", "SE", "S", "SW", "W", "NW" };

const char *direction(double degrees)
{
	return CARDINALS[((int)((degrees + 360 - 22.5) / 45.0)) % 7];
}

double get_direction(double fLng, double fLat, double tLng, double tLat)
{
	double rads;
	double result;

	fLng = DEG2RAD(fLng);
	fLat = DEG2RAD(fLat);
	tLng = DEG2RAD(tLng);
	tLat = DEG2RAD(tLat);

	rads = atan2(sin(tLng-fLng)*cos(tLat),
		     cos(fLat)*sin(tLat)-sin(fLat)*cos(tLat)*cos(tLng-fLng));

	result = RAD2DEG(rads);

	if (result < 0)
		return result + 360;
	else
		return result;
}

#endif
