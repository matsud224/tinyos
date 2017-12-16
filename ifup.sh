#!/bin/sh
/sbin/ip addr add 192.168.4.1/24 dev $1
/sbin/ip link set $1 up

