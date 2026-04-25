#!/bin/sh

CONF_FILE="etc/system.conf"

YI_HACK_PREFIX="/home/yi-hack"
MODEL_SUFFIX=$(cat /home/yi-hack/model_suffix)

get_config()
{
    key=$1
    grep -w $1 $YI_HACK_PREFIX/$CONF_FILE | cut -d "=" -f2
}

TZ_TMP=$(get_config TIMEZONE)

# Set timezone for time osd
TZP=$(date +%z)
TZP_SET=$(echo ${TZP:0:1} ${TZP:1:2} ${TZP:3:2} | awk '{ print ($1$2*3600+$3*60) }')
$YI_HACK_PREFIX/bin/set_tz_offset -c tz_offset_osd -m $MODEL_SUFFIX -f 0 -v $TZP_SET
