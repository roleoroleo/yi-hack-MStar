#!/bin/sh

cd /home/yi-hack/etc

rm -f hostname
rm -f TZ
rm -f passwd

7za -y x defaults.7z > /dev/null

printf "Content-type: application/json\r\n\r\n"
printf "{\n"
printf "\"%s\":\"%s\"\\n" "error" "false"
printf "}\n"
