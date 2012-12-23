/* -*- Mode: C; tab-width: 8;  indent-tabs-mode: nil; c-basic-offset: 8; c-brace-offset: -8; c-argdecl-indent: 8 -*- */
/* Copyright 2012 Dan Smith <dsmith@danplanet.com> */

#ifndef __SERIAL_H
#define __SERIAL_H

int get_packet(int fd, char *buf, unsigned int *len);
int serial_open(const char *device, int baudrate, int hwflow);

#endif
