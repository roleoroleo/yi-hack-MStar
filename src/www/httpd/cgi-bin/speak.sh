#!/bin/sh

export PATH=$PATH:/home/base/tools:/home/yi-hack/bin:/home/yi-hack/sbin:/tmp/sd/yi-hack/bin:/tmp/sd/yi-hack/sbin
export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:/home/lib:/home/yi-hack/lib:/tmp/sd/yi-hack/lib

YI_HACK_PREFIX="/home/yi-hack"

. $YI_HACK_PREFIX/www/cgi-bin/validate.sh

if ! $(validateQueryString $QUERY_STRING); then
    printf "Content-type: application/json\r\n\r\n"
    printf "{\n"
    printf "\"%s\":\"%s\"\\n" "error" "true"
    printf "}"
    exit
fi

LANG="en-US"
VOL="1"
VOLDB="0"
IS_DB="0"

for I in 1 2
do
    PARAM="$(echo $QUERY_STRING | cut -d'&' -f$I | cut -d'=' -f1)"
    VALUE="$(echo $QUERY_STRING | cut -d'&' -f$I | cut -d'=' -f2)"

    if [ "$PARAM" == "lang" ] ; then
        LANG="$VALUE"
    elif [ "$PARAM" == "vol" ] ; then
        VOL="$VALUE"
        IS_DB="0"
    elif [ "$PARAM" == "voldb" ] ; then
        VOLDB="$VALUE"
        IS_DB="1"
    fi
done

if ! $(validateLang $LANG); then
    printf "{\n"
    printf "\"%s\":\"%s\",\\n" "error" "true"
    printf "\"%s\":\"%s\"\\n" "description" "Invalid language"
    printf "}"
    exit
fi
if ! $(validateNumber $VOL); then
    printf "{\n"
    printf "\"%s\":\"%s\",\\n" "error" "true"
    printf "\"%s\":\"%s\"\\n" "description" "Invalid volume"
    printf "}"
    exit
fi
if ! $(validateNumber $VOLDB); then
    printf "{\n"
    printf "\"%s\":\"%s\",\\n" "error" "true"
    printf "\"%s\":\"%s\"\\n" "description" "Invalid volume (dB)"
    printf "}"
    exit
fi

read -r POST_DATA

printf "Content-type: application/json\r\n\r\n"

if [ -f /tmp/sd/yi-hack/bin/nanotts ] && [ -e /tmp/audio_in_fifo ]; then
    TMP_FILE="/tmp/sd/speak.pcm"
    if [ ! -f $TMP_FILE ]; then
        echo "$POST_DATA" | /tmp/sd/yi-hack/bin/nanotts -l /tmp/sd/yi-hack/usr/share/pico/lang -v $LANG -c > $TMP_FILE
        if [ "$IS_DB" == "1" ]; then
            cat $TMP_FILE | pcmvol -G $VOLDB > /tmp/audio_in_fifo
        else
            cat $TMP_FILE | pcmvol -g $VOL > /tmp/audio_in_fifo
        fi
        sleep 1
        rm $TMP_FILE

        printf "{\n"
        printf "\"%s\":\"%s\",\\n" "error" "false"
        printf "\"%s\":\"%s\"\\n" "description" "$POST_DATA"
        printf "}"
    else
        printf "{\n"
        printf "\"%s\":\"%s\",\\n" "error" "true"
        printf "\"%s\":\"%s\"\\n" "description" "Speaker busy"
        printf "}"
    fi
else
    if [ ! -f /tmp/sd/yi-hack/bin/nanotts ]; then
        printf "{\n"
        printf "\"%s\":\"%s\"\\n" "error" "true"
        printf "\"%s\":\"%s\"\\n" "description" "TTS engine not found"
        printf "}"
    elif [ ! -e /tmp/audio_in_fifo ]; then
        printf "{\n"
        printf "\"%s\":\"%s\"\\n" "error" "true"
        printf "\"%s\":\"%s\"\\n" "description" "Audio input disabled"
        printf "}"
    fi
fi
