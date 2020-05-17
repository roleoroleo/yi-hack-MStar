#!/bin/sh

printf "Content-type: application/octet-stream\r\n\r\n"

TMP_DIR="/tmp/yi-temp-save"
mkdir $TMP_DIR
cd $TMP_DIR
cp /home/yi-hack/etc/*.conf .
if [ -f /etc/TZ ]; then
    cp /etc/TZ .
fi
if [ -f /etc/hostname ]; then
    cp /etc/hostname .
fi
7za a config.7z * >/dev/null 2>&1
cat $TMP_DIR/config.7z
cd /tmp
rm -rf $TMP_DIR
