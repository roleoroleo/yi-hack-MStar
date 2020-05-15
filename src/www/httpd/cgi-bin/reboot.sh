#!/bin/sh

printf "Content-type: application/json\r\n\r\n"
printf "{\n"
printf "}\n"

# Yeah, it's pretty ugly.. but hey, it works.

sync
sync
sync
# Kill httpd otherwise reboot command truncates the TCP session
killall httpd
sleep 1
reboot -f
