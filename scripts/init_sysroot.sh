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
    echo "$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null 2>&1 && pwd )"
}

get_sysroot_dir()
{
    local SYSROOT_DIR=$(get_script_dir)/../sysroot
    echo "$(normalize_path $SYSROOT_DIR)"
}

create_sysroot_dir()
{
    local CAMERA_NAME=$1
    local SYSROOT_BASE_DIR=$2

    local SYSROOT_DIR=$SYSROOT_BASE_DIR/$CAMERA_NAME
    if [[ -d $SYSROOT_DIR/home && -d $SYSROOT_DIR/rootfs ]]; then
        echo "ERROR: The $CAMERA_NAME sysroot folder already exists. Exiting."
        echo ""
        exit 1
    fi

    echo "Creating the sysroot dirs.."
    mkdir -p "$SYSROOT_DIR/home"
    echo "\"$SYSROOT_DIR/home\" folder created!"
    mkdir -p "$SYSROOT_DIR/rootfs"
    echo "\"$SYSROOT_DIR/rootfs\" folder created!"
}

jffs2_mount()
{
    local JFFS2_FILE=$1
    local JFFS2_MOUNT=$2

    # cleanup if necessary
    umount /dev/mtdblock0 &>/dev/null
    modprobe -r mtdram >/dev/null
    modprobe -r mtdblock >/dev/null

    modprobe mtdram total_size=32768 erase_size=64 || exit 1
    modprobe mtdblock || exit 1
    dd if="$JFFS2_FILE" of=/dev/mtdblock0 &>/dev/null || exit 1
    mount -t jffs2 /dev/mtdblock0 "$JFFS2_MOUNT" || exit 1
}

jffs2_umount()
{
    local JFFS2_MOUNT=$1
    umount $JFFS2_MOUNT
    umount /dev/mtdblock0 &>/dev/null
}

jffs2_copy()
{
    local JFFS2_FILE=$1
    local DEST_DIR=$2

    local TMP_DIR=$(mktemp -d)

    if [[ ! "$TMP_DIR" || ! -d "$TMP_DIR" ]]; then
        echo "ERROR: Could not create temp dir \"$TMP_DIR\". Exiting."
        exit 1
    fi

    jffs2_mount $JFFS2_FILE $TMP_DIR
    rsync -a $TMP_DIR/* $DEST_DIR
    jffs2_umount $TMP_DIR

    rm -rf "$TMP_DIR"
}

extract_stock_fw()
{
    local CAMERA_ID=$1
    local SYSROOT_DIR=$2
    local FW_DIR=$3

    local FIRMWARE_HOME=$FW_DIR/home_$CAMERA_ID.tgz
    local FIRMWARE_ROOTFS=$FW_DIR/sys_$CAMERA_ID.tgz

    local FIRMWARE_HOME_JFFS2=$FW_DIR/home_$CAMERA_ID.jffs2
    local FIRMWARE_ROOTFS_JFFS2=$FW_DIR/sys_$CAMERA_ID.jffs2

    local FIRMWARE_HOME_DESTDIR=$SYSROOT_DIR/home
    local FIRMWARE_ROOTFS_DESTDIR=$SYSROOT_DIR/rootfs

    echo "Extracting the stock firmware file systems or archives..."

    if [[ ! -f "$FIRMWARE_HOME_JFFS2" || ! -f "$FIRMWARE_ROOTFS_JFFS2" ]]; then
        echo "ERROR: $FIRMWARE_HOME_JFFS2 or $FIRMWARE_ROOTFS_JFFS2 not found. Trying with .tgz archives."

        if [[ ! -f "$FIRMWARE_HOME" || ! -f "$FIRMWARE_ROOTFS" ]]; then
            echo "ERROR: $FIRMWARE_HOME or $FIRMWARE_ROOTFS not found. Exiting."
            exit 1
        fi

        # copy the stock firmware archives contents to the sysroot
        echo $FIRMWARE_HOME
        printf "Extracting \"home_$CAMERA_ID\" to \"$FIRMWARE_HOME_DESTDIR\"... "
        tar zxvf $FIRMWARE_HOME -C $FIRMWARE_HOME_DESTDIR
        echo "done!"

        echo $FIRMWARE_ROOTFS
        printf "Extracting \"sys_$CAMERA_ID\" to \"$FIRMWARE_ROOTFS_DESTDIR\"... "
        tar zxvf $FIRMWARE_ROOTFS -C $FIRMWARE_ROOTFS_DESTDIR
        echo "done!"
    else
        echo $FIRMWARE_HOME_JFFS2
        printf "Extracting \"home_$CAMERA_ID\" to \"$FIRMWARE_HOME_DESTDIR\"... "
        jffs2_copy $FIRMWARE_HOME_JFFS2 $FIRMWARE_HOME_DESTDIR
        echo "done!"

        echo $FIRMWARE_ROOTFS_JFFS2
        printf "Extracting \"sys_$CAMERA_ID\" to \"$FIRMWARE_ROOTFS_DESTDIR\"... "
        jffs2_copy $FIRMWARE_ROOTFS_JFFS2 $FIRMWARE_ROOTFS_DESTDIR
        echo "done!"
    fi
}

###############################################################################

source "$(get_script_dir)/common.sh"

require_root

if [ $# -ne 1 ]; then
    echo "Usage: init_sysroot.sh camera_name"
    echo ""
    exit 1
fi

CAMERA_NAME=$1

check_camera_name $CAMERA_NAME

CAMERA_ID=$(get_camera_id $CAMERA_NAME)
SYSROOT_BASE_DIR=$(get_sysroot_dir)
SYSROOT_DIR=$SYSROOT_BASE_DIR/$CAMERA_NAME
FIRMWARE_DIR=$(normalize_path $(get_script_dir)/../stock_firmware)/$CAMERA_NAME

echo ""
echo "------------------------------------------------------------------------"
echo " YI-HACK - INIT SYSROOT"
echo "------------------------------------------------------------------------"
printf " camera_name      : %s\n" $CAMERA_NAME
printf " camera_id        : %s\n" $CAMERA_ID
printf "\n"
printf " sysroot_base_dir : %s\n" $SYSROOT_BASE_DIR
printf " sysroot_dir      : %s\n" $SYSROOT_DIR
printf " firmware_dir     : %s\n" $FIRMWARE_DIR
echo "------------------------------------------------------------------------"
echo ""

echo ""

if [[ ! -d "$FIRMWARE_DIR" ]]; then
    printf "ERROR: Cannot find %s\nExiting." $FIRMWARE_DIR
    exit 1
fi

# Create the needed directories
create_sysroot_dir $CAMERA_NAME $SYSROOT_BASE_DIR

echo ""

# Extract the stock fw to the camera's sysroot
extract_stock_fw $CAMERA_ID $SYSROOT_DIR $FIRMWARE_DIR

echo ""

echo "Success!"
echo ""

exit 0


