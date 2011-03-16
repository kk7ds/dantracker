#define _GNU_SOURCE
#include <unistd.h>
#include <stdio.h>
#include <math.h>

struct pos {
	double lat;
	double lon;
	double speed;
	double course;
};

char *checksum(const char *str)
{
	unsigned char cs = 0;
	const char *ptr;
	static char csstr[4];

	for (ptr = str+1; *ptr; ptr++)
		cs ^= *ptr;

	sprintf(csstr, "*%02hhX", cs % 256);
	return csstr;
}

void make_gga(FILE *fp, struct pos *pos)
{
	char *str = NULL;
	int lat_d = floor(pos->lat);
	double lat_m = (pos->lat - lat_d) * 60.0;
	int lon_d = floor(pos->lon);
	double lon_m = (pos->lon - lon_d) * 60.0;

	asprintf(&str,
		"$GPGGA,000000,%02i%06.3f,%c,%03i%06.3f,%c,"
		 "1,04,0.9,001.1,M,50.0,M,,,",
		 lat_d, lat_m, lat_d > 0 ? 'N' : 'S',
		 lon_d, lon_m, lon_d > 0 ? 'E' : 'W');


	fprintf(fp, "%s%s\r", str, checksum(str));
}

void make_rmc(FILE *fp, struct pos *pos)
{
	char *str = NULL;
	int lat_d = floor(pos->lat);
	double lat_m = (pos->lat - lat_d) * 60.0;
	int lon_d = floor(pos->lon);
	double lon_m = (pos->lon - lon_d) * 60.0;
	int cs = 0;
	char *ptr;

	asprintf(&str,
		"$GPRMC,000000,A,%02i%06.3f,%c,%03i%06.3f,%c,"
		 "%05.1f,%05.1f,000000,003.0,W,",
		 lat_d, lat_m, lat_d > 0 ? 'N' : 'S',
		 lon_d, lon_m, lon_d > 0 ? 'E' : 'W',
		 pos->speed, pos->course);

	for (ptr = str; *ptr; ptr++)
		cs += *ptr;

	fprintf(fp, "%s%s\r", str, checksum(str));
}

int main()
{
	struct pos pos = {45.525, 122.9164, 55.0, 123.0};
	int i = 0;

	while (1) {
		i++;
		make_gga(stdout, &pos);
		make_rmc(stdout, &pos);
		fflush(NULL);
		sleep(1);

		if ((i > 16) && (i < 20))
			pos.course += 10;
	}
}
