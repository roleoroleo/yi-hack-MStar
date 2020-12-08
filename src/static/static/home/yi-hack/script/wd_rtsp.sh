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
    if [[ $(get_config RTSP) == "yes" ]] ; then
        if [[ $(get_config RTSP_STREAM) == "low" ]]; then
            h264grabber_l -r low -f &
            sleep 1
            NR_LEVEL=$NR_LEVEL RRTSP_RES=1 RRTSP_PORT=$RTSP_PORT RRTSP_USER=$USERNAME RRTSP_PWD=$PASSWORD RRTSP_AUDIO=$RRTSP_AUDIO_COMPRESSION $RTSP_EXE &
        fi
        if [[ $(get_config RTSP_STREAM) == "high" ]]; then
            h264grabber_h -r high -f &
            sleep 1
            NR_LEVEL=$NR_LEVEL RRTSP_RES=0 RRTSP_PORT=$RTSP_PORT RRTSP_USER=$USERNAME RRTSP_PWD=$PASSWORD RRTSP_AUDIO=$RRTSP_AUDIO_COMPRESSION $RTSP_EXE &
        fi
        if [[ $(get_config RTSP_STREAM) == "both" ]]; then
            h264grabber_l -r low -f &
            h264grabber_h -r high -f &
            sleep 1
            NR_LEVEL=$NR_LEVEL RRTSP_RES=2 RRTSP_PORT=$RTSP_PORT RRTSP_USER=$USERNAME RRTSP_PWD=$PASSWORD RRTSP_AUDIO=$RRTSP_AUDIO_COMPRESSION $RTSP_EXE &
        fi
    fi
}

check_rtsp()
{
#  echo "$(date +'%Y-%m-%d %H:%M:%S') - Checking RTSP process..." >> $LOG_FILE
    SOCKET=`netstat -an 2>&1 | grep ":$RTSP_PORT " | grep ESTABLISHED | grep -c ^`
    CPU_1_L=`top -b -n 2 -d 1 | grep h264grabber_l | grep -v grep | tail -n 1 | awk '{print $8}'`
    CPU_1_H=`top -b -n 2 -d 1 | grep h264grabber_h | grep -v grep | tail -n 1 | awk '{print $8}'`
    CPU_2=`top -b -n 2 -d 1 | grep $RTSP_EXE | grep -v grep | tail -n 1 | awk '{print $8}'`

    if [[ $(get_config RTSP_STREAM) == "low" ]] || [[ $(get_config RTSP_STREAM) == "both" ]]; then
        if [ "$CPU_1_L" == "" ] || [ "$CPU_2" == "" ]; then
            echo "$(date +'%Y-%m-%d %H:%M:%S') - No running processes for low res, restarting..." >> $LOG_FILE
            killall -q $RTSP_EXE
            killall -q h264grabber_l
            killall -q h264grabber_h
            sleep 1
            restart_rtsp
        fi
        COUNTER_L=0
    fi
    if [[ $(get_config RTSP_STREAM) == "high" ]] || [[ $(get_config RTSP_STREAM) == "both" ]]; then
        if [ "$CPU_1_H" == "" ] || [ "$CPU_2" == "" ]; then
            echo "$(date +'%Y-%m-%d %H:%M:%S') - No running processes for high res, restarting..." >> $LOG_FILE
            killall -q $RTSP_EXE
            killall -q h264grabber_l
            killall -q h264grabber_h
            sleep 1
            restart_rtsp
        fi
        COUNTER_H=0
    fi
    if [ $SOCKET -gt 0 ]; then
        if [ "$CPU_1_L" == "0.0" ] && [ "$CPU_2" == "0.0" ]; then
            COUNTER_L=$((COUNTER_L+1))
            echo "$(date +'%Y-%m-%d %H:%M:%S') - Detected possible locked process for low res ($COUNTER_L)" >> $LOG_FILE
            if [ $COUNTER_L -ge $COUNTER_LIMIT ]; then
                echo "$(date +'%Y-%m-%d %H:%M:%S') - Restarting processes" >> $LOG_FILE
                killall -q $RTSP_EXE
                killall -q h264grabber_l
                killall -q h264grabber_h
                sleep 1
                restart_rtsp
                COUNTER_L=0
            fi
        else
            COUNTER_L=0
        fi

        if [ "$CPU_1_H" == "0.0" ] && [ "$CPU_2" == "0.0" ]; then
            COUNTER_H=$((COUNTER_H+1))
            echo "$(date +'%Y-%m-%d %H:%M:%S') - Detected possible locked process for high res ($COUNTER_H)" >> $LOG_FILE
            if [ $COUNTER_H -ge $COUNTER_LIMIT ]; then
                echo "$(date +'%Y-%m-%d %H:%M:%S') - Restarting processes" >> $LOG_FILE
                killall -q $RTSP_EXE
                killall -q h264grabber_l
                killall -q h264grabber_h
                sleep 1
                restart_rtsp
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
        sync
        reboot -f
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

if [[ $(get_config RTSP_AUDIO) == "no" ]] || [[ $(get_config RTSP_AUDIO) == "none" ]] ; then
    RTSP_EXE="rRTSPServer"
else
    RTSP_EXE="rRTSPServer_audio"
    if [[ $(get_config RTSP_AUDIO) == "alaw" ]] ; then
        RRTSP_AUDIO_COMPRESSION="alaw"
    elif [[ $(get_config RTSP_AUDIO) == "ulaw" ]] ; then
        RRTSP_AUDIO_COMPRESSION="ulaw"
    elif [[ $(get_config RTSP_AUDIO) == "pcm" ]] ; then
        RRTSP_AUDIO_COMPRESSION="pcm"
    fi
fi

NR_LEVEL=$(get_config RTSP_AUDIO_NR_LEVEL)

echo "$(date +'%Y-%m-%d %H:%M:%S') - Starting RTSP watchdog..." >> $LOG_FILE

while true
do
    check_rtsp
    check_rmm
    if [ $COUNTER_H -eq 0 ] && [ $COUNTER_L -eq 0 ]; then
        sleep $INTERVAL
    fi
done
