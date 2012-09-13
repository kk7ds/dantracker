#!/usr/bin/python

import glob
import serial
import os

ports = glob.glob("/dev/ttyUSB*")
for port in ports:
    s = serial.Serial(port, 4800, timeout=3)
    data = s.read(256)
    if "\n$GP" in data:
        os.system("ln -fs %s /dev/gps" % port)
        os.system("ls -l /dev/gps")
        del ports[ports.index(port)]
        break

if ports:
    os.system("ln -fs %s /dev/radio" % ports[0])
    os.system("ls -l /dev/radio")
