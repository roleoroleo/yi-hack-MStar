#!/bin/sh

CONF_FILE="etc/system.conf"

YI_HACK_PREFIX="/home/yi-hack"
YI_PREFIX="/home/app"

MODEL_SUFFIX=$(cat /home/yi-hack/model_suffix)

get_config()
{
    key=$1
    grep -w $1 $YI_HACK_PREFIX/$CONF_FILE | cut -d "=" -f2
}

export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:/home/lib:/home/yi-hack/lib:/tmp/sd/yi-hack/lib
export PATH=$PATH:/home/base/tools:/home/yi-hack/bin:/home/yi-hack/sbin:/tmp/sd/yi-hack/bin:/tmp/sd/yi-hack/sbin

ulimit -s 1024

# Remove core files, if any
rm -f $YI_HACK_PREFIX/bin/core
rm -f $YI_PREFIX/core

if [ ! -L /home/yi-hack-v4 ]; then
    ln -s $YI_HACK_PREFIX /home/yi-hack-v4
fi

hostname -F /etc/hostname

touch /tmp/httpd.conf

# Restore configuration after a firmware upgrade
if [ -f $YI_HACK_PREFIX/.fw_upgrade_in_progress ]; then
    cp -f /tmp/sd/.fw_upgrade/*.conf $YI_HACK_PREFIX/etc/
    cp -f /tmp/sd/.fw_upgrade/TZ $YI_HACK_PREFIX/etc/
    rm $YI_HACK_PREFIX/.fw_upgrade_in_progress
    chmod 0644 $YI_HACK_PREFIX/etc/*.conf
    chmod 0644 $YI_HACK_PREFIX/etc/TZ
fi

$YI_HACK_PREFIX/script/check_conf.sh

if [[ x$(get_config USERNAME) != "x" ]] ; then
    USERNAME=$(get_config USERNAME)
    PASSWORD=$(get_config PASSWORD)
    ONVIF_USERPWD="--user $USERNAME --password $PASSWORD"
    echo "/:$USERNAME:$PASSWORD" > /tmp/httpd.conf
fi

case $(get_config RTSP_PORT) in
    ''|*[!0-9]*) RTSP_PORT=554 ;;
    *) RTSP_PORT=$(get_config RTSP_PORT) ;;
esac
case $(get_config ONVIF_PORT) in
    ''|*[!0-9]*) ONVIF_PORT=80 ;;
    *) ONVIF_PORT=$(get_config ONVIF_PORT) ;;
esac
case $(get_config HTTPD_PORT) in
    ''|*[!0-9]*) HTTPD_PORT=8080 ;;
    *) HTTPD_PORT=$(get_config HTTPD_PORT) ;;
esac

if [[ $(get_config DISABLE_CLOUD) == "no" ]] ; then
    (
        cd /home/app
        if [[ $(get_config RTSP_AUDIO) == "none" ]] ; then
            sleep 2
        else
            OLD_LD_LIBRARY_PATH=$LD_LIBRARY_PATH
            killall rmm
            sleep 2
            export LD_LIBRARY_PATH="/home/yi-hack/lib:/lib:/home/lib:/home/ms:/home/app/locallib"
            ./rmm &
            export LD_LIBRARY_PATH=$OLD_LD_LIBRARY_PATH
            sleep 2
            dd if=/tmp/audio_fifo of=/dev/null bs=1 count=2048
        fi
        ./mp4record &
        ./cloud &
        ./p2p_tnp &
        ./oss &
        if [ -f ./oss_fast ]; then
            ./oss_fast &
        fi
        if [ -f ./oss_lapse ]; then
            ./oss_lapse &
        fi
        ./watch_process &
    )
else
    (
        cd /home/app
        if [[ $(get_config RTSP_AUDIO) == "none" ]] ; then
            sleep 2
        else
            OLD_LD_LIBRARY_PATH=$LD_LIBRARY_PATH
            killall rmm
            sleep 2
            export LD_LIBRARY_PATH="/home/yi-hack/lib:/lib:/home/lib:/home/ms:/home/app/locallib"
            ./rmm &
            export LD_LIBRARY_PATH=$OLD_LD_LIBRARY_PATH
            sleep 2
            dd if=/tmp/audio_fifo of=/dev/null bs=1 count=2048
        fi
        # Trick to start circular buffer filling
        ipc_cmd -x
        if [[ $(get_config REC_WITHOUT_CLOUD) == "yes" ]] ; then
            ./mp4record &
        fi
    )
fi

if [[ $(get_config HTTPD) == "yes" ]] ; then
    httpd -p $HTTPD_PORT -h $YI_HACK_PREFIX/www/ -c /tmp/httpd.conf
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

if [[ $(get_config RTSP) == "yes" ]] ; then
    RRTSP_RES=$(get_config RTSP_STREAM) RRTSP_AUDIO=$(get_config RTSP_AUDIO) RRTSP_PORT=$RTSP_PORT RRTSP_USER=$USERNAME RRTSP_PWD=$PASSWORD rRTSPServer &

    if [[ $(get_config RTSP_STREAM) == "high" ]]; then
        ONVIF_PROFILE_0="--name Profile_0 --width 1920 --height 1080 --url rtsp://%s$D_RTSP_PORT/ch0_0.h264 --snapurl http://%s$D_HTTPD_PORT/cgi-bin/snapshot.sh?res=high$WATERMARK --type H264"
    fi
    if [[ $(get_config RTSP_STREAM) == "low" ]]; then
        ONVIF_PROFILE_1="--name Profile_1 --width 640 --height 360 --url rtsp://%s$D_RTSP_PORT/ch0_1.h264 --snapurl http://%s$D_HTTPD_PORT/cgi-bin/snapshot.sh?res=low$WATERMARK --type H264"
    fi
    if [[ $(get_config RTSP_STREAM) == "both" ]]; then
        if [[ $(get_config ONVIF_PROFILE) == "high" ]] || [[ $(get_config ONVIF_PROFILE) == "both" ]] ; then
            ONVIF_PROFILE_0="--name Profile_0 --width 1920 --height 1080 --url rtsp://%s$D_RTSP_PORT/ch0_0.h264 --snapurl http://%s$D_HTTPD_PORT/cgi-bin/snapshot.sh?res=high$WATERMARK --type H264"
        fi
        if [[ $(get_config ONVIF_PROFILE) == "low" ]] || [[ $(get_config ONVIF_PROFILE) == "both" ]] ; then
            ONVIF_PROFILE_1="--name Profile_1 --width 640 --height 360 --url rtsp://%s$D_RTSP_PORT/ch0_1.h264 --snapurl http://%s$D_HTTPD_PORT/cgi-bin/snapshot.sh?res=low$WATERMARK --type H264"
        fi
    fi
    $YI_HACK_PREFIX/script/wd_rtsp.sh &
fi

if [[ $(get_config ONVIF) == "yes" ]] ; then
    if [[ $MODEL_SUFFIX == "h201c" ]] ; then
        onvif_srvd --pid_file /var/run/onvif_srvd.pid --model "Yi Hack" --manufacturer "Yi" --ifs wlan0 --port $ONVIF_PORT --scope onvif://www.onvif.org/Profile/S $ONVIF_PROFILE_0 $ONVIF_PROFILE_1 $ONVIF_USERPWD --ptz --move_left "/home/yi-hack/bin/ipc_cmd -m left" --move_right "/home/yi-hack/bin/ipc_cmd -m right" --move_up "/home/yi-hack/bin/ipc_cmd -m up" --move_down "/home/yi-hack/bin/ipc_cmd -m down" --move_stop "/home/yi-hack/bin/ipc_cmd -m stop" --move_preset "/home/yi-hack/bin/ipc_cmd -p"
    else
        onvif_srvd --pid_file /var/run/onvif_srvd.pid --model "Yi Hack" --manufacturer "Yi" --ifs wlan0 --port $ONVIF_PORT --scope onvif://www.onvif.org/Profile/S $ONVIF_PROFILE_0 $ONVIF_PROFILE_1 $ONVIF_USERPWD
    fi
fi

FREE_SPACE=$(get_config FREE_SPACE)
if [[ $FREE_SPACE != "0" ]] ; then
    mkdir -p /var/spool/cron/crontabs/
    echo "  0  *  *  *  *  /home/yi-hack/script/clean_records.sh $FREE_SPACE" > /var/spool/cron/crontabs/root
    /usr/sbin/crond -c /var/spool/cron/crontabs/
fi

if [ -f "/tmp/sd/yi-hack/startup.sh" ]; then
    /tmp/sd/yi-hack/startup.sh
fi
