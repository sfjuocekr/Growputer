#!/bin/sh

avrdude -p ATmega2560 -c wiring -P /dev/ttyACM0 -U flash:w:bin/Growputer.hex
