#!/bin/sh

CONF_FILE="etc/system.conf"

YI_HACK_PREFIX="/home/yi-hack"

#LOG_FILE="/tmp/sd/wd_rtsp.log"
LOG_FILE="/dev/null"

COUNTER_H=0
COUNTER_L=0
COUNTER_LIMIT=10
INTERVAL=10

export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:/home/lib:/home/yi-hack/lib:/tmp/sd/yi-hack/lib
export PATH=$PATH:/home/base/tools:/home/yi-hack/bin:/home/yi-hack/sbin:/tmp/sd/yi-hack/bin:/tmp/sd/yi-hack/sbin

get_config()
{
    key=$1
    grep -w $1 $YI_HACK_PREFIX/$CONF_FILE | cut -d "=" -f2
}

restart_rtsp()
{
    if [[ "$1" == "high" ]]; then
        if [[ $(get_config RTSP_STREAM) == "high" ]] || [[ $(get_config RTSP_STREAM) == "both" ]]; then
            h264grabber_h -r high | RRTSP_RES=0 RRTSP_PORT=$RTSP_PORT RRTSP_USER=$USERNAME RRTSP_PWD=$PASSWORD rRTSPServer_h &
        fi
    elif [[ "$1" == "low" ]]; then
        if [[ $(get_config RTSP_STREAM) == "low" ]] || [[ $(get_config RTSP_STREAM) == "both" ]]; then
            h264grabber_l -r low | RRTSP_RES=1 RRTSP_PORT=$RTSP1_PORT RRTSP_USER=$USERNAME RRTSP_PWD=$PASSWORD rRTSPServer_l &
        fi
    fi
}

check_rtsp()
{
#  echo "$(date +'%Y-%m-%d %H:%M:%S') - Checking RTSP process..." >> $LOG_FILE
    SOCKET_H=`netstat -an 2>&1 | grep ":$RTSP_PORT " | grep ESTABLISHED | grep -c ^`
    CPU_1_H=`top -b -n 2 -d 1 | grep h264grabber_h | grep -v grep | tail -n 1 | awk '{print $8}'`
    CPU_2_H=`top -b -n 2 -d 1 | grep rRTSPServer_h | grep -v grep | tail -n 1 | awk '{print $8}'`

    SOCKET_L=`netstat -an 2>&1 | grep ":$RTSP1_PORT " | grep ESTABLISHED | grep -c ^`
    CPU_1_L=`top -b -n 2 -d 1 | grep h264grabber_l | grep -v grep | tail -n 1 | awk '{print $8}'`
    CPU_2_L=`top -b -n 2 -d 1 | grep rRTSPServer_l | grep -v grep | tail -n 1 | awk '{print $8}'`

    if [ $SOCKET_H -eq 0 ]; then
        if [ "$CPU_1_H" == "" ] || [ "$CPU_2_H" == "" ]; then
            echo "$(date +'%Y-%m-%d %H:%M:%S') - No running processes for high res, restarting..." >> $LOG_FILE
            killall -q rRTSPServer_h
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
                killall -q rRTSPServer_h
                killall -q h264grabber_h
                sleep 1
                restart_rtsp "high"
                COUNTER_H=0
           fi
        else
            COUNTER_H=0
        fi
    fi

    if [ $SOCKET_L -eq 0 ]; then
        if [ "$CPU_1_L" == "" ] || [ "$CPU_2_L" == "" ]; then
            echo "$(date +'%Y-%m-%d %H:%M:%S') - No running processes for low res, restarting..." >> $LOG_FILE
            killall -q rRTSPServer_l
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
                killall -q rRTSPServer_l
                killall -q h264grabber_l
                sleep 1
                restart_rtsp "low"
                COUNTER_L=0
            fi
        else
            COUNTER_L=0
        fi
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
case $(get_config RTSP1_PORT) in
    ''|*[!0-9]*) RTSP1_PORT=8554 ;;
    *) RTSP1_PORT=$(get_config RTSP1_PORT) ;;
esac

echo "$(date +'%Y-%m-%d %H:%M:%S') - Starting RTSP watchdog..." >> $LOG_FILE

while true
do
    check_rtsp
    if [ $COUNTER_H -eq 0 ] && [ $COUNTER_L -eq 0 ]; then
        sleep $INTERVAL
    fi
done
