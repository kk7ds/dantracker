/* Copyright 2012 Dan Smith <dsmith@danplanet.com> */

#ifndef __SERIAL_H
#define __SERIAL_H

int get_packet(int fd, char *buf, unsigned int *len);
int serial_open(const char *device, int baudrate, int hwflow);

#endif
