#!/bin/sh

CONF_FILE="etc/system.conf"
YI_HACK_PREFIX="/home/yi-hack"

get_config()
{
    key=$1
    grep -w $1 $YI_HACK_PREFIX/$CONF_FILE | cut -d "=" -f2
}

start_rtsp()
{
    $YI_HACK_PREFIX/script/wd_rtsp.sh >/dev/null &
}

stop_rtsp()
{
    killall wd_rtsp.sh
    killall rtsp_server_yi
    killall rRTSPServer
    killall h264grabber
}

ps_program()
{
    PS_PROGRAM=$(ps | grep $1 | grep -v grep | grep -c ^)
    if [ $PS_PROGRAM -gt 0 ]; then
        echo "started"
    else
        echo "stopped"
    fi
}

VALUE="none"
RES="none"

if [ $# -ne 1 ]; then
    exit
fi

if [ "$1" == "on" ] || [ "$1" == "yes" ] ; then
    touch /tmp/privacy
    touch /tmp/snapshot.disabled
    stop_rtsp
    killall mp4record
    RES="on"
elif [ "$1" == "off" ] || [ "$1" == "no" ]; then
    rm -f /tmp/snapshot.disabled
    start_rtsp
    if [[ $(get_config DISABLE_CLOUD) == "no" ]] || [[ $(get_config REC_WITHOUT_CLOUD) == "yes" ]] ; then
        cd /home/app
        ./mp4record >/dev/null &
    fi
    rm -f /tmp/privacy
    RES="off"
elif [ "$1" == "status" ] ; then
    if [ -f /tmp/privacy ]; then
        RES="on"
    else
        RES="off"
    fi
fi

if [ ! -z "$RES" ]; then
    echo $RES
fi
