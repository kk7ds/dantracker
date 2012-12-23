/* -*- Mode: C; tab-width: 8;  indent-tabs-mode: nil; c-basic-offset: 8; c-brace-offset: -8; c-argdecl-indent: 8 -*- */
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
