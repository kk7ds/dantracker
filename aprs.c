#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <fcntl.h>
#include <sys/time.h>
#include <sys/select.h>
#include <unistd.h>
#include <time.h>
#include <math.h>
#include <errno.h>

#include <fap.h>

#include "ui.h"

#define FEND  0xC0

#define STREQ(x,y) (strcmp(x, y) == 0)
#define STRNEQ(x,y,n) (strncmp(x, y, n) == 0)

struct state {
	struct {
		double lat;
		double lon;
		double alt;
		double course;
		double speed;
		int qual;
		int sats;
		int tstamp;
		int dstamp;

		double last_course;
	} mypos;

	char mycall[7];
	int ssid;
	char my_callsign[32];

	char gps_buffer[128];
	int gps_idx;
	time_t last_gps_update;
	time_t last_beacon;
	time_t last_time_set;

	uint8_t digi_quality;
};

const char *CARDINALS[] = { "N", "NE", "E", "SE", "S", "SW", "W", "NW" };

const char *direction(double degrees)
{
	return CARDINALS[((int)((degrees + 360 - 22.5) / 45.0)) % 7];
}

#define PI 3.14159265
#define DEG2RAD(x) (x*(PI/180))
#define RAD2DEG(x) (x/(PI/180))

double get_direction(double fLng, double fLat, double tLng, double tLat)
{
	double rads;
	int result;

	fLng = DEG2RAD(fLng);
	fLat = DEG2RAD(fLat);
	tLng = DEG2RAD(tLng);
	tLat = DEG2RAD(tLat);

	rads = atan2(sin(fLng-tLng)*cos(tLat),
		     cos(fLat)*sin(tLat)-sin(fLat)*cos(tLat)*cos(fLng-tLng));

	result = RAD2DEG(rads);

	return (result + 360) % 360;
}

int get_packet(int fd, char *buf, unsigned int *len)
{
	unsigned char byte = 0x00;
	char packet[512] = "";
	int ret;
	int pos = 0;
	unsigned int tnc_id;

	while (byte != FEND)
		read(fd, &byte, 1);

	packet[pos++] = byte;

	while (1) {
		ret = read(fd, &byte, 1);
		if (ret != 1)
			continue;
		packet[pos++] = byte;
		if (byte == FEND)
			break;
	}

	ret = fap_kiss_to_tnc2(packet, pos, buf, len, &tnc_id);
	if (!ret)
		printf("Failed to convert packet: %s\n", buf);

	return ret;
}

#define KEEP_PACKETS 8
fap_packet_t *last_packets[KEEP_PACKETS];
int last_packet = 0;

#define KPH_TO_MPH(km) (km * 0.621371192)
#define MS_TO_MPH(m) (m * 2.23693629)
#define M_TO_FT(m) (m * 3.2808399)
#define C_TO_F(c) ((c * 9.0/5.0) + 32)
#define MM_TO_IN(mm) (mm * 0.0393700787)
#define KTS_TO_MPH(kts) (kts * 1.15077945)

#define TZ_OFFSET (-8)

void display_packet(fap_packet_t *fap, double mylat, double mylon)
{
	char buf[512];
	static char last_callsign[10] = "";
	int isnew = 1;

	if (STREQ(fap->src_callsign, last_callsign))
		isnew = 1;

	set_value("AI_CALLSIGN", fap->src_callsign);
	strncpy(last_callsign, fap->src_callsign, 9);
	last_callsign[9] = 0;

	if (fap->latitude && fap->longitude) {
		snprintf(buf, sizeof(buf), "%5.1fmi %2s",
			 KPH_TO_MPH(fap_distance(mylon, mylat,
						 *fap->longitude,
						 *fap->latitude)),
			 direction(get_direction(mylon, mylat,
						 *fap->longitude,
						 *fap->latitude)));
		set_value("AI_DISTANCE", buf);
	} else if (fap->latitude && fap->longitude && fap->altitude) {
		snprintf(buf, 512, "%5.1fmi %2s (%4.0f ft)",
			 KPH_TO_MPH(fap_distance(mylon, mylat,
						 *fap->longitude,
						 *fap->latitude)),
			 direction(get_direction(mylon, mylat,
						 *fap->longitude,
						 *fap->latitude)),
			 M_TO_FT(*fap->altitude));
		set_value("AI_DISTANCE", buf);
	} else if (isnew)
		set_value("AI_DISTANCE", "");

	if (fap->speed && fap->course && (*fap->speed > 0.0)) {
		snprintf(buf, sizeof(buf), "%3.0f MPH %2s",
			 KPH_TO_MPH(*fap->speed),
			 direction(*fap->course));
		set_value("AI_COURSE", buf);
	} else if (isnew)
		set_value("AI_COURSE", "");

	if (fap->type && (*fap->type == fapSTATUS)) {
		strncpy(buf, fap->status, fap->status_len);
		buf[fap->status_len] = 0;
		set_value("AI_COMMENT", buf);
	} else if (fap->comment_len) {
		strncpy(buf, fap->comment, fap->comment_len);
		buf[fap->comment_len] = 0;
		set_value("AI_COMMENT", buf);
	} else if (isnew)
		set_value("AI_COMMENT", "");

	snprintf(buf, sizeof(buf), "%c%c", fap->symbol_table, fap->symbol_code);
	set_value("AI_ICON", buf);

}

int stored_packet_desc(fap_packet_t *fap, int index,
		       double mylat, double mylon,
		       char *buf, int len)
{
	if (fap->latitude && fap->longitude)
		snprintf(buf, len,
			 "%i: %-9s <small>%3.0fmi %-2s</small>",
			 index, fap->src_callsign,
			 KPH_TO_MPH(fap_distance(mylon, mylat,
						 *fap->longitude,
						 *fap->latitude)),
			 direction(get_direction(mylon, mylat,
						 *fap->longitude,
						 *fap->latitude)));
	else
		snprintf(buf, len,
			 "%i: %-9s", index, fap->src_callsign);

	return 0;
}

int update_packets_ui(struct state *state)
{
	int i, j;
	char name[] = "AL_00";
	char buf[64];

	for (i = KEEP_PACKETS, j = last_packet+1; i > 0; i--, j++) {
		fap_packet_t *p = last_packets[j % KEEP_PACKETS];
		double distance;
		buf[0] = 0;

		sprintf(name, "AL_%02i", i-1);
		if (p)
			stored_packet_desc(p, i,
					   state->mypos.lat, state->mypos.lon,
					   buf, sizeof(buf));
		set_value(name, buf);
	}

	return 0;
}

/* Move packets below @index to @index */
int move_packets(struct state *state, int index)
{
	int i;
	const int max = KEEP_PACKETS;
	int end = (last_packet+1) % max;

	fap_free(last_packets[index]);

	for (i = index; i != end; i -= 1) {
		if (i == 0)
			i = KEEP_PACKETS; /* Zero now, KEEP-1 next */
		last_packets[i % max] = last_packets[(i-1) % max];
	}

	/* This made a hole at the bottom */
	last_packets[end] = NULL;

	return 0;
}

int find_packet(struct state *state, fap_packet_t *fap)
{
	int i;

	for (i = 0; i < KEEP_PACKETS; i++)
		if (last_packets[i] &&
		    STREQ(last_packets[i]->src_callsign, fap->src_callsign))
			return i;

	return -1;
}

int store_packet(struct state *state, fap_packet_t *fap)
{
	int i;

	if (STREQ(fap->src_callsign, state->my_callsign))
		return 0; /* Don't store our own packets */

	i = find_packet(state, fap);
	if (i != -1)
		move_packets(state, i);
	last_packet = (last_packet + 1) % KEEP_PACKETS;

	/* If found in spot X, remove and shift all up, then
	 * replace at top
	 */

	if (last_packets[last_packet])
		fap_free(last_packets[last_packet]);
	last_packets[last_packet] = fap;

	update_packets_ui(state);

	return 0;
}

int update_mybeacon_status(struct state *state)
{
	char buf[512];
	struct tm last_beacon;
	const char *status = "  ..-^^|";
	uint8_t quality = state->digi_quality;
	int count = 0;
	int i;

	localtime_r(&state->last_beacon, &last_beacon);

	strftime(buf, sizeof(buf), " %H:%M:%S", &last_beacon);

	for (i = 1; i < 8; i++)
		count += (quality >> i) & 0x01;

	/* FIXME: Make this pretty */
	buf[0] = status[count];

	set_value("G_LASTBEACON", buf);
}

int handle_incoming_packet(int fd, struct state *state)
{
	char packet[512];
	unsigned int len = sizeof(packet);
	fap_packet_t *fap;
	char err[128];
	int ret;

	memset(packet, 0, len);

	ret = get_packet(fd, packet, &len);
	if (!ret)
		return -1;

	printf("%s\n", packet);
	fap = fap_parseaprs(packet, len, 1);
	if (!fap->error_code) {
		display_packet(fap, state->mypos.lat, state->mypos.lon);
		store_packet(state, fap);
		if (STREQ(fap->src_callsign, state->my_callsign)) {
			state->digi_quality |= 1;
			update_mybeacon_status(state);
		}
	}

	return 0;
}

double parse_lat(char *str)
{
	int deg;
	float min;
	int ret;

	ret = sscanf(str, "%2i%f", &deg, &min);
	if (ret != 2)
		printf("Failed to parse %s\n", str);

	return deg + (min / 60.0);
}

double parse_lon(char *str)
{
	int deg;
	float min;
	int ret;

	ret = sscanf(str, "%3i%f", &deg, &min);
	if (ret != 2)
		printf("Failed to parse %s\n", str);

	return deg + (min / 60.0);
}

int parse_gga(struct state *state, char *str)
{
	int num = 0;
	char *field = strchr(str, ',');

	//if (state->mypos.qual == 0)
	//printf("GGA: %s\n", str);

	while (str && field) {
		*field = 0;

		switch (num) {
		case 1:
			state->mypos.tstamp = atoi(str);
			break;
		case 2:
			state->mypos.lat = parse_lat(str);
			break;
		case 3:
			if (*str == 'S')
				state->mypos.lat *= -1;
			break;
		case 4:
			state->mypos.lon = parse_lon(str);
			break;
		case 5:
			if (*str == 'W')
				state->mypos.lon *= -1;
			break;
		case 6:
			state->mypos.qual = atoi(str);
			break;
		case 7:
			state->mypos.sats = atoi(str);
			break;
		case 9:
			state->mypos.alt = atof(str);
			break;
		}

		num++;
		str = field + 1;
		field = strchr(str, ',');
	}

	return 0;
}

int parse_rmc(struct state *state, char *str)
{
	int num = 0;
	char *field = strchr(str, ',');

	//if (state->mypos.qual == 0)
	//printf("RMC: %s\n", str);

	/* Save the current course for smart-beaconing detection */
	state->mypos.last_course = state->mypos.course;

	while (str && field) {
		*field = 0;

		switch (num) {
		case 2:
			if (*str != 'A') /* Not ACTIVE */
				return 0;
			break;
		case 7:
			state->mypos.speed = atof(str);
			break;
		case 8:
			state->mypos.course = atof(str);
			break;
		case 9:
			state->mypos.dstamp = atoi(str);
			break;
		};

		num++;
		str = field + 1;
		field = strchr(str, ',');
	}

	return 0;
}

int parse_gps_string(struct state *state)
{
	char *str = state->gps_buffer;

	if (*str == '\n')
		str++;

	if (strncmp(str, "$GPGGA", 6) == 0)
		return parse_gga(state, str);
	else if (strncmp(str, "$GPRMC", 6) == 0)
		return parse_rmc(state, str);

	return 0;
}

int display_gps_info(struct state *state)
{
	char buf[512];

	sprintf(buf, "%7.5f %8.5f", state->mypos.lat, state->mypos.lon);
	set_value("G_LATLON", buf);

	sprintf(buf, "ALT %4.0f ft", state->mypos.alt);
	set_value("G_ALT", buf);

	sprintf(buf, "%3.0f MPH %2s",
		KTS_TO_MPH(state->mypos.speed),
		state->mypos.speed > 2.0 ?
		direction(state->mypos.course) : "");
	set_value("G_SPD", buf);

	sprintf(buf, "%7s: %02i sats",
		state->mypos.qual != 0 ? "OK" : "INVALID",
		state->mypos.sats);
	set_value("G_STATUS", buf);

	sprintf(buf, "%02i:%02i:%02i",
		(state->mypos.tstamp / 10000),
		(state->mypos.tstamp / 100) % 100,
		(state->mypos.tstamp % 100));
	//set_value("G_TIME", buf);

	set_value("G_MYCALL", state->my_callsign);

}

int set_time(struct state *state)
{
	struct tm tm;
	time_t tstamp = state->mypos.tstamp;
	time_t dstamp = state->mypos.dstamp;
	char timestr[64];
	int ret;

	if (state->mypos.qual == 0)
		return 1; /* No fix, no set */
	else if ((time(NULL) - state->last_time_set) < 120)
		return 1; /* Too recent */

	tm.tm_mday = dstamp / 10000;
	tm.tm_mon = (dstamp / 100) % 100;
	tm.tm_year = (dstamp % 100);

	tm.tm_hour = (tstamp / 10000) + TZ_OFFSET;
	tm.tm_min = (tstamp / 100) % 100;
	tm.tm_sec = (tstamp % 100);

	if (tm.tm_hour < 0) {
		tm.tm_hour *= -1;
		tm.tm_mday -= 1;
	}

	snprintf(timestr, sizeof(timestr),
		 "date %02i%02i%02i%02i20%02i.%02i",
		 tm.tm_mon, tm.tm_mday,
		 tm.tm_hour, tm.tm_min,
		 tm.tm_year, tm.tm_sec);

	ret = system(timestr);
	printf("Setting date %s: %s\n", timestr, ret == 0 ? "OK" : "FAIL");
	state->last_time_set = time(NULL);

	return 0;
}

int handle_gps_data(int fd, struct state *state)
{
	char buf[33];
	int ret;
	char *cr;

	ret = read(fd, buf, 32);
	buf[ret] = 0; /* Safe because size is +1 */

	if (state->gps_idx + ret > sizeof(state->gps_buffer)) {
		printf("Clearing overrun buffer\n");
		state->gps_idx = 0;
	}

	cr = strchr(buf, '\r');
	if (cr) {
		*cr = 0;
		strcpy(&state->gps_buffer[state->gps_idx], buf);
		parse_gps_string(state);
		strcpy(state->gps_buffer, cr+1);
		state->gps_idx = strlen(state->gps_buffer);
	} else {
		memcpy(&state->gps_buffer[state->gps_idx], buf, ret);
		state->gps_idx += ret;
	}

	if ((time(NULL) - state->last_gps_update) > 3) {
		display_gps_info(state);
		state->last_gps_update = time(NULL);
		set_time(state);
	}

	return 0;
}

char *make_beacon(struct state *state, char *comment)
{
	char *packet;
	char _lat[16];
	char _lon[16];
	int ret;

	double lat = fabs(state->mypos.lat);
	double lon = fabs(state->mypos.lon);
	char course[] = "000/000";

	snprintf(_lat, 16, "%02.0f%05.2f%c",
		 floor(lat),
		 (lat - floor(lat)) * 60,
		 state->mypos.lat > 0 ? 'N' : 'S');

	snprintf(_lon, 16, "%03.0f%05.2f%c",
		 floor(lon),
		 (lon - floor(lon)) * 60,
		 state->mypos.lon > 0 ? 'E' : 'W');

	if (state->mypos.speed > 1.0)
		snprintf(course, sizeof(course),
			 "%03.0f/%03.0f",
			 state->mypos.course,
			 state->mypos.speed);
	else
		strcpy(course, "");

	ret = asprintf(&packet,
		       "%s-%i>APZDMS,WIDE1-1,WIDE2-1:!%s%c%s%c%s%s",
		       state->mycall, state->ssid,
		       _lat,
		       '/',
		       _lon,
		       'j',
		       course,
		       comment);

	if (ret < 0)
		return NULL;

	return packet;
}

struct smart_beacon_point {
	float int_sec;
	float speed;
};

int should_beacon(struct state *state)
{
	time_t delta = time(NULL) - state->last_beacon;
	time_t sb_min_delta;
	double sb_min_angle = 30;
	double sb_course_change = fabs(state->mypos.last_course -
				       state->mypos.course);
	float speed_frac;

	struct smart_beacon_point sb_low  = { 300, 10 };
	struct smart_beacon_point sb_high = {  60, 60 };

	char *reason = NULL;

	/* Time required to have passed in order to beacon,
	 * 0 if never, -1 if now
	 */
	time_t req = 0;

	/* NEVER more often than every 10 seconds! */
	if (delta < 10)
		return 0;

	/* The fractional penetration into the lo/hi zone */
	speed_frac = (KTS_TO_MPH(state->mypos.speed) - sb_low.speed) /
		(sb_high.speed - sb_low.speed);

	/* Determine the fractional that we are slower than the max */
	sb_min_delta = (fabs(sb_high.int_sec - sb_low.int_sec) *
			(1 - speed_frac)) + sb_high.int_sec;

	/* Never when we don't have a fix */
	if (state->mypos.qual == 0) {
		reason = "NOLOCK";
		goto out;
	}

	/* Never less often than every 30 minutes */
	if (delta > (30 * 60)) {
		reason = "30MIN";
		req = -1;
		goto out;
	}

	/* SmartBeaconing: Course Change (only if moving) */
	if ((sb_course_change > sb_min_angle) &&
	    (KPH_TO_MPH(state->mypos.speed) > 2.0)) {
		printf("SB: Angle changed by %.0f\n",
		       state->mypos.last_course - state->mypos.course);
		state->mypos.last_course = state->mypos.course;
		reason = "COURSE";
		req = -1;
		goto out;
	}

	/* SmartBeaconing: Range-based variable speed beaconing */

	/* If we're going below the low point, use that interval */
	if (KTS_TO_MPH(state->mypos.speed) < sb_low.speed) {
		req = sb_low.int_sec;
		reason = "SLOWTO";
		goto out;
	}

	/* If we're going above the high point, use that interval */
	if (KTS_TO_MPH(state->mypos.speed) > sb_high.speed) {
		req = sb_high.int_sec;
		reason = "FASTTO";
		goto out;
	}

	/* We must be in the speed zone, so adjust interval according
	 * to the fractional penetration of the speed range
	 */
	req = sb_min_delta;
	reason = "FRACTO";
 out:
	if (reason) {
		char tmp[256];
		if (req <= 0)
			strcpy(tmp, reason);
		else
			sprintf(tmp, "Every %lu sec", req);
		set_value("G_REASON", tmp);
	}

	if (req == 0)
		return 0;
	else if (req == -1)
		return 1;
	else
		return delta > req;
}

int beacon(int fd, struct state *state)
{
	time_t delta = time(NULL) - state->last_beacon;
	char *packet;
	char buf[512];
	unsigned int len = sizeof(buf);
	int ret;

	if (!should_beacon(state))
		return 0;

	packet = make_beacon(state, "testing");
	if (!packet) {
		printf("Failed to make beacon TNC2 packet\n");
		return 1;
	}
	printf("Sending Packet: %s\n", packet);

	ret = fap_tnc2_to_kiss(packet, strlen(packet),
			       0,
			       buf, &len);
	free(packet);
	if (!ret) {
		printf("Failed to make beacon KISS packet\n");
		return 1;
	}

	state->last_beacon = time(NULL);
	state->digi_quality <<= 1;
	update_mybeacon_status(state);

	ret = write(fd, buf, len);

	return 0;
}

int redir_log()
{
	int fd;

	fd = open("/tmp/aprs.log", O_WRONLY|O_TRUNC|O_CREAT, 0644);
	if (fd < 0) {
		perror("/tmp/aprs.log");
		return -errno;
	}

	dup2(fd, STDOUT_FILENO);
	dup2(fd, STDERR_FILENO);

	setvbuf(stdout, NULL, _IONBF, 0);

	return 0;
}

int fake_gps_data(struct state *state)
{
	state->mypos.lat = 45.525;
	state->mypos.lon = -122.9164;
	state->mypos.qual = 1;
	state->mypos.speed = 8;
	state->mypos.course = 123;
	state->mypos.sats = 4;
}

int set_mycall(struct state *state, char *callsign, int ssid)
{
	strcpy(state->mycall, callsign);
	state->ssid = ssid;
	snprintf(state->my_callsign, sizeof(state->my_callsign),
		 "%s-%i", callsign, ssid);

	return 0;
}

int main(int argc, char **argv)
{
	int tncfd;
	int gpsfd;
	double mylat = 45.525;
	double mylon = -122.9164;
	int i;

	fd_set fds;

	struct state state;

	redir_log();

	memset(&state, 0, sizeof(state));

	set_mycall(&state, "KK7DS", 10);
	state.last_beacon = time(NULL);

	fap_init();

	for (i = 0; i < KEEP_PACKETS; i++)
		last_packets[i] = NULL;

	tncfd = open(argv[1], O_RDWR);
	if (tncfd < 0) {
		perror(argv[1]);
		exit(1);
	}

	gpsfd = open(argv[2], O_RDONLY);
	if (gpsfd < 0) {
		perror(argv[2]);
		exit(1);
	}

	FD_ZERO(&fds);

	while (1) {
		int ret;
		struct timeval tv = {1, 0};

		FD_SET(tncfd, &fds);
		FD_SET(gpsfd, &fds);
		//fake_gps_data(&state);

		ret = select(gpsfd+1, &fds, NULL, NULL, &tv);
		if (ret == -1) {
			perror("select");
			continue;
		}

		if (FD_ISSET(tncfd, &fds))
			handle_incoming_packet(tncfd, &state);
		if (FD_ISSET(gpsfd, &fds))
			handle_gps_data(gpsfd, &state);

		beacon(tncfd, &state);
		fflush(NULL);
	}

	fap_cleanup();
}
