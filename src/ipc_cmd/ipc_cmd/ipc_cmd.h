/*
 * This file is part of libipc (https://github.com/TheCrypt0/libipc).
 * Copyright (c) 2019 roleo.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, version 3.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdarg.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include <time.h>
#include <errno.h>
#include <limits.h>
#include <mqueue.h>
#include <sys/stat.h>

#define IPC_QUEUE_NAME          "/ipc_dispatch"
#define IPC_W_QUEUE_NAME        "/ipc_dispatch_worker"
#define IPC_MESSAGE_MAX_SIZE    512

#define NONE -1

#define MAX_PTZ_X 4100
#define MAX_PTZ_Y 1250

#define IPC_MOTION_START "\x01\x00\x00\x00\x02\x00\x00\x00\x7c\x00\x7c\x00\x00\x00\x00\x00"
#define IPC_MOTION_STOP "\x01\x00\x00\x00\x02\x00\x00\x00\x7d\x00\x7d\x00\x00\x00\x00\x00"

#define IPC_SWITCH_OFF "\x02\x00\x00\x00\x08\x00\x00\x00\x75\x00\x01\x00\x00\x00\x00\x00"
#define IPC_SWITCH_ON  "\x02\x00\x00\x00\x08\x00\x00\x00\x74\x00\x01\x00\x00\x00\x00\x00"

#define SWITCH_OFF 0
#define SWITCH_ON  1

#define IPC_SENS_HIGH   "\x01\x00\x00\x00\x08\x00\x00\x00\x27\x10\x01\x00\x04\x00\x00\x00\x00\x00\x00\x00"
#define IPC_SENS_MEDIUM "\x01\x00\x00\x00\x08\x00\x00\x00\x27\x10\x01\x00\x04\x00\x00\x00\x01\x00\x00\x00"
#define IPC_SENS_LOW    "\x01\x00\x00\x00\x08\x00\x00\x00\x27\x10\x01\x00\x04\x00\x00\x00\x02\x00\x00\x00"

#define SENSITIVITY_HIGH   0
#define SENSITIVITY_MEDIUM 1
#define SENSITIVITY_LOW    2

#define IPC_LED_OFF "\x02\x00\x00\x00\x08\x00\x00\x00\x77\x00\x01\x00\x00\x00\x00\x00"
#define IPC_LED_ON  "\x02\x00\x00\x00\x08\x00\x00\x00\x76\x00\x01\x00\x00\x00\x00\x00"

#define LED_OFF 0
#define LED_ON  1

#define IPC_SAVE_ALWAYS "\x02\x00\x00\x00\x08\x00\x00\x00\x79\x00\x01\x00\x00\x00\x00\x00"
#define IPC_SAVE_DETECT "\x02\x00\x00\x00\x08\x00\x00\x00\x78\x00\x01\x00\x00\x00\x00\x00"

#define SAVE_ALWAYS 0
#define SAVE_DETECT 1

#define IPC_IR_OFF "\x02\x00\x00\x00\x08\x00\x00\x00\x24\x10\x01\x00\x04\x00\x00\x00\x02\x00\x00\x00"
#define IPC_IR_ON  "\x02\x00\x00\x00\x08\x00\x00\x00\x24\x10\x01\x00\x04\x00\x00\x00\x01\x00\x00\x00"

#define IR_OFF 0
#define IR_ON  1

#define IPC_ROTATE_OFF "\x02\x00\x00\x00\x08\x00\x00\x00\x7b\x00\x01\x00\x00\x00\x00\x00"
#define IPC_ROTATE_ON  "\x02\x00\x00\x00\x08\x00\x00\x00\x7a\x00\x01\x00\x00\x00\x00\x00"

#define ROTATE_OFF 0
#define ROTATE_ON  1

#define IPC_MOVE_RIGHT "\x01\x00\x00\x00\x08\x00\x00\x00\x06\x40\x06\x40\x18\x00\x00\x00\x04\x00\x00\x00\x00\x00\x00\x00"
#define IPC_MOVE_LEFT  "\x01\x00\x00\x00\x08\x00\x00\x00\x06\x40\x06\x40\x18\x00\x00\x00\x03\x00\x00\x00\x00\x00\x00\x00"
#define IPC_MOVE_DOWN  "\x01\x00\x00\x00\x08\x00\x00\x00\x06\x40\x06\x40\x18\x00\x00\x00\x02\x00\x00\x00\x00\x00\x00\x00"
#define IPC_MOVE_UP    "\x01\x00\x00\x00\x08\x00\x00\x00\x06\x40\x06\x40\x18\x00\x00\x00\x01\x00\x00\x00\x00\x00\x00\x00"
#define IPC_MOVE_STOP  "\x01\x00\x00\x00\x08\x00\x00\x00\x07\x40\x01\x00\x00\x00\x00\x00"

#define MOVE_STOP  0
#define MOVE_RIGHT 2
#define MOVE_LEFT  -2
#define MOVE_UP    3
#define MOVE_DOWN  -3

#define IPC_JUMP_POSITION "\x01\x00\x00\x00\x08\x00\x00\x00\x09\x40\x09\x40\x18\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"

#define IPC_GOTO_PRESET    "\x01\x00\x00\x00\x08\x00\x00\x00\x02\x40\x01\x00\x04\x00\x00\x00\xff\x00\x00\x00"
#define IPC_ADD_PRESET     "\x01\x00\x00\x00\x08\x00\x00\x00\x00\x40\x01\x00\x00\x00\x00\x00"
#define IPC_REMOVE_PRESET  "\x01\x00\x00\x00\x08\x00\x00\x00\x01\x40\x01\x00\x04\x00\x00\x00\xff\x00\x00\x00"

#define GOTO_PRESET 0

#define IPC_CRUISE_OFF     "\x01\x00\x00\x00\x08\x00\x00\x00\x05\x40\x01\x00\x00\x00\x00\x00"
#define IPC_CRUISE_ON      "\x01\x00\x00\x00\x08\x00\x00\x00\x04\x40\x01\x00\x00\x00\x00\x00"
#define IPC_CRUISE_PRESETS "\x01\x00\x00\x00\x08\x00\x00\x00\x03\x40\x01\x00\x08\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
#define IPC_CRUISE_360     "\x01\x00\x00\x00\x08\x00\x00\x00\x03\x40\x01\x00\x08\x00\x00\x00\x01\x00\x00\x00\x00\x00\x00\x00"

#define CRUISE_OFF     0
#define CRUISE_ON      1
#define CRUISE_PRESETS 2
#define CRUISE_360     3

#define IPC_XXX_0 "\x01\x00\x00\x00\x04\x00\x00\x00\x71\x00\x71\x00\x00\x00\x00\x00\xa1\x0e\x9a\x5e"

#define IPC_SET_ALERT_ALLOW_STATE_HUMAN "\x01\x00\x00\x00\x04\x00\x00\x00\xfb\x00\x01\x00\x18\x00\x00\x00\x01\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"

#define IPC_AI_HUMAN_DETECTION_OFF "\x01\x00\x00\x00\x08\x00\x00\x00\x3c\x10\x01\x00\x04\x00\x00\x00\x00\x00\x00\x00"
#define IPC_AI_HUMAN_DETECTION_ON  "\x01\x00\x00\x00\x08\x00\x00\x00\x3c\x10\x01\x00\x04\x00\x00\x00\x01\x00\x00\x00"

#define AI_HUMAN_DETECTION_OFF 0
#define AI_HUMAN_DETECTION_ON  1

#define IPC_AI_VEHICLE_DETECTION_OFF "\x01\x00\x00\x00\x08\x00\x00\x00\x40\x10\x01\x00\x04\x00\x00\x00\x00\x00\x00\x00"
#define IPC_AI_VEHICLE_DETECTION_ON  "\x01\x00\x00\x00\x08\x00\x00\x00\x40\x10\x01\x00\x04\x00\x00\x00\x01\x00\x00\x00"

#define AI_VEHICLE_DETECTION_OFF 0
#define AI_VEHICLE_DETECTION_ON  1

#define IPC_AI_ANIMAL_DETECTION_OFF "\x01\x00\x00\x00\x08\x00\x00\x00\x3f\x10\x01\x00\x04\x00\x00\x00\x00\x00\x00\x00"
#define IPC_AI_ANIMAL_DETECTION_ON  "\x01\x00\x00\x00\x08\x00\x00\x00\x3f\x10\x01\x00\x04\x00\x00\x00\x01\x00\x00\x00"

#define AI_ANIMAL_DETECTION_OFF 0
#define AI_ANIMAL_DETECTION_ON  1

#define IPC_AI_MOTION_DETECTION_OFF "\x01\x00\x00\x00\x08\x00\x00\x00\x41\x10\x01\x00\x04\x00\x00\x00\x00\x00\x00\x00"
#define IPC_AI_MOTION_DETECTION_ON  "\x01\x00\x00\x00\x08\x00\x00\x00\x41\x10\x01\x00\x04\x00\x00\x00\x01\x00\x00\x00"

#define AI_MOTION_DETECTION_OFF 0
#define AI_MOTION_DETECTION_ON  1

#define IPC_FACE_DETECTION_OFF "\x01\x00\x00\x00\x08\x00\x00\x00\x3d\x10\x01\x00\x04\x00\x00\x00\x00\x00\x00\x00"
#define IPC_FACE_DETECTION_ON  "\x01\x00\x00\x00\x08\x00\x00\x00\x3d\x10\x01\x00\x04\x00\x00\x00\x01\x00\x00\x00"

#define FACE_DETECTION_OFF 0
#define FACE_DETECTION_ON  1

#define IPC_MOTION_TRACKING_OFF "\x01\x00\x00\x00\x08\x00\x00\x00\x0b\x40\x01\x00\x04\x00\x00\x00\x00\x00\x00\x00"
#define IPC_MOTION_TRACKING_ON  "\x01\x00\x00\x00\x08\x00\x00\x00\x0b\x40\x01\x00\x04\x00\x00\x00\x01\x00\x00\x00"

#define MOTION_TRACKING_OFF 0
#define MOTION_TRACKING_ON  1

#define IPC_BABYCRYING_OFF "\x02\x00\x00\x00\x08\x00\x00\x00\x29\x10\x01\x00\x04\x00\x00\x00\x01\x00\x00\x00"
#define IPC_BABYCRYING_ON  "\x02\x00\x00\x00\x08\x00\x00\x00\x29\x10\x01\x00\x04\x00\x00\x00\x02\x00\x00\x00"

#define BABYCRYING_OFF 0
#define BABYCRYING_ON  1

#define IPC_MIC_OFF "\x02\x00\x00\x00\x08\x00\x00\x00\x2a\x10\x01\x00\x04\x00\x00\x00\x65\x00\x00\x00"
#define IPC_MIC_ON  "\x02\x00\x00\x00\x08\x00\x00\x00\x2a\x10\x01\x00\x04\x00\x00\x00\x64\x00\x00\x00"

#define MIC_OFF 0
#define MIC_ON  1

#define IPC_SOUND_DETECTION_OFF "\x02\x00\x00\x00\x08\x00\x00\x00\x3a\x10\x01\x00\x04\x00\x00\x00\x01\x00\x00\x00"
#define IPC_SOUND_DETECTION_ON  "\x02\x00\x00\x00\x08\x00\x00\x00\x3a\x10\x01\x00\x04\x00\x00\x00\x02\x00\x00\x00"

#define SOUND_DETECTION_OFF 0
#define SOUND_DETECTION_ON  1

#define IPC_SOUND_SENS_30 "\x02\x00\x00\x00\x08\x00\x00\x00\x3b\x10\x01\x00\x04\x00\x00\x00\x1e\x00\x00\x00"
#define IPC_SOUND_SENS_35 "\x02\x00\x00\x00\x08\x00\x00\x00\x3b\x10\x01\x00\x04\x00\x00\x00\x23\x00\x00\x00"
#define IPC_SOUND_SENS_40 "\x02\x00\x00\x00\x08\x00\x00\x00\x3b\x10\x01\x00\x04\x00\x00\x00\x28\x00\x00\x00"
#define IPC_SOUND_SENS_45 "\x02\x00\x00\x00\x08\x00\x00\x00\x3b\x10\x01\x00\x04\x00\x00\x00\x2d\x00\x00\x00"
#define IPC_SOUND_SENS_50 "\x02\x00\x00\x00\x08\x00\x00\x00\x3b\x10\x01\x00\x04\x00\x00\x00\x32\x00\x00\x00"
#define IPC_SOUND_SENS_60 "\x02\x00\x00\x00\x08\x00\x00\x00\x3b\x10\x01\x00\x04\x00\x00\x00\x3c\x00\x00\x00"
#define IPC_SOUND_SENS_70 "\x02\x00\x00\x00\x08\x00\x00\x00\x3b\x10\x01\x00\x04\x00\x00\x00\x46\x00\x00\x00"
#define IPC_SOUND_SENS_80 "\x02\x00\x00\x00\x08\x00\x00\x00\x3b\x10\x01\x00\x04\x00\x00\x00\x50\x00\x00\x00"
#define IPC_SOUND_SENS_90 "\x02\x00\x00\x00\x08\x00\x00\x00\x3b\x10\x01\x00\x04\x00\x00\x00\x5a\x00\x00\x00"

#define SOUND_SENS_30 30
#define SOUND_SENS_35 35
#define SOUND_SENS_40 40
#define SOUND_SENS_45 45
#define SOUND_SENS_50 50
#define SOUND_SENS_60 60
#define SOUND_SENS_70 70
#define SOUND_SENS_80 80
#define SOUND_SENS_90 90

#define IPC_SAVE_CONFIG "\x40\x00\x00\x00\x01\x00\x00\x00\xFF\x00\xFF\x00\x00\x00\x00\x00"
