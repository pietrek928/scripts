#!/bin/bash

IP_CACHE_FILE="/tmp/.ip"

rm ${IP_CACHE_FILE}

function seohost_update() {
    IP="$1"
    LAST_IP=$(cat ${IP_CACHE_FILE})
    if [ "${IP}" == "${LAST_IP}" ] ; then
        return 0
    fi

    echo "IP change ${LAST_IP} -> ${IP}"
    ./seohost-set.py "${IP}" || ( echo "Setting ip failed" && return 1 )

    echo -n ${IP} > "${IP_CACHE_FILE}"
}

function check_net() {
    ping -W 20 -q -c 1 google.pl >/dev/null 2>/dev/null || ( echo "ping google failed" && return 1 )
}

err_count=0

while true ; do
    if ! check_net ; then
        err_count=$((err_count+1))
        echo "Net failed, error count: ${err_count}"
        if [ $err_count -gt 2 ] ; then
            sleep 120
        fi
    else
        err_count=0
    fi

    IP="$( ./funbox-ip.py )"
    if [ -n "${IP}" ] ; then
        seohost_update "${IP}"
    fi

    sleep 60
done
