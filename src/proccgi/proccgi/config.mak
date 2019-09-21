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

CC=arm-linux-gnueabihf-gcc
USER_CFLAGS=-mcpu=cortex-a7 -mfpu=neon-vfpv4 -I/opt/yi/arm-linux-gnueabihf-4.8.3-201404/arm-linux-gnueabihf/libc/usr/include -L/opt/yi/arm-linux-gnueabihf-4.8.3-201404/arm-linux-gnueabihf/libc/lib/arm-linux-gnueabihf
USER_LDFLAGS=
AR=arm-linux-gnueabihf-ar
RANLIB=arm-linux-gnueabihf-ranlib
STRIP=arm-linux-gnueabihf-strip
