#!/bin/sh

YI_HACK_PREFIX="/home/yi-hack"
CONF_FILE="$YI_HACK_PREFIX/etc/camera.conf"

. $YI_HACK_PREFIX/www/cgi-bin/validate.sh

if ! $(validateQueryString $QUERY_STRING); then
    printf "Content-type: application/json\r\n\r\n"
    printf "{\n"
    printf "\"%s\":\"%s\"\\n" "error" "true"
    printf "}"
    exit
fi

CONF_LAST="CONF_LAST"

for I in 1 2 3 4 5 6 7
do
    CONF="$(echo $QUERY_STRING | cut -d'&' -f$I | cut -d'=' -f1)"
    VAL="$(echo $QUERY_STRING | cut -d'&' -f$I | cut -d'=' -f2)"

    if ! $(validateString $CONF); then
        printf "Content-type: application/json\r\n\r\n"
        printf "{\n"
        printf "\"%s\":\"%s\"\\n" "error" "true"
        printf "}"
        exit
    fi
    if ! $(validateString $VAL); then
        printf "Content-type: application/json\r\n\r\n"
        printf "{\n"
        printf "\"%s\":\"%s\"\\n" "error" "true"
        printf "}"
        exit
    fi

    if [ $CONF == $CONF_LAST ]; then
        continue
    fi
    CONF_LAST=$CONF
    CONF_UPPER="$(echo $CONF | tr '[a-z]' '[A-Z]')"

    sed -i "s/^\(${CONF_UPPER}\s*=\s*\).*$/\1${VAL}/" $CONF_FILE

    if [ "$CONF" == "switch_on" ] ; then
        if [ "$VAL" == "no" ] ; then
            ipc_cmd -t off
        else
            ipc_cmd -t on
        fi
    elif [ "$CONF" == "save_video_on_motion" ] ; then
        if [ "$VAL" == "no" ] ; then
            ipc_cmd -v always
        else
            ipc_cmd -v detect
        fi
    elif [ "$CONF" == "sensitivity" ] ; then
        if [ "$VAL" == "low" ] || [ "$VAL" == "medium" ] || [ "$VAL" == "high" ]; then
            ipc_cmd -s $VAL
        fi
    elif [ "$CONF" == "baby_crying_detect" ] ; then
        if [ "$VAL" == "no" ] ; then
            ipc_cmd -b off
        else
            ipc_cmd -b on
        fi
    elif [ "$CONF" == "led" ] ; then
        if [ "$VAL" == "no" ] ; then
            ipc_cmd -l off
        else
            ipc_cmd -l on
        fi
    elif [ "$CONF" == "ir" ] ; then
        if [ "$VAL" == "no" ] ; then
            ipc_cmd -i off
        else
            ipc_cmd -i on
        fi
    elif [ "$CONF" == "rotate" ] ; then
        if [ "$VAL" == "no" ] ; then
            ipc_cmd -r off
        else
            ipc_cmd -r on
        fi
    fi
    sleep 1
done

printf "Content-type: application/json\r\n\r\n"

printf "{\n"
printf "\"%s\":\"%s\"\\n" "error" "false"
printf "}"
