#!/bin/bash
# the build / compile instruction is already exist at bottom of the code, but who will check the code ? :)

gcc select_area.c -o select_area $( pkg-config --cflags --libs gtk+-3.0 )
