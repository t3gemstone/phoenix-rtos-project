#!/bin/ash

sleep 1
ifconfig en1 up 192.168.122.10 netmask 255.255.255.0 
route add default gw 192.168.122.1 en1
#needed for openssl:
openssl rehash /etc/ssl/certs
