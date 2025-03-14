#!/bin/sh

CONF_FILE="etc/system.conf"
CAMERA_CONF_FILE="etc/camera.conf"

YI_HACK_PREFIX="/home/yi-hack"
MODEL_SUFFIX=$(cat /home/yi-hack/model_suffix)

START_STOP_SCRIPT=$YI_HACK_PREFIX/script/service.sh

#LOG_FILE="/tmp/sd/wd.log"
LOG_FILE="/dev/null"
LOGWIFI_FILE="/tmp/sd/hack_wififailsafe.log"

COUNTER_H=0
COUNTER_L=0
COUNTER_LIMIT=10
INTERVAL=10

get_camera_config()
{
    key=$1
    grep -w $1 $YI_HACK_PREFIX/$CAMERA_CONF_FILE | cut -d "=" -f2
}

get_config()
{
    key=$1
    grep -w $1 $YI_HACK_PREFIX/$CONF_FILE | cut -d "=" -f2
}

restart_rtsp()
{
    $START_STOP_SCRIPT rtsp start
}

check_rtsp()
{
    if [[ $(get_camera_config SWITCH_ON) == "yes" ]] ; then
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
    else
        echo "Camera is swiched off no rtsp restart needed" >> $LOG_FILE
    fi
}

check_rtsp_alt()
{
    if [[ $(get_camera_config SWITCH_ON) == "yes" ]] ; then
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
    else
        echo "Camera is swiched off no rtsp restart needed" >> $LOG_FILE
    fi
}

check_rtsp_go2rtc()
{
    if [[ $(get_camera_config SWITCH_ON) == "yes" ]] ; then
        #  echo "$(date +'%Y-%m-%d %H:%M:%S') - Checking RTSP process..." >> $LOG_FILE
        LISTEN=`netstat -an 2>&1 | grep ":$RTSP_PORT_NUMBER " | grep LISTEN | grep -c ^`
        CPU_1_L=`top -b -n 2 -d 1 | grep h264grabber_l | grep -v grep | tail -n 1 | awk '{print $8}'`
        CPU_1_H=`top -b -n 2 -d 1 | grep h264grabber_h | grep -v grep | tail -n 1 | awk '{print $8}'`
        CPU_2=`top -b -n 2 -d 1 | grep go2rtc | grep -v grep | tail -n 1 | awk '{print $8}'`

        if [ $LISTEN -eq 0 ]; then
            echo "$(date +'%Y-%m-%d %H:%M:%S') - Restarting rtsp process" >> $LOG_FILE
            killall -q go2rtc
            killall -q h264grabber_l
            killall -q h264grabber_h
            sleep 1
            restart_rtsp
        fi
        if [[ $(get_config RTSP_STREAM) == "low" ]] || [[ $(get_config RTSP_STREAM) == "both" ]]; then
            if [ "$CPU_1_L" == "" ] || [ "$CPU_2" == "" ]; then
                echo "$(date +'%Y-%m-%d %H:%M:%S') - No running processes for low res, restarting..." >> $LOG_FILE
                killall -q go2rtc
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
                killall -q go2rtc
                killall -q h264grabber_l
                killall -q h264grabber_h
                sleep 1
                restart_rtsp
            fi
            COUNTER_H=0
        fi
    else
        echo "Camera is swiched off no rtsp restart needed" >> $LOG_FILE
    fi
}

check_rmm()
{
#  echo "$(date +'%Y-%m-%d %H:%M:%S') - Checking rmm process..." >> $LOG_FILE
    PS=`ps | grep rmm | grep -v grep | grep -c ^`

    if [ $PS -eq 0 ]; then
        echo "check_rmm failed, reboot!" >> $LOG_FILE
        sync
        reboot -f
    fi
}

check_mqtt()
{
#  echo "$(date +'%Y-%m-%d %H:%M:%S') - Checking mqttv4 process..." >> $LOG_FILE
    PS=`ps ww | grep mqttv4 | grep -v grep | grep -c ^`

    if [ $PS -eq 0 ]; then
        echo "check_mqtt failed, restart it!" >> $LOG_FILE
        $START_STOP_SCRIPT mqtt start
    fi
}

check_wifi()
{
    if ! wpa_cli -i wlan0 status 2>&1 | grep -q "wpa_state=COMPLETED"; then
        if [ -e "$LOGFILE" ]; then
            /usr/bin/tail -n 145 "$LOGFILE" > "$LOGFILE.tmp" && mv "$LOGFILE.tmp" "$LOGFILE"
        fi
        echo -e "$(date): Wifi connection lost:\n$(wpa_cli -i wlan0 status 2>&1)" >> "$LOGFILE"
        failsafecounter=$((failsafecounter + 1))
        if [ "$failsafecounter" -ge 6 ]; then
            echo -e "$(date): Wifi connection still could't be restored. Restarting." >> "$LOGFILE"
            sync
            reboot -f
        else
            echo -e "$(date): Attempting reconnect." >> "$LOGFILE"
            sleep 2
            ifconfig wlan0 down
            sleep 1
            ifconfig wlan0 up
            sleep 1
            wpa_cli -i wlan0 reconfigure
        fi
    fi
}

if [[ $(get_config RTSP) == "no" ]] ; then
    exit
fi

case $(get_config RTSP_PORT) in
    ''|*[!0-9]*) RTSP_PORT=554 ;;
    *) RTSP_PORT=$(get_config RTSP_PORT) ;;
esac

if [ ! -z $RTSP_PORT ]; then
    RTSP_PORT_NUMBER=$RTSP_PORT
fi

RTSP_ALT=$(get_config RTSP_ALT)

echo "$(date +'%Y-%m-%d %H:%M:%S') - Starting RTSP watchdog..." >> $LOG_FILE

while true
do
    if [[ "$RTSP_ALT" == "standard" ]] ; then
        check_rtsp
    elif [[ "$RTSP_ALT" == "alternative" ]] ; then
        check_rtsp_alt
    else
        check_rtsp_go2rtc
    fi
    check_rmm
    check_mqtt
    check_wifi

    echo 1500 > /sys/class/net/eth0/mtu
    echo 1500 > /sys/class/net/wlan0/mtu

    if [ $COUNTER_H -eq 0 ] && [ $COUNTER_L -eq 0 ]; then
        sleep $INTERVAL
    fi
done
