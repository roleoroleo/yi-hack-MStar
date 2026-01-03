#!/bin/bash

set -e

. ./config.jq

export CROSSPATH=/opt/yi/arm-linux-gnueabihf-4.8.3-201404/bin
export PATH=${PATH}:${CROSSPATH}

export TARGET=arm-linux-gnueabihf
export CROSS=arm-linux-gnueabihf
export BUILD=x86_64-pc-linux-gnu

export CROSSPREFIX=${CROSS}-

export STRIP=${CROSSPREFIX}strip
export CXX=${CROSSPREFIX}g++
export CC=${CROSSPREFIX}gcc
export LD=${CROSSPREFIX}ld
export AS=${CROSSPREFIX}as
export AR=${CROSSPREFIX}ar

SCRIPT_DIR=$(cd `dirname $0` && pwd)
cd $SCRIPT_DIR

cd jq-${VERSION}

make clean
make -j $(nproc)

mkdir -p ../_install/bin
mkdir -p ../_install/lib

cp ./vendor/oniguruma/src/.libs/libonig.so.5.5.0 ../_install/lib
cp ./.libs/libjq.so.1.0.4 ../_install/lib
cp ./.libs/jq ../_install/bin
ln -s libonig.so.5.5.0 ../_install/lib/libonig.so.5
ln -s libonig.so.5.5.0 ../_install/lib/libonig.so
ln -s libjq.so.1.0.4 ../_install/lib/libjq.so.1
ln -s libjq.so.1.0.4 ../_install/lib/libjq.so

$STRIP ../_install/bin/*
$STRIP ../_install/lib/*
