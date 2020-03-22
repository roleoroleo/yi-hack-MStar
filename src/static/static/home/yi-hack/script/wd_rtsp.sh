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

restart_rtsp_high()
{
    if [[ $(get_config RTSP_HIGH) == "yes" ]]; then
        h264grabber_high -r high | RRTSP_RES=0 RRTSP_PORT=$RTSP_HIGH_PORT RRTSP_USER=$USERNAME RRTSP_PWD=$PASSWORD rRTSPServer_high &
    fi
}        
 
restart_rtsp_low()        
    if [[ $(get_config RTSP_LOW) == "yes" ]]; then
        h264grabber_low -r low | RRTSP_RES=1 RRTSP_PORT=$RTSP_LOW_PORT RRTSP_USER=$USERNAME RRTSP_PWD=$PASSWORD rRTSPServer_low &
    fi
}

check_rtsp_high()
{
#  echo "$(date +'%Y-%m-%d %H:%M:%S') - Checking RTSP process..." >> $LOG_FILE
    SOCKET_H=`netstat -an 2>&1 | grep ":$RTSP_HIGH_PORT " | grep ESTABLISHED | grep -c ^`
    CPU_1_H=`top -b -n 1 -d 1 | grep h264grabber_high | grep -v grep | tail -n 1 | awk '{print $8}'`
    CPU_2_H=`top -b -n 1 -d 1 | grep rRTSPServer_high | grep -v grep | tail -n 1 | awk '{print $8}'`

    if [ $SOCKET_H -eq 0 ]; then
        if [ "$CPU_1_H" == "" ] || [ "$CPU_2_H" == "" ]; then
            echo "$(date +'%Y-%m-%d %H:%M:%S') - No running processes for high res, restarting..." >> $LOG_FILE
            killall -q rRTSPServer_high
            killall -q h264grabber_high
            sleep 1
            restart_rtsp_high
        fi
        COUNTER_H=0
    fi
    if [ $SOCKET_H -gt 0 ]; then
        if [ "$CPU_1_H" == "0.0" ] && [ "$CPU_2_H" == "0.0" ]; then
            COUNTER_H=$((COUNTER_H+1))
            echo "$(date +'%Y-%m-%d %H:%M:%S') - Detected possible locked process for high res ($COUNTER_H)" >> $LOG_FILE
            if [ $COUNTER_H -ge $COUNTER_LIMIT ]; then
                echo "$(date +'%Y-%m-%d %H:%M:%S') - Restarting processes for high res" >> $LOG_FILE
                killall -q rRTSPServer_high
                killall -q h264grabber_high
                sleep 1
                restart_rtsp_high
                COUNTER_H=0
           fi
        else
            COUNTER_H=0
        fi
    fi
}

check_rtsp_low()
{
#  echo "$(date +'%Y-%m-%d %H:%M:%S') - Checking RTSP process..." >> $LOG_FILE
    SOCKET_L=`netstat -an 2>&1 | grep ":$RTSP_LOW_PORT " | grep ESTABLISHED | grep -c ^`
    CPU_1_L=`top -b -n 1 -d 1 | grep h264grabber_low | grep -v grep | tail -n 1 | awk '{print $8}'`
    CPU_2_L=`top -b -n 1 -d 1 | grep rRTSPServer_low | grep -v grep | tail -n 1 | awk '{print $8}'`

    if [ $SOCKET_L -eq 0 ]; then
        if [ "$CPU_1_L" == "" ] || [ "$CPU_2_L" == "" ]; then
            echo "$(date +'%Y-%m-%d %H:%M:%S') - No running processes for low res, restarting..." >> $LOG_FILE
            killall -q rRTSPServer_low
            killall -q h264grabber_low
            sleep 1
            restart_rtsp_low
        fi
        COUNTER_L=0
    fi
    if [ $SOCKET_L -gt 0 ]; then
        if [ "$CPU_1_L" == "0.0" ] && [ "$CPU_2_L" == "0.0" ]; then
            COUNTER_L=$((COUNTER_L+1))
            echo "$(date +'%Y-%m-%d %H:%M:%S') - Detected possible locked process for low res ($COUNTER_L)" >> $LOG_FILE
            if [ $COUNTER_L -ge $COUNTER_LIMIT ]; then
                echo "$(date +'%Y-%m-%d %H:%M:%S') - Restarting processes for low res" >> $LOG_FILE
                killall -q rRTSPServer_low
                killall -q h264grabber_low
                sleep 1
                restart_rtsp_low
                COUNTER_L=0
            fi
        else
            COUNTER_L=0
        fi
    fi
}


if [[ $(get_config RTSP_HIGH) == "no" ]] || [[ $(get_config RTSP_LOW) == "no" ]]; then
    exit
fi

if [[ "$(get_config USERNAME)" != "" ]] ; then
    USERNAME=$(get_config USERNAME)
    PASSWORD=$(get_config PASSWORD)
fi

case $(get_config RTSP__HIGH_PORT) in
    ''|*[!0-9]*) RTSP_HIGH_PORT=554 ;;
    *) RTSP_HIGH_PORT=$(get_config RTSP_HIGH_PORT) ;;
esac
case $(get_config RTSP_LOW_PORT) in
    ''|*[!0-9]*) RTSP_LOW_PORT=8554 ;;
    *) RTSP_LOW_PORT=$(get_config RTSP_LOW_PORT) ;;
esac

echo "$(date +'%Y-%m-%d %H:%M:%S') - Starting RTSP watchdog..." >> $LOG_FILE

while true
do
    check_rtsp_high
    check_rtsp_low
    if [ $COUNTER_H -eq 0 ] && [ $COUNTER_L -eq 0 ]; then
        sleep $INTERVAL
    fi
done
