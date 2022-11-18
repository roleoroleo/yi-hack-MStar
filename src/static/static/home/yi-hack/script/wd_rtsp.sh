#!/bin/sh

CONF_FILE="etc/system.conf"

YI_HACK_PREFIX="/home/yi-hack"
MODEL_SUFFIX=$(cat /home/yi-hack/model_suffix)

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
            h264grabber_l -m $MODEL_SUFFIX -r low -f &
            sleep 1
            CODEC_LOW=$(cat /tmp/lowres)
            if [ ! -z $CODEC_LOW ]; then
                CODEC_LOW="-c "$CODEC_LOW
            fi
            $RTSP_DAEMON -r low $CODEC_LOW $RTSP_AUDIO_COMPRESSION $RTSP_PORT $RTSP_USER $RTSP_PASSWORD $NR_LEVEL &
        fi
        if [[ $(get_config RTSP_STREAM) == "high" ]]; then
            h264grabber_h -m $MODEL_SUFFIX -r high -f &
            sleep 1
            CODEC_HIGH=$(cat /tmp/highres)
            if [ ! -z $CODEC_HIGH ]; then
                CODEC_HIGH="-C "$CODEC_HIGH
            fi
            $RTSP_DAEMON -r high $CODEC_HIGH $RTSP_AUDIO_COMPRESSION $RTSP_PORT $RTSP_USER $RTSP_PASSWORD $NR_LEVEL &
        fi
        if [[ $(get_config RTSP_STREAM) == "both" ]]; then
            h264grabber_l -m $MODEL_SUFFIX -r low -f &
            h264grabber_h -m $MODEL_SUFFIX -r high -f &
            sleep 1
            CODEC_LOW=$(cat /tmp/lowres)
            if [ ! -z $CODEC_LOW ]; then
                CODEC_LOW="-c "$CODEC_LOW
            fi
            CODEC_HIGH=$(cat /tmp/highres)
            if [ ! -z $CODEC_HIGH ]; then
                CODEC_HIGH="-C "$CODEC_HIGH
            fi
            $RTSP_DAEMON -r both $CODEC_LOW $CODEC_HIGH $RTSP_AUDIO_COMPRESSION $RTSP_PORT $RTSP_USER $RTSP_PASSWORD $NR_LEVEL &
        fi
    fi
}

check_rtsp()
{
#  echo "$(date +'%Y-%m-%d %H:%M:%S') - Checking RTSP process..." >> $LOG_FILE
    LISTEN=`netstat -an 2>&1 | grep ":$RTSP_PORT_NUMBER " | grep LISTEN | grep -c ^`
    SOCKET=`netstat -an 2>&1 | grep ":$RTSP_PORT_NUMBER " | grep ESTABLISHED | grep -c ^`
    CPU_1_L=`top -b -n 2 -d 1 | grep h264grabber_l | grep -v grep | tail -n 1 | awk '{print $8}'`
    CPU_1_H=`top -b -n 2 -d 1 | grep h264grabber_h | grep -v grep | tail -n 1 | awk '{print $8}'`
    CPU_2=`top -b -n 2 -d 1 | grep rRTSPServer | grep -v grep | tail -n 1 | awk '{print $8}'`

    if [ $LISTEN -eq 0 ]; then
        echo "$(date +'%Y-%m-%d %H:%M:%S') - Restarting rtsp process" >> $LOG_FILE
        killall -q rRTSPServer
        killall -q h264grabber_l
        killall -q h264grabber_h
        sleep 1
        restart_rtsp
    fi
    if [[ $(get_config RTSP_STREAM) == "low" ]] || [[ $(get_config RTSP_STREAM) == "both" ]]; then
        if [ "$CPU_1_L" == "" ] || [ "$CPU_2" == "" ]; then
            echo "$(date +'%Y-%m-%d %H:%M:%S') - No running processes for low res, restarting..." >> $LOG_FILE
            killall -q rRTSPServer
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
            killall -q rRTSPServer
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
                killall -q rRTSPServer
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
                killall -q rRTSPServer
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

check_rtsp_alt()
{
#  echo "$(date +'%Y-%m-%d %H:%M:%S') - Checking RTSP process..." >> $LOG_FILE
    LISTEN=`netstat -an 2>&1 | grep ":$RTSP_PORT_NUMBER " | grep LISTEN | grep -c ^`
    CPU_1_L=`top -b -n 2 -d 1 | grep h264grabber_l | grep -v grep | tail -n 1 | awk '{print $8}'`
    CPU_1_H=`top -b -n 2 -d 1 | grep h264grabber_h | grep -v grep | tail -n 1 | awk '{print $8}'`
    CPU_2=`top -b -n 2 -d 1 | grep rtsp_server_yi | grep -v grep | tail -n 1 | awk '{print $8}'`

    if [ $LISTEN -eq 0 ]; then
        echo "$(date +'%Y-%m-%d %H:%M:%S') - Restarting rtsp process" >> $LOG_FILE
        killall -q rtsp_server_yi
        killall -q h264grabber_l
        killall -q h264grabber_h
        sleep 1
        restart_rtsp
    fi
    if [[ $(get_config RTSP_STREAM) == "low" ]] || [[ $(get_config RTSP_STREAM) == "both" ]]; then
        if [ "$CPU_1_L" == "" ] || [ "$CPU_2" == "" ]; then
            echo "$(date +'%Y-%m-%d %H:%M:%S') - No running processes for low res, restarting..." >> $LOG_FILE
            killall -q rtsp_server_yi
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
            killall -q rtsp_server_yi
            killall -q h264grabber_l
            killall -q h264grabber_h
            sleep 1
            restart_rtsp
        fi
        COUNTER_H=0
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

RTSP_DAEMON="rRTSPServer"
RTSP_AUDIO_COMPRESSION=$(get_config RTSP_AUDIO)
NR_LEVEL=$(get_config RTSP_AUDIO_NR_LEVEL)

if [[ $(get_config RTSP_ALT) == "yes" ]] ; then
    RTSP_DAEMON="rtsp_server_yi"
    if [[ "$RTSP_AUDIO_COMPRESSION" == "aac" ]] ; then
        RTSP_AUDIO_COMPRESSION="alaw"
    fi
    NR_LEVEL=""
fi

if [[ "$RTSP_AUDIO_COMPRESSION" == "none" ]] ; then
    RTSP_AUDIO_COMPRESSION="no"
fi
if [ ! -z $RTSP_AUDIO_COMPRESSION ]; then
    RTSP_AUDIO_COMPRESSION="-a "$RTSP_AUDIO_COMPRESSION
fi
if [ ! -z $RTSP_PORT ]; then
    RTSP_PORT_NUMBER=$RTSP_PORT
    RTSP_PORT="-p "$RTSP_PORT
fi
if [ ! -z $USERNAME ]; then
    RTSP_USER="-u "$USERNAME
fi
if [ ! -z $PASSWORD ]; then
    RTSP_PASSWORD="-w "$PASSWORD
fi
if [ ! -z $NR_LEVEL ]; then
    NR_LEVEL="-n "$NR_LEVEL
fi

RTSP_ALT=$(get_config RTSP_ALT)

echo "$(date +'%Y-%m-%d %H:%M:%S') - Starting RTSP watchdog..." >> $LOG_FILE

while true
do
    if [[ "$RTSP_ALT" == "no" ]] ; then
        check_rtsp
    else
        check_rtsp_alt
    fi
    check_rmm

    echo 1500 > /sys/class/net/eth0/mtu
    echo 1500 > /sys/class/net/wlan0/mtu

    if [ $COUNTER_H -eq 0 ] && [ $COUNTER_L -eq 0 ]; then
        sleep $INTERVAL
    fi
done
