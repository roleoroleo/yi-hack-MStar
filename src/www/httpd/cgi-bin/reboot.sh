#!/bin/sh

printf "Content-type: application/json\r\n\r\n"
printf "{\n"
printf "\"%s\":\"%s\"\\n" "error" "false"
printf "}\n"

# Yeah, it's pretty ugly.. but hey, it works.

sync
sync
sync
# Kill httpd otherwise reboot command truncates the TCP session
killall httpd
# Kill mqtt to drop mqtt connection asap
killall mqttv4
sleep 1
reboot -f
