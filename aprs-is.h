/* -*- Mode: C; tab-width: 8;  indent-tabs-mode: nil; c-basic-offset: 8; c-brace-offset: -8; c-argdecl-indent: 8 -*- */
/* Copyright 2012 Dan Smith <dsmith@danplanet.com> */

#ifndef __APRS_IS_H
#define __APRS_IS_H

int aprsis_connect(const char *hostname, int port, const char *mycall,
                   double lat, double lon, double range);
int get_packet_text(int fd, char *buffer, unsigned int *len);

#endif
