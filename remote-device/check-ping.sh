#!/bin/bash
ping -W 20 -c 1 google.pl || killall /usr/bin/wvdial

