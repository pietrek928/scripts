#!/bin/bash
#systemctl stop ssh-forward
#systemctl stop check-ping
sleep 45
#wvdial pin
#sleep 30
wvdial internety &
for i in 1 2 3 4 5 6 7 8 9 10 11 12 13 14;
do
    sleep 10
    wget -T 5 -O /dev/null "https://elektroinf.eu/test.txt" && break
done
ssh -N forwarder@elektroinf.eu -p 17683 -R 47552:127.0.0.1:22 -o ExitOnForwardFailure=yes
killall /usr/bin/wvdial
sleep 1
killall /usr/bin/pppd
sleep 5
killall -9 /usr/bin/wvdial
sleep 1
killall -9 /usr/bin/pppd
#( sleep 75 && systemctl restart ssh-forward ) &
#( sleep 600 && systemctl restart check-ping ) &
#wvdial internety
#systemctl restart sms
