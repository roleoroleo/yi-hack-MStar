#!/bin/sh

printf "Content-type: application/octet-stream\r\n\r\n"

7za a /tmp/config.7z /home/yi-hack/etc/*.conf TZ >/dev/null 2>&1
cat /tmp/config.7z
