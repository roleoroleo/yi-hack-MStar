#!/bin/sh

CONF_FILE="etc/system.conf"

YI_PREFIX="/home/app"
YI_HACK_PREFIX="/home/yi-hack"
START_STOP_SCRIPT=$YI_HACK_PREFIX/script/service.sh

YI_HACK_VER=$(cat /home/yi-hack/version)
MODEL_SUFFIX=$(cat /home/yi-hack/model_suffix)

get_config()
{
    key=$1
    grep -w $1 $YI_HACK_PREFIX/$CONF_FILE | cut -d "=" -f2
}

start_buffer()
{
    # Trick to start circular buffer filling
    ./cloud &
    IDX=`hexdump -n 16 /dev/fshare_frame_buf | awk 'NR==1{print $8}'`
    N=0
    while [ "$IDX" -eq "0000" ] && [ $N -lt 60 ]; do
        IDX=`hexdump -n 16 /dev/fshare_frame_buf | awk 'NR==1{print $8}'`
        N=$(($N+1))
        sleep 0.2
    done
    killall cloud
    ipc_cmd -x
}

log()
{
    if [ "$DEBUG_LOG" == "yes" ]; then
        echo $1 >> /tmp/sd/hack_debug.log

        if [ "$2" == "1" ]; then
            echo "" >> /tmp/sd/hack_debug.log
            ps >> /tmp/sd/hack_debug.log
            echo "" >> /tmp/sd/hack_debug.log
            free >> /tmp/sd/hack_debug.log
            echo "" >> /tmp/sd/hack_debug.log
        fi
    fi
}

DEBUG_LOG=$(get_config DEBUG_LOG)
rm -f /tmp/sd/hack_debug.log

log "Starting system.sh"

export PATH=$PATH:/home/base/tools:/home/yi-hack/bin:/home/yi-hack/sbin:/tmp/sd/yi-hack/bin:/tmp/sd/yi-hack/sbin
export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:/home/lib:/home/yi-hack/lib:/tmp/sd/yi-hack/lib

ulimit -s 1024

if [ -d /sys/class/net/eth0/ ]; then
    echo 1500 > /sys/class/net/eth0/mtu
fi
if [ -d /sys/class/net/wlan0/ ]; then
    echo 1500 > /sys/class/net/wlan0/mtu
fi

mount --bind /home/yi-hack/script/wifidhcp.sh /home/app/script/wifidhcp.sh
mount --bind /home/yi-hack/script/ethdhcp.sh /home/app/script/ethdhcp.sh

# Remove core files, if any
rm -f $YI_PREFIX/core
rm -f $YI_HACK_PREFIX/bin/core
rm -f $YI_HACK_PREFIX/script/core
rm -f $YI_HACK_PREFIX/www/core
rm -f $YI_HACK_PREFIX/www/cgi-bin/core
rm -f $YI_HACK_PREFIX/core

if [ ! -L /home/yi-hack-v4 ]; then
    ln -s $YI_HACK_PREFIX /home/yi-hack-v4
fi

touch /tmp/httpd.conf

# Restore configuration after a firmware upgrade
if [ -f $YI_HACK_PREFIX/.fw_upgrade_in_progress ]; then
    for f in `ls /tmp/sd/.fw_upgrade/*.conf`; do
        fb="${f##*/}"
        if [ "$fb" == "proxychains.conf" ]; then
            cat /tmp/sd/.fw_upgrade/$fb > $YI_HACK_PREFIX/etc/$fb
        else
            cp -f /tmp/sd/.fw_upgrade/$fb $YI_HACK_PREFIX/etc/
        fi
    done
    chmod 0644 $YI_HACK_PREFIX/etc/*.conf
    if [ -f /tmp/sd/.fw_upgrade/hostname ]; then
        cp -f /tmp/sd/.fw_upgrade/hostname $YI_HACK_PREFIX/etc/
        chmod 0644 $YI_HACK_PREFIX/etc/hostname
    fi
    if [ -f /tmp/sd/.fw_upgrade/TZ ]; then
        cp -f /tmp/sd/.fw_upgrade/TZ $YI_HACK_PREFIX/etc/
        chmod 0644 $YI_HACK_PREFIX/etc/TZ
    fi
    if [ -f /tmp/sd/.fw_upgrade/passwd ]; then
        cp -f /tmp/sd/.fw_upgrade/passwd $YI_HACK_PREFIX/etc/
        chmod 0644 $YI_HACK_PREFIX/etc/passwd
    fi
    rm $YI_HACK_PREFIX/.fw_upgrade_in_progress
fi

$YI_HACK_PREFIX/script/check_conf.sh

hostname -F $YI_HACK_PREFIX/etc/hostname

if [ -f $YI_HACK_PREFIX/etc/TZ ]; then
    TZ_TMP=`cat $YI_HACK_PREFIX/etc/TZ`
fi

if [[ x$(get_config USERNAME) != "x" ]] ; then
    log "Setting username and password"
    USERNAME=$(get_config USERNAME)
    PASSWORD=$(get_config PASSWORD)
    RTSP_USERPWD=$USERNAME:$PASSWORD@
    ONVIF_USERPWD="user=$USERNAME\npassword=$PASSWORD"
    echo "/onvif::" > /tmp/httpd.conf
    echo "/:$USERNAME:$PASSWORD" >> /tmp/httpd.conf
    chmod 0600 /tmp/httpd.conf
fi

PASSWORD_MD5='$1$$qRPK7m23GJusamGpoGLby/'
if [[ x$(get_config SSH_PASSWORD) != "x" ]] ; then
    log "Setting SSH password"
    SSH_PASSWORD=$(get_config SSH_PASSWORD)
    PASSWORD_MD5="$(echo "${SSH_PASSWORD}" | mkpasswd --method=MD5 --stdin)"
fi
CUR_PASSWORD_MD5=$(awk -F":" '$1 != "" { print $2 } ' $YI_HACK_PREFIX/etc/passwd)
if [[ x$CUR_PASSWORD_MD5 != x$PASSWORD_MD5 ]] ; then
    sed -i 's|^\(root:\)[^:]*:|root:'${PASSWORD_MD5}':|g' "$YI_HACK_PREFIX/etc/passwd"
fi

case $(get_config RTSP_PORT) in
    ''|*[!0-9]*) RTSP_PORT=554 ;;
    *) RTSP_PORT=$(get_config RTSP_PORT) ;;
esac
case $(get_config HTTPD_PORT) in
    ''|*[!0-9]*) HTTPD_PORT=80 ;;
    *) HTTPD_PORT=$(get_config HTTPD_PORT) ;;
esac

log "Starting yi processes" 1
if [[ $(get_config DISABLE_CLOUD) == "no" ]] ; then
    (
        if [ $(get_config RTSP_AUDIO) != "no" ]; then
            touch /tmp/audio_fifo.requested
            touch /tmp/audio_fifo_mono_8khz.requested
        fi
        if [ $(get_config SPEAKER_AUDIO) != "no" ]; then
            touch /tmp/audio_in_fifo.requested
        fi
        cd /home/app
        killall dispatch
        LD_PRELOAD=/home/yi-hack/lib/ipc_multiplex.so ./dispatch &
        sleep 3
        if [ $(get_config TIME_OSD) == "yes" ]; then
            # Enable time osd
            set_tz_offset -c osd -o on
        fi
        LD_LIBRARY_PATH="/home/yi-hack/lib:/lib:/home/lib:/home/ms:/home/app/locallib" ./rmm &
        sleep 4
        dd if=/tmp/audio_fifo of=/dev/null bs=1 count=8192
        if [[ $(get_config TIME_OSD) == "yes" ]] ; then
            (sleep 30; export TZP=`TZ=$TZ_TMP date +%z`; export TZP=${TZP:0:3}:${TZP:3:2}; export TZ=GMT$TZP; ./mp4record) &
        else
            ./mp4record &
        fi
        ./cloud &
        ./p2p_tnp &
        ./oss &
        if [ -f ./oss_fast ]; then
            ./oss_fast &
        fi
        if [ -f ./oss_lapse ]; then
            ./oss_lapse &
        fi
        (sleep 30; ./watch_process) &
    )
else
    (
        while read -r line
        do
            echo "127.0.0.1    $line" >> /etc/hosts
        done < $YI_HACK_PREFIX/script/blacklist/url

        while read -r line
        do
            route add -host $line reject
        done < $YI_HACK_PREFIX/script/blacklist/ip

        if [ $(get_config RTSP_AUDIO) != "no" ]; then
            touch /tmp/audio_fifo.requested
            touch /tmp/audio_fifo_mono_8khz.requested
        fi
        if [ $(get_config SPEAKER_AUDIO) != "no" ]; then
            touch /tmp/audio_in_fifo.requested
        fi
        cd /home/app
        killall dispatch
        LD_PRELOAD=/home/yi-hack/lib/ipc_multiplex.so ./dispatch &
        sleep 3
        if [ $(get_config TIME_OSD) == "yes" ]; then
            # Enable time osd
            set_tz_offset -c osd -o on
        fi
        LD_LIBRARY_PATH="/home/yi-hack/lib:/lib:/home/lib:/home/ms:/home/app/locallib" ./rmm &
        sleep 4
        dd if=/tmp/audio_fifo of=/dev/null bs=1 count=8192
        # Trick to start circular buffer filling
        start_buffer
        if [[ $(get_config REC_WITHOUT_CLOUD) == "yes" ]] ; then
            if [[ $(get_config TIME_OSD) == "yes" ]] ; then
                (sleep 30; export TZP=`TZ=$TZ_TMP date +%z`; export TZP=${TZP:0:3}:${TZP:3:2}; export TZ=GMT$TZP; ./mp4record) &
            else
                ./mp4record &
            fi
        fi
        ./cloud &
    )
fi

log "Yi processes started successfully" 1

if [ -f $YI_HACK_PREFIX/etc/TZ ]; then
    export TZ=`cat $YI_HACK_PREFIX/etc/TZ`
fi

if [[ $(get_config HTTPD) == "yes" ]] ; then
    log "Starting http"
    httpd -p $HTTPD_PORT -h $YI_HACK_PREFIX/www/ -c /tmp/httpd.conf
fi

if [[ $(get_config TELNETD) == "yes" ]] ; then
    telnetd
fi

if [[ $(get_config FTPD) == "yes" ]] ; then
    log "Starting ftp"
    $START_STOP_SCRIPT ftpd start
fi

if [[ $(get_config SSHD) == "yes" ]] ; then
    log "Starting sshd"
    dropbear -R
fi

if [[ $(get_config NTPD) == "yes" ]] ; then
    log "Starting ntp"
    # Wait until all the other processes have been initialized
    sleep 5 && ntpd -p $(get_config NTP_SERVER) &
fi

log "Starting mqtt services"
$START_STOP_SCRIPT mqtt start
if [[ $(get_config MQTT) == "yes" ]] ; then
    $START_STOP_SCRIPT mqtt-config start
    $YI_HACK_PREFIX/script/conf2mqtt.sh &
fi

sleep 5

if [[ $RTSP_PORT != "554" ]] ; then
    D_RTSP_PORT=:$RTSP_PORT
fi

if [[ $HTTPD_PORT != "80" ]] ; then
    D_HTTPD_PORT=:$HTTPD_PORT
fi

if [[ $(get_config ONVIF_WM_SNAPSHOT) == "yes" ]] ; then
    WATERMARK="&watermark=yes"
fi

if [[ $(get_config SNAPSHOT) == "no" ]] ; then
    touch /tmp/snapshot.disabled
fi

if [[ $(get_config SNAPSHOT_LOW) == "yes" ]] ; then
    touch /tmp/snapshot.low
fi

if [[ $(get_config RTSP) == "yes" ]] ; then
    log "Starting rtsp"
    $START_STOP_SCRIPT rtsp start
fi

if [[ $(get_config ONVIF) == "yes" ]] ; then
    log "Starting onvif"
    $START_STOP_SCRIPT onvif start

    if [[ $(get_config ONVIF_WSDD) == "yes" ]] ; then
        $START_STOP_SCRIPT wsdd start
    fi
fi

if [[ $(get_config TIME_OSD) == "yes" ]] ; then
    # Set timezone for time osd
    TZP=$(date +%z)
    TZP_SET=$(echo ${TZP:0:1} ${TZP:1:2} ${TZP:3:2} | awk '{ print ($1$2*3600+$3*60) }')
    set_tz_offset -c tz_offset_osd -m $MODEL_SUFFIX -f 0 -v $TZP_SET
fi

log "Starting crontab"
# Add crontab
CRONTAB=$(get_config CRONTAB)
FREE_SPACE=$(get_config FREE_SPACE)
mkdir -p /var/spool/cron/crontabs/
if [ ! -z "$CRONTAB" ]; then
    echo -e "$CRONTAB" > /var/spool/cron/crontabs/root
fi
if [[ $(get_config SNAPSHOT) == "yes" ]] && [[ $(get_config SNAPSHOT_VIDEO) == "yes" ]] ; then
    echo "* * * * * /home/yi-hack/script/thumb.sh cron" >> /var/spool/cron/crontabs/root
fi
if [ "$FREE_SPACE" != "0" ]; then
    echo "0 * * * * sleep 20; /home/yi-hack/script/clean_records.sh $FREE_SPACE" >> /var/spool/cron/crontabs/root
fi
if [[ $(get_config FTP_UPLOAD) == "yes" ]] ; then
    echo "* * * * * sleep 40; /home/yi-hack/script/ftppush.sh cron" >> /var/spool/cron/crontabs/root
fi
if [[ $(get_config TIMELAPSE) == "yes" ]] ; then
    DT=$(get_config TIMELAPSE_DT)
    OFF=""
    CRDT=""
    if [ "$DT" == "1" ]; then
        CRDT="* * * * *"
    elif [ "$DT" == "2" ] || [ "$DT" == "3" ] || [ "$DT" == "4" ] || [ "$DT" == "5" ] || [ "$DT" == "6" ] || [ "$DT" == "10" ] || [ "$DT" == "15" ] || [ "$DT" == "20" ] || [ "$DT" == "30" ]; then
        CRDT="*/$DT * * * *"
    elif [ "$DT" == "60" ]; then
        CRDT="0 * * * *"
    elif [ "$DT" == "120" ] || [ "$DT" == "180" ] || [ "$DT" == "240" ] || [ "$DT" == "360" ]; then
        DTF=$(($DT/60))
        CRDT="* */$DTF * * *"
    elif [ "$DT" == "1440" ]; then
        CRDT="0 0 * * *"
    elif [ "${DT:0:4}" == "1440" ]; then
        if [ "${DT:4:1}" == "+" ]; then
            OFF="${DT:5:10}"
            if [ ! -z $OFF ]; then
                case $OFF in
                    ''|*[!0-9]* )
                        OFF_OK=0;;
                    * )
                        OFF_OK=1;;
                esac
                if [ "$OFF_OK" == "1" ] && [ $OFF -le 1440 ]; then
                    OFF_H=$(($OFF/60))
                    OFF_M=$(($OFF%60))
                    CRDT="$OFF_M $OFF_H * * *"
                fi
            fi
        fi
    fi
    if [[ $(get_config TIMELAPSE_FTP) == "yes" ]] ; then
        echo "$CRDT sleep 30; /home/yi-hack/script/time_lapse.sh yes" >> /var/spool/cron/crontabs/root
    else
        if [ ! -z "$CRDT" ]; then
            echo "$CRDT sleep 30; /home/yi-hack/script/time_lapse.sh no" >> /var/spool/cron/crontabs/root
        fi
        VDT=$(get_config TIMELAPSE_VDT)
        if [ ! -z "$VDT" ]; then
            echo "$(get_config TIMELAPSE_VDT) /home/yi-hack/script/create_avi.sh /tmp/sd/record/timelapse 1920x1080 5" >> /var/spool/cron/crontabs/root
        fi
    fi
fi
/usr/sbin/crond -c /var/spool/cron/crontabs/

# Add MQTT Advertise
if [ -f "$YI_HACK_PREFIX/script/mqtt_advertise/startup.sh" ]; then
    $YI_HACK_PREFIX/script/mqtt_advertise/startup.sh
fi

unset TZ

log "Starting custom startup.sh"
if [ -f "/tmp/sd/yi-hack/startup.sh" ]; then
    /tmp/sd/yi-hack/startup.sh
fi

log "system.sh completed" 1
