#!/bin/sh

CONF_FILE="etc/system.conf"
YI_HACK_PREFIX="/home/yi-hack"
YI_HACK_VER=$(cat /home/yi-hack/version)
MODEL_SUFFIX=$(cat /tmp/sd/yi-hack/model_suffix)

get_config()
{
    key=$1
    grep -w $1 $YI_HACK_PREFIX/$CONF_FILE | cut -d "=" -f2
}

init_config()
{
    if [[ x$(get_config USERNAME) != "x" ]] ; then
        USERNAME=$(get_config USERNAME)
        PASSWORD=$(get_config PASSWORD)
    fi

    case $(get_config RTSP_PORT) in
        ''|*[!0-9]*) RTSP_PORT=554 ;;
        *) RTSP_PORT=$(get_config RTSP_PORT) ;;
    esac

    if [[ $RTSP_PORT != "554" ]] ; then
        D_RTSP_PORT=:$RTSP_PORT
    fi

    if [[ $(get_config RTSP) == "yes" ]] ; then
        RTSP_DAEMON="rRTSPServer"
        RTSP_AUDIO_COMPRESSION=$(get_config RTSP_AUDIO)
        NR_LEVEL=$(get_config RTSP_AUDIO_NR_LEVEL)

        if [[ $(get_config RTSP_ALT) == "yes" ]] ; then
            RTSP_DAEMON="rtsp_server_yi"
            if [[ "$RTSP_AUDIO_COMPRESSION" == "aac" ]] ; then
                RTSP_AUDIO_COMPRESSION="alaw"
            fi
        fi

        if [[ "$RTSP_AUDIO_COMPRESSION" == "none" ]] ; then
            RTSP_AUDIO_COMPRESSION="no"
        fi
        if [ ! -z $RTSP_AUDIO_COMPRESSION ]; then
            RTSP_AUDIO_COMPRESSION="-a "$RTSP_AUDIO_COMPRESSION
        fi
        if [ ! -z $RTSP_PORT ]; then
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

        RTSP_RES=$(get_config RTSP_STREAM)
        RTSP_ALT=$(get_config RTSP_ALT)
    fi
}

start_rtsp()
{
    if [ "$1" == "low" ] || [ "$1" == "high" ] || [ "$1" == "both" ]; then
        RTSP_RES=$1
    fi
    if [ "$2" == "no" ] || [ "$2" == "yes" ] || [ "$2" == "alaw" ] || [ "$2" == "ulaw" ] || [ "$2" == "pcm" ] || [ "$2" == "aac" ] ; then
        RTSP_AUDIO_COMPRESSION="-a "$2
    fi

    if [[ $RTSP_RES == "low" ]]; then
        h264grabber -m $MODEL_SUFFIX -r low -f &
        sleep 1
        CODEC_LOW=$(cat /tmp/lowres)
        if [ ! -z $CODEC_LOW ]; then
            CODEC_LOW="-c "$CODEC_LOW
        fi
        $RTSP_DAEMON -r low $CODEC_LOW $RTSP_AUDIO_COMPRESSION $RTSP_PORT $RTSP_USER $RTSP_PASSWORD $NR_LEVEL &
    elif [[ $RTSP_RES == "high" ]]; then
        h264grabber -m $MODEL_SUFFIX -r high -f &
        sleep 1
        CODEC_HIGH=$(cat /tmp/highres)
        if [ ! -z $CODEC_HIGH ]; then
            CODEC_HIGH="-C "$CODEC_HIGH
        fi
        $RTSP_DAEMON -r high $CODEC_HIGH $RTSP_AUDIO_COMPRESSION $RTSP_PORT $RTSP_USER $RTSP_PASSWORD $NR_LEVEL &
    elif [[ $RTSP_RES == "both" ]]; then
        h264grabber -m $MODEL_SUFFIX -r both -f &
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
    $YI_HACK_PREFIX/script/wd_rtsp.sh >/dev/null &
}

stop_rtsp()
{
    killall wd_rtsp.sh
    killall $RTSP_DAEMON
    killall h264grabber
}

ps_program()
{
    PS_PROGRAM=$(ps | grep $1 | grep -v grep | grep -c ^)
    if [ $PS_PROGRAM -gt 0 ]; then
        echo "started"
    else
        echo "stopped"
    fi
}

. $YI_HACK_PREFIX/www/cgi-bin/validate.sh

if ! $(validateQueryString $QUERY_STRING); then
    printf "Content-type: application/json\r\n\r\n"
    printf "{\n"
    printf "\"%s\":\"%s\"\\n" "error" "true"
    printf "}"
    exit
fi

VALUE="none"
PARAM1="none"
PARAM2="none"
RES="none"

CONF="$(echo $QUERY_STRING | cut -d'&' -f1 | cut -d'=' -f1)"
VAL="$(echo $QUERY_STRING | cut -d'&' -f1 | cut -d'=' -f2)"

if [ "$CONF" == "value" ] ; then
    VALUE="$VAL"
fi

init_config

if [ "$VALUE" == "on" ] ; then
    touch /tmp/privacy
    touch /tmp/snapshot.disabled
    stop_rtsp
    killall mp4record
    RES="on"
elif [ "$VALUE" == "off" ] ; then
    rm -f /tmp/snapshot.disabled
    start_rtsp $PARAM1 $PARAM2
    cd /home/app
    ./mp4record >/dev/null &
    rm -f /tmp/privacy
    RES="off"
elif [ "$VALUE" == "status" ] ; then
    if [ -f /tmp/privacy ]; then
        RES="on"
    else
        RES="off"
    fi
fi

printf "Content-type: application/json\r\n\r\n"
printf "{\n"
if [ ! -z "$RES" ]; then
    printf "\"status\": \"$RES\",\n"
fi
printf "\"%s\":\"%s\"\\n" "error" "true"
printf "}"
