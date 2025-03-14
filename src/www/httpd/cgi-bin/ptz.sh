#!/bin/sh

YI_HACK_PREFIX="/home/yi-hack"

. $YI_HACK_PREFIX/www/cgi-bin/validate.sh

if ! $(validateQueryString $QUERY_STRING); then
    printf "Content-type: application/json\r\n\r\n"
    printf "{\n"
    printf "\"%s\":\"%s\"\\n" "error" "true"
    printf "}"
    exit
fi

MODEL_SUFFIX=$(cat /home/yi-hack/model_suffix)

# ACTION: "step", "abs", "rel", "cont"
# COORD: x,y when ACTION="abs" or ACTION="rel"
# DIR: "right", "left", "down", "up", "stop" when ACTION="step" or ACTION="cont"
# TIME: when ACTION="step" is the time between start and stop
ACTION="step"
X="9999"
Y="9999"
DIR="none"
TIME="0.3"

for I in 1 2 3 4 5
do
    CONF="$(echo $QUERY_STRING | cut -d'&' -f$I | cut -d'=' -f1)"
    VAL="$(echo $QUERY_STRING | cut -d'&' -f$I | cut -d'=' -f2)"

    if [ "$CONF" == "action" ] ; then
        ACTION="$VAL"
    elif [ "$CONF" == "x" ] ; then
        X="$VAL"
    elif [ "$CONF" == "y" ] ; then
        Y="$VAL"
    elif [ "$CONF" == "dir" ] ; then
        DIR="$VAL"
    elif [ "$CONF" == "time" ] ; then
        TIME="$VAL"
    fi
done

if ! $(validateNumber $X); then
    printf "Content-type: application/json\r\n\r\n"
    printf "{\n"
    printf "\"%s\":\"%s\"\\n" "error" "true"
    printf "}"
    exit
fi
if ! $(validateNumber $Y); then
    printf "Content-type: application/json\r\n\r\n"
    printf "{\n"
    printf "\"%s\":\"%s\"\\n" "error" "true"
    printf "}"
    exit
fi
if ! $(validateString $DIR); then
    printf "Content-type: application/json\r\n\r\n"
    printf "{\n"
    printf "\"%s\":\"%s\"\\n" "error" "true"
    printf "}"
    exit
fi
if ! $(validateNumber $TIME); then
    printf "Content-type: application/json\r\n\r\n"
    printf "{\n"
    printf "\"%s\":\"%s\"\\n" "error" "true"
    printf "}"
    exit
fi

if [ "$ACTION" == "step" ]; then
    if [ "$DIR" != "none" ]; then
        case "$DIR" in
            *_*)
                DIR1=$(echo $DIR | cut -d_ -f1)
                DIR2=$(echo $DIR | cut -d_ -f2)
                if [ ! -z $DIR1 ]; then
                    ipc_cmd -m $DIR1
                    sleep $TIME
                    ipc_cmd -m stop
                fi
                if [ ! -z $DIR2 ]; then
                    ipc_cmd -m $DIR2
                    sleep $TIME
                    ipc_cmd -m stop
                fi
                ;;
            *)
                ipc_cmd -m $DIR
                sleep $TIME
                ipc_cmd -m stop
                ;;
        esac
    fi
elif [ "$ACTION" == "abs" ]; then
    ipc_cmd -j $X,$Y
elif [ "$ACTION" == "rel" ]; then
    ipc_cmd -J $X,$Y
elif [ "$ACTION" == "cont" ]; then
    if [ "$DIR" != "none" ]; then
        ipc_cmd -m $DIR
    fi
else
    printf "Content-type: application/json\r\n\r\n"
    printf "{\n"
    printf "\"%s\":\"%s\"\\n" "error" "true"
    printf "}"
    exit
fi

printf "Content-type: application/json\r\n\r\n"

printf "{\n"
printf "\"%s\":\"%s\"\\n" "error" "false"
printf "}"
