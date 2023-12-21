#!/bin/bash
rm -rf var/lib/dhcp && mkdir -p rw/tmp-files/dhcp && ln -s /rw/tmp-files/dhcp var/lib/dhcp
rm -rf var/lib/dhcpcd && mkdir -p rw/tmp-files/dhcpcd && ln -s /rw/tmp-files/dhcpcd var/lib/dhcpcd
rm -rf etc/resolv.conf && mkdir -p rw/tmp-files/ && ln -s /rw/tmp-files/resolv.conf etc/resolv.conf
rm -rf etc/ppp && mkdir -p rw/tmp-files/ppp && ln -s /rw/tmp-files/ppp etc/ppp
