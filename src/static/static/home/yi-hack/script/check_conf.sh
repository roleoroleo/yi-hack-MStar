#!/bin/sh

SYSTEM_CONF_FILE="/home/yi-hack/etc/system.conf"
CAMERA_CONF_FILE="/home/yi-hack/etc/camera.conf"

PARMS1="
HTTPD=yes
TELNETD=yes
SSHD=yes
FTPD=yes
BUSYBOX_FTPD=no
DISABLE_CLOUD=no
REC_WITHOUT_CLOUD=no
MQTT=no
RTSP=yes
RTSP_STREAM=high
ONVIF=yes
NTPD=yes
NTP_SERVER=pool.ntp.org
PROXYCHAINSNG=no
RTSP_PORT=554
ONVIF_PORT=80
HTTPD_PORT=8080
USERNAME=
PASSWORD=
FREE_SPACE=0"

PARMS2="
SWITCH_ON=yes
SAVE_VIDEO_ON_MOTION=yes
SENSITIVITY=low
LED=no
ROTATE=no
IR=yes"

for i in $PARMS1
do
    if [ ! -z "$i" ]; then
        PAR=$(echo "$i" | cut -d= -f1)
        MATCH=$(cat $SYSTEM_CONF_FILE | grep $PAR)
        if [ -z "$MATCH" ]; then
            echo "$i" >> $SYSTEM_CONF_FILE
        fi
    fi
done

for i in $PARMS2
do
    if [ ! -z "$i" ]; then
        PAR=$(echo "$i" | cut -d= -f1)
        MATCH=$(cat $CAMERA_CONF_FILE | grep $PAR)
        if [ -z "$MATCH" ]; then
            echo "$i" >> $CAMERA_CONF_FILE
        fi
    fi
done
