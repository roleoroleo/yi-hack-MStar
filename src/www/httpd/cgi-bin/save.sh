#!/bin/sh

printf "Content-type: application/octet-stream\r\n\r\n"

TMP_DIR="/tmp/yi-temp-save"
mkdir $TMP_DIR
cd $TMP_DIR
cp /home/yi-hack/etc/*.conf .
if [ -f /home/yi-hack/etc/TZ ]; then
    cp /home/yi-hack/etc/TZ .
fi
if [ -f /home/yi-hack/etc/hostname ]; then
    cp /home/yi-hack/etc/hostname .
fi
if [ -f /home/yi-hack/etc/passwd ]; then
    cp /home/yi-hack/etc/passwd .
fi
7za a config.7z * > /dev/null 2>&1
cat $TMP_DIR/config.7z
cd /tmp
rm -rf $TMP_DIR
