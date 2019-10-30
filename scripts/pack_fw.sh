#!/bin/bash

#
#  This file is part of yi-hack-v4 (https://github.com/TheCrypt0/yi-hack-v4).
#  Copyright (c) 2018-2019 Davide Maggioni.
# 
#  This program is free software: you can redistribute it and/or modify
#  it under the terms of the GNU General Public License as published by
#  the Free Software Foundation, version 3.
# 
#  This program is distributed in the hope that it will be useful, but
#  WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
#  General Public License for more details.
# 
#  You should have received a copy of the GNU General Public License
#  along with this program. If not, see <http://www.gnu.org/licenses/>.
#

get_script_dir()
{
    echo "$(cd `dirname $0` && pwd)"
}

create_tmp_dir()
{
    local TMP_DIR=$(mktemp -d)

    if [[ ! "$TMP_DIR" || ! -d "$TMP_DIR" ]]; then
        echo "ERROR: Could not create temp dir \"$TMP_DIR\". Exiting."
        exit 1
    fi

    echo $TMP_DIR
}

compress_file()
{
    local DIR=$1
    local FILENAME=$2
    local FILE=$DIR/$FILENAME
    if [[ -f "$FILE" ]]; then
        printf "Compressing %s... " $FILENAME
        7za a "$FILE.7z" "$FILE" > /dev/null
        rm -f "$FILE"
        printf "done!\n"
    fi
}

pack_image()
{
    local TYPE=$1
    local CAMERA_ID=$2
    local DIR=$3
    local OUT=$4

    printf "> PACKING : %s_%s\n\n" $TYPE $CAMERA_ID

    printf "Creating jffs2 filesystem... "
    mkfs.jffs2 -l -e 0x10000 -r $DIR/$TYPE -o $OUT/${TYPE}_${CAMERA_ID}.jffs2 || exit 1
    printf "done!\n"
}

###############################################################################

source "$(get_script_dir)/common.sh"

require_root


if [ $# -ne 1 ]; then
    echo "Usage: pack_sw.sh camera_name"
    echo ""
    exit 1
fi

CAMERA_NAME=$1

check_camera_name $CAMERA_NAME

CAMERA_ID=$(get_camera_id $CAMERA_NAME)

BASE_DIR=$(get_script_dir)/../
BASE_DIR=$(normalize_path $BASE_DIR)

SYSROOT_DIR=$BASE_DIR/sysroot/$CAMERA_NAME
STATIC_DIR=$BASE_DIR/static
BUILD_DIR=$BASE_DIR/build
OUT_DIR=$BASE_DIR/out/$CAMERA_NAME
VER=$(cat VERSION)

echo ""
echo "------------------------------------------------------------------------"
echo " YI-HACK - FIRMWARE PACKER"
echo "------------------------------------------------------------------------"
printf " camera_name      : %s\n" $CAMERA_NAME
printf " camera_id        : %s\n" $CAMERA_ID
printf "                      \n"
printf " sysroot_dir      : %s\n" $SYSROOT_DIR
printf " static_dir       : %s\n" $STATIC_DIR
printf " build_dir        : %s\n" $BUILD_DIR
printf " out_dir          : %s\n" $OUT_DIR
echo "------------------------------------------------------------------------"
echo ""

printf "Starting...\n\n"

sleep 1 

printf "Checking if the required sysroot exists... "

# Check if the sysroot exist
if [[ ! -d "$SYSROOT_DIR/home" || ! -d "$SYSROOT_DIR/rootfs" ]]; then
    printf "\n\n"
    echo "ERROR: Cannot find the sysroot. Missing:"
    echo " > $SYSROOT_DIR/home"
    echo " > $SYSROOT_DIR/rootfs"
    echo ""
    echo "You should create the $CAMERA_NAME sysroot before trying to pack the firmware."
    exit 1
else
    printf "yeah!\n"
fi

printf "Creating the out directory... "
mkdir -p $OUT_DIR
printf "%s created!\n\n" $OUT_DIR

printf "Creating the tmp directory... "
TMP_DIR=$(create_tmp_dir)
printf "%s created!\n\n" $TMP_DIR

# Copy the sysroot to the tmp dir
printf "Copying the sysroot contents... "
rsync -a $SYSROOT_DIR/rootfs/* $TMP_DIR/rootfs || exit 1
rsync -a $SYSROOT_DIR/home/* $TMP_DIR/home || exit 1
printf "done!\n"

# We can safely replace chinese audio files with links to the us version
printf "Removing unneeded audio files... "

AUDIO_EXTENSION="*.aac"

for AUDIO_FILE in $TMP_DIR/home/app/audio_file/us/$AUDIO_EXTENSION ; do
    AUDIO_NAME=$(basename $AUDIO_FILE)

    # Delete the original audio files
    rm -f $TMP_DIR/home/app/audio_file/jp/$AUDIO_NAME
    rm -f $TMP_DIR/home/app/audio_file/simplecn/$AUDIO_NAME

    # Create links to the us version
    ln -s ../us/$AUDIO_NAME $TMP_DIR/home/app/audio_file/jp/$AUDIO_NAME
    ln -s ../us/$AUDIO_NAME $TMP_DIR/home/app/audio_file/simplecn/$AUDIO_NAME
done
printf "done!\n"

# Copy the build files to the tmp dir
printf "Copying the build files... "
rsync -a $BUILD_DIR/rootfs/* $TMP_DIR/rootfs || exit 1
rsync -a $BUILD_DIR/home/* $TMP_DIR/home || exit 1
printf "done!\n"

# insert the version file
printf "Copying the version file... "
cp $BASE_DIR/VERSION $TMP_DIR/home/yi-hack/version
printf "done!\n\n"

# fix the files ownership
printf "Fixing the files ownership... "
chown -R root:root $TMP_DIR/*
printf "done!\n\n"

# Compress a couple of the yi app files
compress_file "$TMP_DIR/home/app" cloudAPI
compress_file "$TMP_DIR/home/app" oss
compress_file "$TMP_DIR/home/app" p2p_tnp
compress_file "$TMP_DIR/home/app" rmm

# Compress the yi-hack folder
printf "Compressing yi-hack... "
7za a $TMP_DIR/home/yi-hack/yi-hack.7z $TMP_DIR/home/yi-hack/* > /dev/null

# Delete all the compressed files except system_init.sh and yi-hack.7z
find $TMP_DIR/home/yi-hack/script/ -maxdepth 0 ! -name 'system_init.sh' -type f -exec rm -f {} +
find $TMP_DIR/home/yi-hack/* -maxdepth 0 -type d ! -name 'script' -exec rm -rf {} +
find $TMP_DIR/home/yi-hack/* -maxdepth 0 -type f -not -name 'yi-hack.7z' -exec rm {} +
printf "done!\n\n"

# home 
pack_image "home" $CAMERA_ID $TMP_DIR $OUT_DIR
mv $OUT_DIR/home_$CAMERA_ID.jffs2 $OUT_DIR/home_$CAMERA_ID

# rootfs
pack_image "rootfs" $CAMERA_ID $TMP_DIR $OUT_DIR
mv $OUT_DIR/rootfs_$CAMERA_ID.jffs2 $OUT_DIR/sys_$CAMERA_ID

# create tar.gz
rm -f $OUT_DIR/*.tgz
tar zcvf $OUT_DIR/${CAMERA_NAME}_${VER}.tgz -C $OUT_DIR sys_$CAMERA_ID home_$CAMERA_ID

# Cleanup
printf "Cleaning up the tmp folder... "
rm -rf $TMP_DIR
printf "done!\n\n"

echo "------------------------------------------------------------------------"
echo " Finished!"
echo "------------------------------------------------------------------------"

