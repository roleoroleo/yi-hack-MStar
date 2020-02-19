#!/bin/sh

CONF_FILE="etc/system.conf"

YI_HACK_PREFIX="/home/yi-hack"

#LOG_FILE="/tmp/sd/wd_rtsp.log"
LOG_FILE="/dev/null"

COUNTER=0
COUNTER_LIMIT=5
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
        if [[ $(get_config RTSP) == "yes" ]] ; then
                if [[ $(get_config RTSP_HIGH) == "yes" ]] ; then
                        h264grabber -r high | RRTSP_RES=0 RRTSP_PORT=$RTSP_PORT RRTSP_USER=$USERNAME RRTSP_PWD=$PASSWORD rRTSPServer &
                else
                        h264grabber -r low | RRTSP_RES=1 RRTSP_PORT=$RTSP_PORT RRTSP_USER=$USERNAME RRTSP_PWD=$PASSWORD rRTSPServer &
                fi
        fi
}

check_rtsp()
{
#       echo "$(date +'%Y-%m-%d %H:%M:%S') - Checking RTSP process..." >> $LOG_FILE
        SOCKET=`netstat -an 2>&1 | grep ":554 " | grep ESTABLISHED | grep -c ^`
        CPU_1=`top -b -n 1 | grep h264grabber | grep -v grep | awk '{print $8}'`
        CPU_2=`top -b -n 1 | grep rRTSPServer | grep -v grep | awk '{print $8}'`

        if [ $SOCKET -eq 0 ]; then
                if [ "$CPU_1" == "" ] || [ "$CPU_2" == "" ]; then
                        echo "$(date +'%Y-%m-%d %H:%M:%S') - No running process, restarting..." >> $LOG_FILE
                        killall -q rRTSPServer
                        killall -q h264grabber
                        sleep 1
                        restart_rtsp
                fi
                COUNTER=0
        fi
        if [ $SOCKET -gt 0 ]; then
                if [ "$CPU_1" == "0.0" ] && [ "$CPU_2" == "0.0" ]; then
                        COUNTER=$((COUNTER+1))
                        echo "$(date +'%Y-%m-%d %H:%M:%S') - Detected possible locked process ($COUNTER)" >> $LOG_FILE
                        if [ $COUNTER -ge $COUNTER_LIMIT ]; then
                                echo "$(date +'%Y-%m-%d %H:%M:%S') - Restarting process" >> $LOG_FILE
                                killall -q rRTSPServer
                                killall -q h264grabber
                                sleep 1
                                restart_rtsp
                                COUNTER=0
                        fi
                else
                        COUNTER=0
                fi
        fi
}

if [[ x$(get_config USERNAME) != "x" ]] ; then
    USERNAME=$(get_config USERNAME)
    PASSWORD=$(get_config PASSWORD)
fi

case $(get_config RTSP_PORT) in
    ''|*[!0-9]*) RTSP_PORT=554 ;;
    *) RTSP_PORT=$(get_config RTSP_PORT) ;;
esac

echo "$(date +'%Y-%m-%d %H:%M:%S') - Starting RTSP watchdog..." >> $LOG_FILE

while true
do
        check_rtsp
        if [ $COUNTER -eq 0 ]; then
                sleep $INTERVAL
        else
                sleep 1
        fi
done
