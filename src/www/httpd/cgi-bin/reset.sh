#!/bin/sh

cd /home/yi-hack/etc

rm -f /etc/hostname
rm -f /etc/TZ

rm camera.conf
rm mqttv4.conf
rm proxychains.conf
rm system.conf

7za x defaults.7z > /dev/null

printf "Content-type: application/json\r\n\r\n"
printf "{\n"
printf "}\n"
