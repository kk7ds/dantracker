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
#include <termios.h>
#include <getopt.h>

#include <fap.h>
#include <iniparser.h>

#include "ui.h"

#define FEND  0xC0

#define STREQ(x,y) (strcmp(x, y) == 0)
#define STRNEQ(x,y,n) (strncmp(x, y, n) == 0)

#define HAS_BEEN(s, d) ((time(NULL) - s) > d)

#define KEEP_PACKETS 8

#define DO_TYPE_NONE 0
#define DO_TYPE_WX   1
#define DO_TYPE_PHG  2

struct smart_beacon_point {
	float int_sec;
	float speed;
};

struct state {
	struct {
		char *tnc;
		char *gps;
		char *tel;

		char *gps_type;
		int testing;
		int verbose;
		char *icon;

		char *digi_path;

		int power;
		int height;
		int gain;
		int directivity;

		int atrest_rate;
		struct smart_beacon_point sb_low;
		struct smart_beacon_point sb_high;
		int course_change;

		unsigned int do_types;

		char **comments;
		int comments_count;

		char *config;

		double static_lat, static_lon, static_alt;
		double static_spd, static_crs;

	} conf;

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

	struct {
		double temp1;
		double voltage;

		time_t last_tel_beacon;
		time_t last_tel;
	} tel;

	char *mycall;

	fap_packet_t *recent[KEEP_PACKETS];
	int recent_idx;

	char gps_buffer[128];
	int gps_idx;
	time_t last_gps_update;
	time_t last_gps_data;
	time_t last_beacon;
	time_t last_time_set;

	int comment_idx;
	int other_beacon_idx;

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

#define KPH_TO_MPH(km) (km * 0.621371192)
#define MS_TO_MPH(m) (m * 2.23693629)
#define M_TO_FT(m) (m * 3.2808399)
#define C_TO_F(c) ((c * 9.0/5.0) + 32)
#define MM_TO_IN(mm) (mm * 0.0393700787)
#define KTS_TO_MPH(kts) (kts * 1.15077945)

#define TZ_OFFSET (-8)

void display_wx(fap_packet_t *_fap)
{
	fap_wx_report_t *fap = _fap->wx_report;
	char *wind = NULL;
	char *temp = NULL;
	char *rain = NULL;
	char *humid = NULL;
	char *pres = NULL;
	char *report = NULL;

	if (fap->wind_gust && fap->wind_dir && fap->wind_speed)
		asprintf(&wind, "Wind %s %.0fmph (%.0f gst) ",
			 direction(*fap->wind_dir),
			 MS_TO_MPH(*fap->wind_speed),
			 MS_TO_MPH(*fap->wind_gust));
	else if (fap->wind_dir && fap->wind_speed)
		asprintf(&wind, "Wind %s %.0f mph ",
			 direction(*fap->wind_dir),
			 MS_TO_MPH(*fap->wind_speed));

	if (fap->temp)
		asprintf(&temp, "%.0fF ", C_TO_F(*fap->temp));

	if (fap->rain_1h && *fap->rain_24h)
		asprintf(&rain, "Rain %.2f\"h%.2f\"d ",
			 MM_TO_IN(*fap->rain_1h),
			 MM_TO_IN(*fap->rain_24h));
	else if (fap->rain_1h)
		asprintf(&rain, "Rain %.2f\"h ", MM_TO_IN(*fap->rain_1h));
	else if (fap->rain_24h)
		asprintf(&rain, "Rain %.2f\"d ", MM_TO_IN(*fap->rain_24h));

	if (fap->humidity)
		asprintf(&humid, "Hum. %2i%% ", *fap->humidity);

	asprintf(&report, "%s%s%s%s",
		 wind ? wind : "",
		 temp ? temp : "",
		 rain ? rain : "",
		 humid ? humid : "");

	set_value("AI_COMMENT", report);

	/* Comment is used for larger WX report, so report the
	 * comment (if any) in the smaller course field
	 */
	if (_fap->comment_len) {
		char buf[512];

		strncpy(buf, _fap->comment, _fap->comment_len);
		buf[_fap->comment_len] = 0;
		set_value("AI_COURSE", buf);
	} else
		set_value("AI_COURSE", "");

	free(report);
	free(pres);
	free(humid);
	free(rain);
	free(temp);
	free(wind);
}

void display_telemetry(fap_telemetry_t *fap)
{
	set_value("AI_COURSE", "(Telemetry)");
	set_value("AI_COMMENT", "");
}

void display_phg(fap_packet_t *fap)
{
	int power, gain, dir;
	char height;
	int ret;
	char *buf = NULL;

	ret = sscanf(fap->phg, "%1d%c%1d%1d",
		     &power, &height, &gain, &dir);
	if (ret != 4) {
		set_value("AI_COURSE", "(Broken PHG)");
		return;
	}

	asprintf(&buf, "Power %iW at %.0fft (%idB gain @ %s)",
		 power*power,
		 pow(2, height - '0') * 10,
		 gain,
		 dir ? direction(dir) : "omni");
	set_value("AI_COMMENT", buf);
	free(buf);

	if (fap->comment) {
		buf = strndup(fap->comment, fap->comment_len);
		set_value("AI_COURSE", buf);
		free(buf);
	} else
		set_value("AI_COURSE", "");
}

void display_posit(fap_packet_t *fap, int isnew)
{
	char buf[512];

	if (fap->speed && fap->course && (*fap->speed > 0.0)) {
		snprintf(buf, sizeof(buf), "%.0f MPH %2s",
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
}

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

	if (fap->wx_report)
		display_wx(fap);
	else if (fap->telemetry)
		display_telemetry(fap->telemetry);
	else if (fap->phg)
		display_phg(fap);
	else
		display_posit(fap, isnew);

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

	for (i = KEEP_PACKETS, j = state->recent_idx + 1; i > 0; i--, j++) {
		fap_packet_t *p = state->recent[j % KEEP_PACKETS];
		double distance;

		sprintf(name, "AL_%02i", i-1);
		if (p)
			stored_packet_desc(p, i,
					   state->mypos.lat, state->mypos.lon,
					   buf, sizeof(buf));
		else
			sprintf(buf, "%i:", i);
		set_value(name, buf);
	}

	return 0;
}

/* Move packets below @index to @index */
int move_packets(struct state *state, int index)
{
	int i;
	const int max = KEEP_PACKETS;
	int end = (state->recent_idx +1 ) % max;

	fap_free(state->recent[index]);

	for (i = index; i != end; i -= 1) {
		if (i == 0)
			i = KEEP_PACKETS; /* Zero now, KEEP-1 next */
		state->recent[i % max] = state->recent[(i - 1) % max];
	}

	/* This made a hole at the bottom */
	state->recent[end] = NULL;

	return 0;
}

int find_packet(struct state *state, fap_packet_t *fap)
{
	int i;

	for (i = 0; i < KEEP_PACKETS; i++)
		if (state->recent[i] &&
		    STREQ(state->recent[i]->src_callsign, fap->src_callsign))
			return i;

	return -1;
}

int store_packet(struct state *state, fap_packet_t *fap)
{
	int i;

	if (STREQ(fap->src_callsign, state->mycall))
		return 0; /* Don't store our own packets */

	i = find_packet(state, fap);
	if (i != -1)
		move_packets(state, i);
	state->recent_idx = (state->recent_idx + 1) % KEEP_PACKETS;

	/* If found in spot X, remove and shift all up, then
	 * replace at top
	 */

	if (state->recent[state->recent_idx])
		fap_free(state->recent[state->recent_idx]);
	state->recent[state->recent_idx] = fap;

	update_packets_ui(state);

	return 0;
}

int update_mybeacon_status(struct state *state)
{
	char buf[512];
	struct tm last_beacon;
	uint8_t quality = state->digi_quality;
	int count = 1;
	int i;

	for (i = 1; i < 8; i++)
		count += (quality >> i) & 0x01;

	sprintf(buf, "%i", count / 2);
	set_value("G_SIGBARS", buf);

	localtime_r(&state->last_beacon, &last_beacon);
	strftime(buf, sizeof(buf), "%H:%M:%S", &last_beacon);
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
		if (STREQ(fap->src_callsign, state->mycall)) {
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

	if (state->mypos.speed > 1.0)
		sprintf(buf, "%.0f MPH %2s",
			KTS_TO_MPH(state->mypos.speed),
			direction(state->mypos.course));
	else
		sprintf(buf, "Stationary");
	set_value("G_SPD", buf);

	sprintf(buf, "%7s: %02i sats",
		state->mypos.qual != 0 ? "OK" : "<span foreground='red'>INVALID</span>",
		state->mypos.sats);
	set_value("G_STATUS", buf);

	sprintf(buf, "%02i:%02i:%02i",
		(state->mypos.tstamp / 10000),
		(state->mypos.tstamp / 100) % 100,
		(state->mypos.tstamp % 100));
	//set_value("G_TIME", buf);

	set_value("G_MYCALL", state->mycall);

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
	else if (!HAS_BEEN(state->last_time_set, 120))
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

	if (ret < 0) {
		perror("gps");
		return -errno;
	} else if (ret == 0)
		return 0;

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

	if (HAS_BEEN(state->last_gps_update, 3)) {
		display_gps_info(state);
		state->last_gps_update = time(NULL);
		set_time(state);
	}

	state->last_gps_data = time(NULL);

	return 0;
}

int handle_telemetry(int fd, struct state *state)
{
	char _buf[512] = "";
	int i = 0;
	int ret;
	char *buf = _buf;
	char *space;

	while (i < sizeof(_buf)) {
		ret = read(fd, &buf[i], 1);
		if (buf[i] == '\n')
			break;
		if (ret < 0)
			return -ret;
		else if (ret == 1)
			i++;
	}

	while (buf && *buf != '\n') {
		char name[16];
		char value[16];

		space = strchr(buf, ' ');
		if (space)
			*space = 0;

		ret = sscanf(buf, "%16[^=]=%16s", (char*)&name, (char*)&value);
		if (ret != 2) {
			printf("Invalid telemetry: %s\n", buf);
			return -EINVAL;
		}

		buf = space+1;

		if (STREQ(name, "temp1"))
			state->tel.temp1 = atof(value);
		else if (STREQ(name, "voltage"))
			state->tel.voltage = atof(value);
		else
			printf("Unknown telemetry value %s\n", name);
	}

	snprintf(_buf, sizeof(_buf), "%.1fV", state->tel.voltage);
	set_value("T_VOLTAGE", _buf);

	snprintf(_buf, sizeof(_buf), "%.0fF", state->tel.temp1);
	set_value("T_TEMP1", _buf);

	state->tel.last_tel = time(NULL);

	return 0;
}

/* Get a substitution value for a given key (result must be free()'d) */
char *get_subst(struct state *state, char *key)
{
	char *value;
	struct tm tm;
	char timestr[16];
	time_t t;

	t = time(NULL);
	localtime_r(&t, &tm);

	if (STREQ(key, "index"))
		asprintf(&value, "%i",
			 state->comment_idx++ % state->conf.comments_count);
	else if (STREQ(key, "mycall"))
		value = strdup(state->mycall);
	else if (STREQ(key, "temp1"))
		asprintf(&value, "%.0f", state->tel.temp1);
	else if (STREQ(key, "voltage"))
		asprintf(&value, "%.1f", state->tel.voltage);
	else if (STREQ(key, "sats"))
		asprintf(&value, "%i", state->mypos.sats);
	else if (STREQ(key, "ver"))
		asprintf(&value, "v0.1");
	else if (STREQ(key, "time")) {
		strftime(timestr, sizeof(timestr), "%H:%M:%S", &tm);
		value = strdup(timestr);
	} else if (STREQ(key, "date")) {
		strftime(timestr, sizeof(timestr), "%m/%d/%Y", &tm);
		value = strdup(timestr);
	} else
		printf("Unknown substitution `%s'", key);

	return value;
}

/* Given a string with substition variables, do the substitutions
 * and return the new result (which must be free()'d)
 */
char *process_subst(struct state *state, char *src)
{
	char *str;
	char *ptr1;
	char *ptr2;

	/* FIXME: might overrun! */
	str = malloc(strlen(src) * 2);
	if (!str)
		return str;
	str[0] = 0;

	for (ptr1 = src; *ptr1; ptr1++) {
		char subst[16] = "";
		char *value = NULL;

		ptr2 = strchr(ptr1, '$');
		if (!ptr2) {
			/* No more substs */
			strcat(str, ptr1);
			break;
		}

		/* Copy up to the next variable */
		strncat(str, ptr1, ptr2-ptr1);

		ptr1 = ptr2+1;
		ptr2 = strchr(ptr1, '$');
		if (!ptr2) {
			printf("Bad substitution `%s'\n", ptr1);
			goto err;
		}

		strncpy(subst, ptr1, ptr2-ptr1);
		ptr1 = ptr2;

		value = get_subst(state, subst);
		if (value) {
			strcat(str, value);
			free(value);
		}
	}

	return str;
 err:
	free(str);
	return NULL;
}

/*
 * Choose a comment out of the list, and choose a type
 * of (phg, wx, normal) from the list of configured types
 * and construct it.
 */
char *choose_data(struct state *state, char *req_icon)
{
	char *data = NULL;
	int cmt = state->comment_idx++ % state->conf.comments_count;
	char *comment;

	comment = process_subst(state, state->conf.comments[cmt]);
	if (!comment)
		comment = strdup("Error");

	/* We're moving, so we do course/speed */
	if (state->mypos.speed > 5) {
		asprintf(&data, "%03.0f/%03.0f%s",
			 state->mypos.course,
			 state->mypos.speed,
			 comment);
		goto out;
	}

	/* We're not moving, so choose a type */
	switch (state->other_beacon_idx++ % 3) {
	case DO_TYPE_WX:
		if ((state->conf.do_types & DO_TYPE_WX) &&
		    (!HAS_BEEN(state->tel.last_tel, 30))) {
			*req_icon = '_';
			asprintf(&data,
				 ".../...g...t%03.0fr...p...P...P...h..b.....%s",
				 state->tel.temp1,
				 comment);
			break;
		}
	case DO_TYPE_PHG:
		if (state->conf.do_types & DO_TYPE_PHG) {
			asprintf(&data,
				 "PHG%1d%1d%1d%1d%s",
				 state->conf.power,
				 state->conf.height,
				 state->conf.gain,
				 state->conf.directivity,
				 comment);
			break;
		}
	case DO_TYPE_NONE:
		data = strdup(comment);
		break;
	}
 out:
	free(comment);
	return data;
}

char *make_beacon(struct state *state, char *payload)
{
	char *data = NULL;
	char *packet;
	char _lat[16];
	char _lon[16];
	int ret;
	char icon = state->conf.icon[1];

	double lat = fabs(state->mypos.lat);
	double lon = fabs(state->mypos.lon);

	snprintf(_lat, 16, "%02.0f%05.2f%c",
		 floor(lat),
		 (lat - floor(lat)) * 60,
		 state->mypos.lat > 0 ? 'N' : 'S');

	snprintf(_lon, 16, "%03.0f%05.2f%c",
		 floor(lon),
		 (lon - floor(lon)) * 60,
		 state->mypos.lon > 0 ? 'E' : 'W');

	if (!payload)
		payload = data = choose_data(state, &icon);

	ret = asprintf(&packet,
		       "%s>APZDMS,%s:!%s%c%s%c%s",
		       state->mycall,
		       state->conf.digi_path,
		       _lat,
		       state->conf.icon[0],
		       _lon,
		       icon,
		       payload);

	free(data);

	if (ret < 0)
		return NULL;

	return packet;
}

int should_beacon(struct state *state)
{
	time_t delta = time(NULL) - state->last_beacon;
	time_t sb_min_delta;
	double sb_min_angle = 30;
	double sb_course_change = fabs(state->mypos.last_course -
				       state->mypos.course);
	float speed_frac;
	float d_speed = state->conf.sb_high.speed - state->conf.sb_low.speed;
	float d_rate = state->conf.sb_low.int_sec - state->conf.sb_high.int_sec;

	char *reason = NULL;

	/* Time required to have passed in order to beacon,
	 * 0 if never, -1 if now
	 */
	time_t req = 0;

	/* NEVER more often than every 10 seconds! */
	if (delta < 10)
		return 0;

	/* The fractional penetration into the lo/hi zone */
	speed_frac = (KTS_TO_MPH(state->mypos.speed) -
		      state->conf.sb_low.speed) / d_speed;

	/* Determine the fractional that we are slower than the max */
	sb_min_delta = (d_rate * (1 - speed_frac)) +
		state->conf.sb_high.int_sec;

	/* Never when we don't have a fix */
	if (state->mypos.qual == 0) {
		reason = "NOLOCK";
		goto out;
	}

	/* Never when we aren't getting data anymore */
	if (HAS_BEEN(state->last_gps_data, 30)) {
		reason = "NODATA";
		goto out;
	}

	/* If we're not moving at all, choose the "at rest" rate */
	if (state->mypos.speed <= 1) {
		req = state->conf.atrest_rate;
		reason = "ATREST";
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
	if (KTS_TO_MPH(state->mypos.speed) < state->conf.sb_low.speed) {
		req = state->conf.sb_low.int_sec;
		reason = "SLOWTO";
		goto out;
	}

	/* If we're going above the high point, use that interval */
	if (KTS_TO_MPH(state->mypos.speed) > state->conf.sb_high.speed) {
		req = state->conf.sb_high.int_sec;
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

	if (req == 0) {
		update_mybeacon_status(state);
		return 0;
	} else if (req == -1)
		return 1;
	else
		return delta > req;
}

int beacon(int fd, struct state *state)
{
	char *packet;
	char buf[512];
	unsigned int len = sizeof(buf);
	int ret;
	static time_t max_beacon_check = 0;

	/* Don't even check but every half-second */
	if (!HAS_BEEN(max_beacon_check, 0.5))
		return 0;

	max_beacon_check = time(NULL);

	if (!should_beacon(state))
		return 0;

	packet = make_beacon(state, NULL);
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
	state->mypos.lat = state->conf.static_lat;
	state->mypos.lon = state->conf.static_lon;
	state->mypos.alt = state->conf.static_alt;
	state->mypos.speed = state->conf.static_spd;
	state->mypos.course = state->conf.static_crs;

	state->mypos.qual = 1;
	state->mypos.sats = 0; /* We may claim qual=1, but no sats */

	state->last_gps_data = time(NULL);

	if ((time(NULL) - state->last_gps_update) > 3) {
		display_gps_info(state);
		state->last_gps_update = time(NULL);
	}
}

int set_mycall(struct state *state, char *callsign)
{
	strcpy(state->mycall, callsign);

	return 0;
}

int serial_set_rate(int fd, int baudrate)
{
	struct termios term;
	int ret;

	ret = tcgetattr(fd, &term);
	if (ret < 0)
		goto err;

	cfmakeraw(&term);
	cfsetspeed(&term, B9600);

	ret = tcsetattr(fd, TCSAFLUSH, &term);
	if (ret < 0)
		goto err;

	return 0;
 err:
	perror("unable to configure serial port");
	return ret;
}

int tnc_open(const char *device, int baudrate)
{
	int fd;
	int ret;

	fd = open(device, O_RDWR);
	if (fd < 0)
		return fd;

	ret = serial_set_rate(fd, baudrate);
	if (ret) {
		close(fd);
		fd = ret;
	}

	return fd;
}

int parse_opts(int argc, char **argv, struct state *state)
{
	static struct option lopts[] = {
		{"tnc",       1, 0, 't'},
		{"gps",       1, 0, 'g'},
		{"telemetry", 1, 0, 'T'},
		{"testing",   0, 0,  1 },
		{"verbose",   0, 0, 'v'},
		{"conf",      1, 0, 'c'},
		{NULL,        0, 0,  0 },
	};

	while (1) {
		int c;
		int optidx;

		c = getopt_long(argc, argv, "t:g:T:c:sv",
				lopts, &optidx);
		if (c == -1)
			break;

		switch(c) {
		case 't':
			state->conf.tnc = optarg;
			break;
		case 'g':
			state->conf.gps = optarg;
			break;
		case 'T':
			state->conf.tel = optarg;
			break;
		case 1:
			state->conf.testing = 1;
			break;
		case 'v':
			state->conf.verbose = 1;
			break;
		case 'c':
			state->conf.config = optarg;
			break;
		case '?':
			printf("Unknown option\n");
			return -1;
		};
	}

	return 0;
}

char **parse_list(char *string, int *count)
{
	char **list;
	char *ptr;
	int i = 0;

	for (ptr = string; *ptr; ptr++)
		if (*ptr == ',')
			i++;
	*count = i+1;

	list = calloc(*count, sizeof(char **));
	if (!list)
		return NULL;

	for (i = 0; string; i++) {
		ptr = strchr(string, ',');
		if (ptr) {
			*ptr = 0;
			ptr++;
		}
		list[i] = strdup(string);
		string = ptr;
	}

	return list;
}

int parse_ini(char *filename, struct state *state)
{
	dictionary *ini;
	char *tmp;
	int ret;

	ini = iniparser_load(filename);
	if (ini == NULL)
		return -EINVAL;

	if (!state->conf.tnc)
		state->conf.tnc = iniparser_getstring(ini, "tnc:port", NULL);

	if (!state->conf.gps)
		state->conf.gps = iniparser_getstring(ini, "gps:port", NULL);

	state->conf.gps_type = iniparser_getstring(ini, "gps:type", "static");

	if (!state->conf.tel)
		state->conf.tel = iniparser_getstring(ini, "telemetry:port",
						      NULL);

	state->mycall = iniparser_getstring(ini, "station:mycall", "N0CAL-7");
	state->conf.icon = iniparser_getstring(ini, "station:icon", "/>");

	if (strlen(state->conf.icon) != 2) {
		printf("ERROR: Icon must be two characters, not `%s'\n",
		       state->conf.icon);
		return -1;
	}

	state->conf.digi_path = iniparser_getstring(ini, "station:digi_path",
						    "WIDE1-1,WIDE2-1");

	state->conf.power = iniparser_getint(ini, "station:power", 0);
	state->conf.height = iniparser_getint(ini, "station:height", 0);
	state->conf.gain = iniparser_getint(ini, "station:gain", 0);
	state->conf.directivity = iniparser_getint(ini, "station:directivity",
						   0);

	state->conf.atrest_rate = iniparser_getint(ini,
						   "beaconing:atrest_rate",
						   600);
	state->conf.sb_low.speed = iniparser_getint(ini,
						    "beaconing:min_speed",
						    10);
	state->conf.sb_low.int_sec = iniparser_getint(ini,
						      "beaconing:min_rate",
						      600);
	state->conf.sb_high.speed = iniparser_getint(ini,
						     "beaconing:max_speed",
						     60);
	state->conf.sb_high.int_sec = iniparser_getint(ini,
						       "beaconing:max_rate",
						       60);
	state->conf.course_change = iniparser_getint(ini,
						     "beaconing:course_change",
						     30);

	state->conf.static_lat = iniparser_getdouble(ini,
						     "static:lat",
						     0.0);
	state->conf.static_lon = iniparser_getdouble(ini,
						     "static:lon",
						     0.0);
	state->conf.static_alt = iniparser_getdouble(ini,
						     "static:alt",
						     0.0);
	state->conf.static_spd = iniparser_getdouble(ini,
						     "static:speed",
						     0.0);
	state->conf.static_crs = iniparser_getdouble(ini,
						     "static:course",
						     0.0);

	tmp = iniparser_getstring(ini, "station:beacon_types", "posit");
	if (strlen(tmp) != 0) {
		char **types;
		int count;
		int i;

		types = parse_list(tmp, &count);
		if (!types) {
			printf("Failed to parse beacon types\n");
			return -EINVAL;
		}

		for (i = 0; i < count; i++) {
			if (STREQ(types[i], "weather"))
				state->conf.do_types |= DO_TYPE_WX;
			else if (STREQ(types[i], "phg"))
				state->conf.do_types |= DO_TYPE_PHG;
			else
				printf("WARNING: Unknown beacon type %s\n",
				       types[i]);
			free(types[i]);
		}
		free(types);
	}

	tmp = iniparser_getstring(ini, "comments:enabled", "");
	if (strlen(tmp) != 0) {
		int i;

		state->conf.comments = parse_list(tmp,
						  &state->conf.comments_count);
		if (!state->conf.comments)
			return -EINVAL;

		for (i = 0; i < state->conf.comments_count; i++) {
			char section[32];

			snprintf(section, sizeof(section),
				 "comments:%s", state->conf.comments[i]);
			free(state->conf.comments[i]);
			state->conf.comments[i] = iniparser_getstring(ini,
								      section,
								      "INVAL");
		}
	}
	return 0;
}

int main(int argc, char **argv)
{
	int tncfd;
	int gpsfd;
	int telfd;
	double mylat = 45.525;
	double mylon = -122.9164;
	int i;

	fd_set fds;

	struct state state;
	memset(&state, 0, sizeof(state));

	fap_init();

	if (parse_opts(argc, argv, &state)) {
		printf("Invalid option(s)\n");
		exit(1);
	}

	if (parse_ini(state.conf.config ? state.conf.config : "aprs.ini", &state)) {
		printf("Invalid config\n");
		exit(1);
	}

	if (!state.conf.verbose)
		redir_log();

	if (state.conf.testing)
		state.digi_quality = 0xFF;

	state.last_beacon = 0;

	for (i = 0; i < KEEP_PACKETS; i++)
		state.recent[i] = NULL;

	tncfd = tnc_open(state.conf.tnc, 9600);
	if (tncfd < 0) {
		printf("Failed to open TNC: %m\n");
		exit(1);
	}

	if (state.conf.gps) {
		gpsfd = open(state.conf.gps, O_RDONLY);
		if (gpsfd < 0) {
			perror(state.conf.gps);
			exit(1);
		}
	} else
		gpsfd = -1;

	if (state.conf.tel) {
		telfd = tnc_open(state.conf.tel, 9600);
		if (telfd < 0) {
			perror(state.conf.tel);
			exit(1);
		}
	} else
		telfd = -1;

	FD_ZERO(&fds);

	while (1) {
		int ret;
		struct timeval tv = {1, 0};

		FD_SET(tncfd, &fds);
		if (gpsfd > 0)
			FD_SET(gpsfd, &fds);
		if (telfd > 0)
			FD_SET(telfd, &fds);

		if (STREQ(state.conf.gps_type, "static"))
			fake_gps_data(&state);

		ret = select(telfd+1, &fds, NULL, NULL, &tv);
		if (ret == -1) {
			perror("select");
			continue;
		}

		if (FD_ISSET(tncfd, &fds))
			handle_incoming_packet(tncfd, &state);
		if (FD_ISSET(gpsfd, &fds))
			handle_gps_data(gpsfd, &state);
		if (FD_ISSET(telfd, &fds))
			handle_telemetry(telfd, &state);

		beacon(tncfd, &state);
		fflush(NULL);
	}

	fap_cleanup();
}
