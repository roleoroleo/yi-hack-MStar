#!/bin/sh

YI_HACK_PREFIX="/home/yi-hack"

killall wsd_simple_server
killall onvif_simple_server
killall wd_rtsp.sh
killall rRTSPServer
killall rtsp_server_yi
killall h264grabber_l
killall h264grabber_h
killall h264grabber
killall mqtt-config
killall mqttv4
killall ipc_multiplexer
killall proxychains4
killall tcpsvd
killall pure-ftpd
killall mp4record
sleep 1
umount /tmp/sd
sleep 1

mount | grep /tmp/sd > /dev/null
if [ $? -eq 0 ]; then
    printf "Content-type: application/json\r\n\r\n"
    printf "{\n"
    printf "\"%s\":\"%s\"\\n" "error" "true"
    printf "}"
else
    printf "Content-type: application/json\r\n\r\n"
    printf "{\n"
    printf "\"%s\":\"%s\"\\n" "error" "false"
    printf "}"
fi
