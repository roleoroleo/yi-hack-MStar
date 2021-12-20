#!/bin/sh

cd /home/yi-hack/etc

rm -f hostname
rm -f TZ
rm -f passwd

rm camera.conf
rm mqttv4.conf
rm proxychains.conf
rm system.conf

7za x defaults.7z > /dev/null

printf "Content-type: application/json\r\n\r\n"
printf "{\n"
printf "\"%s\":\"%s\"\\n" "error" "false"
printf "}\n"
