#!/bin/sh

BASE64="no"
RES="-r high"

for I in 1 2
do
    CONF="$(echo $QUERY_STRING | cut -d'&' -f$I | cut -d'=' -f1)"
    VAL="$(echo $QUERY_STRING | cut -d'&' -f$I | cut -d'=' -f2)"

    if [ "$CONF" == "res" ] ; then
        RES="-r $VAL"
    elif [ "$CONF" == "base64" ] ; then
        BASE64=$VAL
    fi
done

if [ "$BASE64" == "no" ] ; then
    printf "Content-type: image/jpeg\r\n\r\n"
    imggrabber $RES
else
    printf "Content-type: image/jpeg;base64\r\n\r\n"
    imggrabber $RES | base64
fi
