/*
 * Copyright (c) 2025 roleo.
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

/*
 * Dump h264, h265 and aac content from /dev/shm/fshare_frame_buffer and
 * copy it to a queue.
 * Then send the queue to live555.
 */

#ifndef _R_RTSP_SERVER_H
#define _R_RTSP_SERVER_H

//#define _GNU_SOURCE

#include <queue>
#include <vector>

#include <sys/types.h>

#define BUFFER_FILE "/dev/fshare_frame_buf"
#define BUFFER_SHM "fshare_frame_buf"

#define MAX_QUEUE_SIZE 20

#define BUF_OFFSET 230 //228
#define FRAME_HEADER_SIZE 19

#define MILLIS_10 10000
#define MILLIS_25 25000

#define RESOLUTION_NONE 0
#define RESOLUTION_LOW  360
#define RESOLUTION_HIGH 1080
#define RESOLUTION_BOTH 1440

#define TYPE_NONE 0
#define TYPE_LOW  360
#define TYPE_HIGH 1080
#define TYPE_AAC 65521

#define RESOLUTION_FHD  1080
#define RESOLUTION_3K   1296

#define OUTPUT_BUFFER_SIZE_LOW  131072
#define OUTPUT_BUFFER_SIZE_HIGH 524288
#define OUTPUT_BUFFER_SIZE_AUDIO 32768

#define CODEC_NONE 0
#define CODEC_H264 264
#define CODEC_H265 265

typedef struct
{
    unsigned char *buffer;                  // pointer to the base of the input buffer
    char filename[256];                     // name of the buffer file
    unsigned int size;                      // size of the buffer file
    unsigned int offset;                    // offset where stream starts
    unsigned char *read_index;              // read absolute index
} cb_input_buffer;

typedef struct
{
    std::vector<unsigned char> frame;
    uint32_t time;
    int counter;
} output_frame;

typedef struct
{
    std::queue<output_frame> frame_queue;
    pthread_mutex_t mutex;
    unsigned int type;
} output_queue;

struct __attribute__((__packed__)) frame_header {
    uint32_t len;
    uint32_t counter;
    uint32_t time;
    uint16_t type;
    uint16_t stream_counter;
};

struct __attribute__((__packed__)) frame_header_19 {
    uint32_t len;
    uint16_t counter;
    uint16_t type;
    uint32_t u1;
    uint32_t time;
    uint16_t stream_counter;
    uint8_t u2;
};

struct stream_type_s {
    int codec_low;
    int codec_high;
    int sps_type_low;
    int sps_type_high;
    int vps_type_low;
    int vps_type_high;
};

long long current_timestamp();

#endif
