#!/bin/sh

printf "Content-type: image/jpeg\r\n\r\n"

CONF="$(echo $QUERY_STRING | cut -d'=' -f1)"
VAL="$(echo $QUERY_STRING | cut -d'=' -f2)"
RES=""

if [ $CONF == "res" ] ; then
    RES="$VAL"
fi

imggrabbercmd $RES
