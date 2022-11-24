#!/bin/bash

DIR=../itd/LINUX/FTDI/
LIBDIR=/usr/local/lib

sudo rm /usr/local/lib/libftd3xx.so
sudo cp $DIR/lib/x86_64/libftd3xx.so $LIBDIR
sudo cp $DIR/lib/x86_64/libftd3xx.so.0.5.21 $LIBDIR
sudo cp $DIR/docs/51-ftd3xx.rules /etc/udev/rules.d/
sudo udevadm control --reload-rules
