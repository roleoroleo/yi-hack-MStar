#!/bin/sh

CONF_FILE="etc/system.conf"

YI_HACK_PREFIX="/home/yi-hack"

#LOG_FILE="/tmp/sd/wd_rtsp.log"
LOG_FILE="/dev/null"

COUNTER_H=0
COUNTER_L=0
COUNTER_LIMIT=10
INTERVAL=10

get_config()
{
    key=$1
    grep -w $1 $YI_HACK_PREFIX/$CONF_FILE | cut -d "=" -f2
}

restart_rtsp()
{
    if [[ "$1" == "low" ]]; then
        if [[ $(get_config RTSP_STREAM) == "low" ]] || [[ $(get_config RTSP_STREAM) == "both" ]]; then
            h264grabber_l -r low | RRTSP_RES=1 RRTSP_PORT=$RTSP_SUB_PORT RRTSP_USER=$USERNAME RRTSP_PWD=$PASSWORD $RTSP_L_EXE &
        fi
    elif [[ "$1" == "high" ]]; then
        if [[ $(get_config RTSP_STREAM) == "high" ]] || [[ $(get_config RTSP_STREAM) == "both" ]]; then
            h264grabber_h -r high | RRTSP_RES=0 RRTSP_PORT=$RTSP_PORT RRTSP_USER=$USERNAME RRTSP_PWD=$PASSWORD $RTSP_H_EXE &
        fi
    fi
}

check_rtsp()
{
#  echo "$(date +'%Y-%m-%d %H:%M:%S') - Checking RTSP process..." >> $LOG_FILE
    SOCKET_L=`netstat -an 2>&1 | grep ":$RTSP_SUB_PORT " | grep ESTABLISHED | grep -c ^`
    CPU_1_L=`top -b -n 2 -d 1 | grep h264grabber_l | grep -v grep | tail -n 1 | awk '{print $8}'`
    CPU_2_L=`top -b -n 2 -d 1 | grep $RTSP_L_EXE | grep -v grep | tail -n 1 | awk '{print $8}'`

    SOCKET_H=`netstat -an 2>&1 | grep ":$RTSP_PORT " | grep ESTABLISHED | grep -c ^`
    CPU_1_H=`top -b -n 2 -d 1 | grep h264grabber_h | grep -v grep | tail -n 1 | awk '{print $8}'`
    CPU_2_H=`top -b -n 2 -d 1 | grep $RTSP_H_EXE | grep -v grep | tail -n 1 | awk '{print $8}'`

    if [ $SOCKET_L -eq 0 ]; then
        if [ "$CPU_1_L" == "" ] || [ "$CPU_2_L" == "" ]; then
            echo "$(date +'%Y-%m-%d %H:%M:%S') - No running processes for low res, restarting..." >> $LOG_FILE
            killall -q $RTSP_L_EXE
            killall -q h264grabber_l
            sleep 1
            restart_rtsp "low"
        fi
        COUNTER_L=0
    fi
    if [ $SOCKET_L -gt 0 ]; then
        if [ "$CPU_1_L" == "0.0" ] && [ "$CPU_2_L" == "0.0" ]; then
            COUNTER_L=$((COUNTER_L+1))
            echo "$(date +'%Y-%m-%d %H:%M:%S') - Detected possible locked process for low res ($COUNTER_L)" >> $LOG_FILE
            if [ $COUNTER_L -ge $COUNTER_LIMIT ]; then
                echo "$(date +'%Y-%m-%d %H:%M:%S') - Restarting processes for low res" >> $LOG_FILE
                killall -q $RTSP_L_EXE
                killall -q h264grabber_l
                sleep 1
                restart_rtsp "low"
                COUNTER_L=0
            fi
        else
            COUNTER_L=0
        fi
    fi

    if [ $SOCKET_H -eq 0 ]; then
        if [ "$CPU_1_H" == "" ] || [ "$CPU_2_H" == "" ]; then
            echo "$(date +'%Y-%m-%d %H:%M:%S') - No running processes for high res, restarting..." >> $LOG_FILE
            killall -q $RTSPS_H_EXE
            killall -q h264grabber_h
            sleep 1
            restart_rtsp "high"
        fi
        COUNTER_H=0
    fi
    if [ $SOCKET_H -gt 0 ]; then
        if [ "$CPU_1_H" == "0.0" ] && [ "$CPU_2_H" == "0.0" ]; then
            COUNTER_H=$((COUNTER_H+1))
            echo "$(date +'%Y-%m-%d %H:%M:%S') - Detected possible locked process for high res ($COUNTER_H)" >> $LOG_FILE
            if [ $COUNTER_H -ge $COUNTER_LIMIT ]; then
                echo "$(date +'%Y-%m-%d %H:%M:%S') - Restarting processes for high res" >> $LOG_FILE
                killall -q $RTSPS_H_EXE
                killall -q h264grabber_h
                sleep 1
                restart_rtsp "high"
                COUNTER_H=0
           fi
        else
            COUNTER_H=0
        fi
    fi
}

check_rmm()
{
#  echo "$(date +'%Y-%m-%d %H:%M:%S') - Checking rmm process..." >> $LOG_FILE
    PS=`ps | grep rmm | grep -v grep | grep -c ^`

    if [ $PS -eq 0 ]; then
        reboot
    fi
}

if [[ $(get_config RTSP) == "no" ]] ; then
    exit
fi

if [[ "$(get_config USERNAME)" != "" ]] ; then
    USERNAME=$(get_config USERNAME)
    PASSWORD=$(get_config PASSWORD)
fi

case $(get_config RTSP_PORT) in
    ''|*[!0-9]*) RTSP_PORT=554 ;;
    *) RTSP_PORT=$(get_config RTSP_PORT) ;;
esac
case $(get_config RTSP_SUB_PORT) in
    ''|*[!0-9]*) RTSP_SUB_PORT=8554 ;;
    *) RTSP_SUB_PORT=$(get_config RTSP_SUB_PORT) ;;
esac

if [[ $(get_config RTSP_AUDIO) == "none" ]] ; then
    RTSP_L_EXE="rRTSPServer_l"
    RTSP_H_EXE="rRTSPServer_h"
elif [[ $(get_config RTSP_AUDIO) == "low" ]] ; then
    RTSP_L_EXE="rRTSPServer_audio_l"
    RTSP_H_EXE="rRTSPServer_h"
elif [[ $(get_config RTSP_AUDIO) == "high" ]] ; then
    RTSP_L_EXE="rRTSPServer_l"
    RTSP_H_EXE="rRTSPServer_audio_h"
fi

echo "$(date +'%Y-%m-%d %H:%M:%S') - Starting RTSP watchdog..." >> $LOG_FILE

while true
do
    check_rtsp
    check_rmm
    if [ $COUNTER_H -eq 0 ] && [ $COUNTER_L -eq 0 ]; then
        sleep $INTERVAL
    fi
done
