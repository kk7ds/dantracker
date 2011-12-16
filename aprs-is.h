#ifndef __APRS_IS_H
#define __APRS_IS_H

int aprsis_connect(const char *hostname, int port, const char *mycall,
		   double lat, double lon, double range);
int get_packet_text(int fd, char *buffer, unsigned int *len);

#endif
