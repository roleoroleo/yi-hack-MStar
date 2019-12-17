#!/bin/sh

DIR="none"

CONF="$(echo $QUERY_STRING | cut -d'=' -f1)"
VAL="$(echo $QUERY_STRING | cut -d'=' -f2)"

if [ "$CONF" == "dir" ] ; then
    DIR="-m $VAL"
fi

if [ "$DIR" != "none" ] ; then
    ipc_cmd $DIR
    sleep 1
    ipc_cmd -m stop
fi

printf "Content-type: application/json\r\n\r\n"

printf "{\n"
printf "}"
