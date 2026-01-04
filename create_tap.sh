#!/bin/bash

if [[ $(/usr/bin/id -u) -ne 0 ]]; then
    echo "Script must be run as root"
    exit
fi


TAP_NAME=itask1_tap
TAP_IP_ADDR=192.168.56.1

ip tuntap add $TAP_NAME mode tap user $USER
ip link set $TAP_NAME up

# set ip address for this tap
ip addr add $TAP_NAME/24 dev $TAP_NAME

# use ifconfig if the previous line wouldn't work
# ifconfig $TAP_NAME $TAP_IP_ADDR

