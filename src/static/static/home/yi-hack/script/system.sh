#!/bin/sh

CONF_FILE="etc/system.conf"

YI_HACK_PREFIX="/home/yi-hack"

get_config()
{
    key=$1
    grep -w $1 $YI_HACK_PREFIX/$CONF_FILE | cut -d "=" -f2
}

export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:/home/lib:/home/yi-hack/lib:/tmp/sd/yi-hack/lib
export PATH=$PATH:/home/base/tools:/home/yi-hack/bin:/home/yi-hack/sbin:/tmp/sd/yi-hack/bin:/tmp/sd/yi-hack/sbin

ulimit -s 1024
hostname -F /etc/hostname

if [[ $(get_config DISABLE_CLOUD) == "no" ]] ; then
    (
        cd /home/app
        sleep 2
        ./mp4record &
        ./cloud &
        ./p2p_tnp &
        ./oss &
        ./watch_process &
    )
else
    (
        cd /home/app
        sleep 2
        ./mp4record &
        # Trick to start circular buffer filling
        ./cloud &
        IDX=`hexdump -n 16 /dev/fshare_frame_buf | awk 'NR==1{print $8}'`
        N=0
        while [ "$IDX" -eq "0000" ] && [ $N -lt 50 ]; do
            IDX=`hexdump -n 16 /dev/fshare_frame_buf | awk 'NR==1{print $8}'`
            N=$[$N+1]
            sleep 0.2
        done
        killall cloud
        if [[ $(get_config REC_WITHOUT_CLOUD) == "no" ]] ; then
            killall mp4record
        fi
    )
fi

if [[ $(get_config HTTPD) == "yes" ]] ; then
    httpd -p 80 -h $YI_HACK_PREFIX/www/
fi

if [[ $(get_config TELNETD) == "yes" ]] ; then
    telnetd
fi

if [[ $(get_config FTPD) == "yes" ]] ; then
    if [[ $(get_config BUSYBOX_FTPD) == "yes" ]] ; then
        tcpsvd -vE 0.0.0.0 21 ftpd -w &
    else
        pure-ftpd -B
    fi
fi

if [[ $(get_config SSHD) == "yes" ]] ; then
    dropbear -R
fi

if [[ $(get_config NTPD) == "yes" ]] ; then
    # Wait until all the other processes have been initialized
    sleep 5 && ntpd -p $(get_config NTP_SERVER) &
fi

if [[ $(get_config MQTT) == "yes" ]] ; then
    mqttv4 &
fi

if [[ $(get_config RTSP) == "yes" ]] ; then
    if [[ $(get_config RTSP_HIGH) == "yes" && $(get_config RTSP_LOW) == "yes" ]] ; then
        RRTSP_RES=2 rRTSPServer &
    elif [[ $(get_config RTSP_LOW) == "yes" ]] ; then
        RRTSP_RES=1 rRTSPServer &
    elif [[ $(get_config RTSP_HIGH) == "yes" ]] ; then
        RRTSP_RES=0 rRTSPServer &
    fi
fi

if [ -f "/tmp/sd/yi-hack/startup.sh" ]; then
    /tmp/sd/yi-hack/startup.sh
fi
