/*
 * Copyright (c) 2023 roleo.
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
 * Dump h264 content from /dev/fshare_frame_buffer to stdout
 */

#define _GNU_SOURCE

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <getopt.h>
#include <signal.h>

#define BUF_OFFSET 228
#define BUF_SIZE 1786086
#define FRAME_HEADER_SIZE 19

#define MILLIS_25 25000

#define RESOLUTION_NONE 0
#define RESOLUTION_LOW  360
#define RESOLUTION_HIGH 1080

#define BUFFER_FILE "/dev/fshare_frame_buf"

#define FIFO_NAME_LOW "/tmp/h264_low_fifo"
#define FIFO_NAME_HIGH "/tmp/h264_high_fifo"

unsigned char IDR[]               = {0x65, 0xB8};
unsigned char NAL_START[]         = {0x00, 0x00, 0x00, 0x01};
unsigned char IDR_START[]         = {0x00, 0x00, 0x00, 0x01, 0x65, 0x88};
unsigned char PFR_START[]         = {0x00, 0x00, 0x00, 0x01, 0x41};
unsigned char SPS_START[]         = {0x00, 0x00, 0x00, 0x01, 0x67};
unsigned char PPS_START[]         = {0x00, 0x00, 0x00, 0x01, 0x68};
unsigned char N06_START[]         = {0x00, 0x00, 0x00, 0x01, 0x06};
unsigned char N06F02C_START[]     = {0x00, 0x00, 0x00, 0x01, 0x06, 0xF0, 0x2C};
unsigned char N06F002_START[]     = {0x00, 0x00, 0x00, 0x01, 0x06, 0xF0, 0x02};
unsigned char SPS_640X360[]       = {0x00, 0x00, 0x00, 0x01, 0x67, 0x4D, 0x00, 0x14,
                                       0x96, 0x54, 0x05, 0x01, 0x7B, 0xCB, 0x37, 0x01,
                                       0x01, 0x01, 0x02};
unsigned char SPS_1920X1080[]     = {0x00, 0x00, 0x00, 0x01, 0x67, 0x4D, 0x00, 0x20,
                                       0x96, 0x54, 0x03, 0xC0, 0x11, 0x2F, 0x2C, 0xDC,
                                       0x04, 0x04, 0x04, 0x08};

unsigned char *addr;                      /* Pointer to shared memory region (header) */
int resolution;
int fifo = 0;
int debug = 0;                            /* Set to 1 to debug this .c */

long long current_timestamp() {
    struct timeval te; 
    gettimeofday(&te, NULL); // get current time
    long long milliseconds = te.tv_sec*1000LL + te.tv_usec/1000; // calculate milliseconds

    return milliseconds;
}

/* Locate a string in the circular buffer */
unsigned char *cb_memmem(unsigned char *src, int src_len, unsigned char *what, int what_len)
{
    unsigned char *p;

    if (src_len >= 0) {
        p = (unsigned char*) memmem(src, src_len, what, what_len);
    } else {
        // From src to the end of the buffer
        p = (unsigned char*) memmem(src, addr + BUF_SIZE - src, what, what_len);
        if (p == NULL) {
            // And from the start of the buffer size src_len
            p = (unsigned char*) memmem(addr + BUF_OFFSET, src + src_len - (addr + BUF_OFFSET), what, what_len);
        }
    }
    return p;
}

unsigned char *cb_memmem_snal(unsigned char *src, int src_len)
{
    unsigned char *idx1, *idx2, *idx3, *ret;
    unsigned char *tmp_idx1, *tmp_idx2, *tmp_idx3;

    idx1 = cb_memmem(src, src_len, SPS_START, sizeof(SPS_START));
    idx2 = cb_memmem(src, src_len, PPS_START, sizeof(PPS_START));
    idx3 = cb_memmem(src, src_len, N06_START, sizeof(N06_START));

    if ((idx1 == NULL) && (idx2 == NULL) && (idx3 == NULL)) {
        return NULL;
    }

    if (idx1 == NULL) {
        // Move forward enough
        tmp_idx1 = idx1 + (10 * BUF_SIZE);
    } else {
        if (idx1 < src) {
            tmp_idx1 = idx1 + BUF_SIZE;
        } else {
            tmp_idx1 = idx1;
        }
    }
    if (idx2 == NULL) {
        // Move forward enough
        tmp_idx2 = idx2 + (10 * BUF_SIZE);
    } else {
        if (idx2 < src) {
            tmp_idx2 = idx2 + BUF_SIZE;
        } else {
            tmp_idx2 = idx2;
        }
    }
    if (idx3 == NULL) {
        // Move forward enough
        tmp_idx3 = idx3 + (10 * BUF_SIZE);
    } else {
        if (idx3 < src) {
            tmp_idx3 = idx3 + BUF_SIZE;
        } else {
            tmp_idx3 = idx3;
        }
    }

    if (tmp_idx1 <= tmp_idx2) {
        if (tmp_idx1 <= tmp_idx3) {
            ret = idx1;
        } else {
            ret = idx3;
        }
    } else {
        if (tmp_idx2 <= tmp_idx3) {
            ret = idx2;
        } else {
            ret = idx3;
        }
    }

    return ret;
}

unsigned char *cb_move(unsigned char *buf, int offset)
{
    buf += offset;
    if ((offset > 0) && (buf > addr + BUF_SIZE))
        buf -= (BUF_SIZE - BUF_OFFSET);
    if ((offset < 0) && (buf < addr + BUF_OFFSET))
        buf += (BUF_SIZE - BUF_OFFSET);

    return buf;
}

// The second argument is the circular buffer
int cb_memcmp(unsigned char *str1, unsigned char *str2, size_t n)
{
    int ret;

    if (str2 + n > addr + BUF_SIZE) {
        ret = memcmp(str1, str2, addr + BUF_SIZE - str2);
        if (ret != 0) return ret;
        ret = memcmp(str1 + (addr + BUF_SIZE - str2), addr + BUF_OFFSET, n - (addr + BUF_SIZE - str2));
    } else {
        ret = memcmp(str1, str2, n);
    }

    return ret;
}

// The second argument is the circular buffer
void cb2s_memcpy(unsigned char *dest, unsigned char *src, size_t n)
{
    if (src + n > addr + BUF_SIZE) {
        memcpy(dest, src, addr + BUF_SIZE - src);
        memcpy(dest + (addr + BUF_SIZE - src), addr + BUF_OFFSET, n - (addr + BUF_SIZE - src));
    } else {
        memcpy(dest, src, n);
    }
}

void sigpipe_handler(int unused)
{
    // Do nothing
}

void print_usage(char *progname)
{
    fprintf(stderr, "\nUsage: %s [-r RES] [-d]\n\n", progname);
    fprintf(stderr, "\t-r RES, --resolution RES\n");
    fprintf(stderr, "\t\tset resolution: LOW or HIGH (default HIGH)\n");
    fprintf(stderr, "\t-f, --fifo\n");
    fprintf(stderr, "\t\tenable fifo output\n");
    fprintf(stderr, "\t-d, --debug\n");
    fprintf(stderr, "\t\tenable debug\n");
    fprintf(stderr, "\t-h, --help\n");
    fprintf(stderr, "\t\tprint this help\n");
}

int main(int argc, char **argv) {
    unsigned char *buf_idx_1, *buf_idx_2;
    unsigned char *buf_idx_w, *buf_idx_tmp;
    unsigned char *buf_idx_start = NULL;
    unsigned char *sps_addr;
    int sps_len;
    FILE *fFid;
    FILE *fOut = stdout;

    int frame_res, frame_len, frame_counter = -1;
    int frame_counter_last_valid = -1;
    int frame_counter_invalid = 0;

    int i, c;
    int write_enable = 0;
    int sps_sync = 0;

    mode_t mode = 0755;

    resolution = RESOLUTION_HIGH;
    fifo = 0;
    debug = 0;

    while (1) {
        static struct option long_options[] =
        {
            {"resolution",  required_argument, 0, 'r'},
            {"fifo",  no_argument, 0, 'f'},
            {"debug",  no_argument, 0, 'd'},
            {"help",  no_argument, 0, 'h'},
            {0, 0, 0, 0}
        };
        /* getopt_long stores the option index here. */
        int option_index = 0;

        c = getopt_long (argc, argv, "r:fdh",
                         long_options, &option_index);

        /* Detect the end of the options. */
        if (c == -1)
            break;

        switch (c) {
        case 'r':
            if (strcasecmp("low", optarg) == 0) {
                resolution = RESOLUTION_LOW;
            } else if (strcasecmp("high", optarg) == 0) {
                resolution = RESOLUTION_HIGH;
            }
            break;

        case 'f':
            fprintf (stderr, "using fifo as output\n");
            fifo = 1;
            break;

        case 'd':
            fprintf (stderr, "debug on\n");
            debug = 1;
            break;

        case 'h':
            print_usage(argv[0]);
            return -1;
            break;

        case '?':
            /* getopt_long already printed an error message. */
            break;

        default:
            print_usage(argv[0]);
            return -1;
        }
    }

    setpriority(PRIO_PROCESS, 0, -10);

    sps_addr = SPS_1920X1080;
    sps_len = sizeof(SPS_1920X1080);
    if (resolution == RESOLUTION_LOW) {
        sps_addr = SPS_640X360;
        sps_len = sizeof(SPS_640X360);
    } else if (resolution == RESOLUTION_HIGH) {
        sps_addr = SPS_1920X1080;
        sps_len = sizeof(SPS_1920X1080);
    }
    sps_addr = SPS_START;
    sps_len = sizeof(SPS_START);

    // Opening an existing file
    fFid = fopen(BUFFER_FILE, "r") ;
    if ( fFid == NULL ) {
        fprintf(stderr, "could not open file %s\n", BUFFER_FILE) ;
        return -1;
    }

    // Map file to memory
    addr = (unsigned char*) mmap(NULL, BUF_SIZE, PROT_READ, MAP_SHARED, fileno(fFid), 0);
    if (addr == MAP_FAILED) {
        fprintf(stderr, "error mapping file %s\n", BUFFER_FILE);
        fclose(fFid);
        return -2;
    }
    if (debug) fprintf(stderr, "mapping file %s, size %d, to %08x\n", BUFFER_FILE, BUF_SIZE, (unsigned int) addr);

    // Closing the file
    if (debug) fprintf(stderr, "closing the file %s\n", BUFFER_FILE) ;
    fclose(fFid) ;

    if (fifo == 0) {
        char stdoutbuf[262144];

        if (setvbuf(stdout, stdoutbuf, _IOFBF, sizeof(stdoutbuf)) != 0) {
            fprintf(stderr, "Error setting stdout buffer\n");
        }
        fOut = stdout;
    } else {
        sigaction(SIGPIPE, &(struct sigaction){{sigpipe_handler}}, NULL);

        if (resolution == RESOLUTION_LOW) {
            if (debug) fprintf(stderr, "opening fifo %s\n", FIFO_NAME_LOW) ;
            unlink(FIFO_NAME_LOW);
            if (mkfifo(FIFO_NAME_LOW, mode) < 0) {
                fprintf(stderr, "mkfifo failed for file %s\n", FIFO_NAME_LOW);
                return -1;
            }
            fOut = fopen(FIFO_NAME_LOW, "w");
            if (fOut == NULL) {
                fprintf(stderr, "Error opening fifo %s\n", FIFO_NAME_LOW);
                return -1;
            }
        } else if (resolution == RESOLUTION_HIGH) {
            if (debug) fprintf(stderr, "opening fifo %s\n", FIFO_NAME_HIGH) ;
            unlink(FIFO_NAME_HIGH);
            if (mkfifo(FIFO_NAME_HIGH, mode) < 0) {
                fprintf(stderr, "mkfifo failed for file %s\n", FIFO_NAME_HIGH);
                return -1;
            }
            fOut = fopen(FIFO_NAME_HIGH, "w");
            if (fOut == NULL) {
                fprintf(stderr, "Error opening fifo %s\n", FIFO_NAME_HIGH);
                return -1;
            }
        }
    }

    memcpy(&i, addr + 16, sizeof(i));
    buf_idx_w = addr + BUF_OFFSET + i;
    buf_idx_1 = buf_idx_w;

    if (debug) fprintf(stderr, "starting capture main loop\n");

    // Infinite loop
    while (1) {
        memcpy(&i, addr + 16, sizeof(i));
        buf_idx_w = addr + BUF_OFFSET + i;
//        if (debug) fprintf(stderr, "buf_idx_w: %08x\n", (unsigned int) buf_idx_w);
        buf_idx_tmp = cb_memmem_snal(buf_idx_1, buf_idx_w - buf_idx_1);
        if (buf_idx_tmp == NULL) {
            usleep(MILLIS_25);
            continue;
        } else {
            buf_idx_1 = buf_idx_tmp;
        }
//        if (debug) fprintf(stderr, "found buf_idx_1: %08x\n", (unsigned int) buf_idx_1);

        if (cb_memcmp(N06F002_START, buf_idx_1, sizeof(N06F002_START)) == 0) {
            buf_idx_tmp += 10;
        }
//        buf_idx_tmp = cb_memmem(buf_idx_1 + 1, buf_idx_w - (buf_idx_1 + 1), NAL_START, sizeof(NAL_START));
        buf_idx_tmp = cb_memmem_snal(buf_idx_tmp + 1, buf_idx_w - (buf_idx_tmp + 1));
        if (buf_idx_tmp == NULL) {
            usleep(MILLIS_25);
            continue;
        } else {
            buf_idx_2 = buf_idx_tmp;
        }
//        if (debug) fprintf(stderr, "found buf_idx_2: %08x\n", (unsigned int) buf_idx_2);

        if ((write_enable) && (sps_sync)) {
//            if (debug) fprintf(stderr, "%lld: writing frame n. %d - len %d\n", current_timestamp(), frame_counter, frame_len);
            if (buf_idx_start + frame_len > addr + BUF_SIZE) {
                fwrite(buf_idx_start, 1, addr + BUF_SIZE - buf_idx_start, fOut);
                fwrite(addr + BUF_OFFSET, 1, frame_len - (addr + BUF_SIZE - buf_idx_start), fOut);
            } else {
                fwrite(buf_idx_start, 1, frame_len, fOut);
            }
        }

        if (cb_memcmp(sps_addr, buf_idx_1, sps_len) == 0) {
            // SPS frame
            write_enable = 1;
            sps_sync = 1;
            buf_idx_1 = cb_move(buf_idx_1, - (6 + FRAME_HEADER_SIZE));
            if (buf_idx_1[7] == 8) {
                frame_res = RESOLUTION_LOW;
            } else if (buf_idx_1[7] == 4) {
                frame_res = RESOLUTION_HIGH;
            } else {
                frame_res = RESOLUTION_NONE;
            }
            if (frame_res == resolution) {
                cb2s_memcpy((unsigned char *) &frame_len, buf_idx_1, 4);
                frame_len -= 6;                                                              // -6 only for SPS
                frame_counter = (int) buf_idx_1[16] + (int) buf_idx_1[17] * 256;
                if ((frame_counter - frame_counter_last_valid > 20) ||
                            ((frame_counter < frame_counter_last_valid) && (frame_counter - frame_counter_last_valid > -65515))) {

                    if (debug) fprintf(stderr, "%lld: incorrect frame counter - frame_counter: %d - frame_counter_last_valid: %d\n",
                                current_timestamp(), frame_counter, frame_counter_last_valid);
                    frame_counter_invalid++;
                    // Check if sync is lost
                    if (frame_counter_invalid > 40) {
                        if (debug) fprintf(stderr, "%lld: sync lost\n", current_timestamp());
                        frame_counter_last_valid = frame_counter;
                        frame_counter_invalid = 0;
                    } else {
                        write_enable = 0;
                    }
                } else {
                    frame_counter_invalid = 0;
                    frame_counter_last_valid = frame_counter;
                }
            } else {
                write_enable = 0;
            }
            if (debug) fprintf(stderr, "%lld: SPS   detected - frame_len: %d - frame_counter: %d - frame_counter_last_valid: %d - resolution: %d\n",
                        current_timestamp(), frame_len, frame_counter,
                        frame_counter_last_valid, frame_res);
            buf_idx_1 = cb_move(buf_idx_1, 6 + FRAME_HEADER_SIZE);
            buf_idx_start = buf_idx_1;
        } else if ((cb_memcmp(PPS_START, buf_idx_1, sizeof(PPS_START)) == 0) ||
                    (cb_memcmp(N06_START, buf_idx_1, sizeof(N06_START)) == 0)) {
            // PPS, IDR and PFR frames
            write_enable = 1;
            buf_idx_1 = cb_move(buf_idx_1, -FRAME_HEADER_SIZE);
            if (buf_idx_1[7] == 8) {
                frame_res = RESOLUTION_LOW;
            } else if (buf_idx_1[7] == 4) {
                frame_res = RESOLUTION_HIGH;
            } else {
                frame_res = RESOLUTION_NONE;
            }
            if (frame_res == resolution) {
                cb2s_memcpy((unsigned char *) &frame_len, buf_idx_1, 4);
                frame_counter = (int) buf_idx_1[16] + (int) buf_idx_1[17] * 256;
                if ((frame_counter - frame_counter_last_valid > 20) ||
                            ((frame_counter < frame_counter_last_valid) && (frame_counter - frame_counter_last_valid > -65515))) {

                    if (debug) fprintf(stderr, "%lld: incorrect frame counter - frame_counter: %d - frame_counter_last_valid: %d\n",
                                current_timestamp(), frame_counter, frame_counter_last_valid);
                    frame_counter_invalid++;
                    // Check if sync is lost
                    if (frame_counter_invalid > 40) {
                        if (debug) fprintf(stderr, "%lld: sync lost\n", current_timestamp());
                        frame_counter_last_valid = frame_counter;
                        frame_counter_invalid = 0;
                    } else {
                        write_enable = 0;
                    }
                } else {
                    int fl = (frame_counter + 65536 - frame_counter_last_valid) % 65536;
                    if (fl > 1) {
                        fprintf(stderr, "%lld - %d frame(s) lost\n", current_timestamp(), fl);
                    }
                    frame_counter_invalid = 0;
                    frame_counter_last_valid = frame_counter;
                }
            } else {
                write_enable = 0;
            }
            if (debug) fprintf(stderr, "%lld: frame detected - frame_len: %d - frame_counter: %d - frame_counter_last_valid: %d - resolution: %d\n",
                        current_timestamp(), frame_len, frame_counter,
                        frame_counter_last_valid, frame_res);
            buf_idx_1 = cb_move(buf_idx_1, FRAME_HEADER_SIZE);
            buf_idx_start = buf_idx_1;
        } else {
            write_enable = 0;
        }

        buf_idx_1 = buf_idx_2;
    }

    // Unreacheable path

    if (fifo == 1) {
        if (resolution == RESOLUTION_LOW) {
            fclose(fOut);
            unlink(FIFO_NAME_LOW);
        } else if (resolution == RESOLUTION_HIGH) {
            fclose(fOut);
            unlink(FIFO_NAME_HIGH);
        }
    }

    // Unmap file from memory
    if (munmap(addr, BUF_SIZE) == -1) {
        if (debug) fprintf(stderr, "error munmapping file");
    } else {
        if (debug) fprintf(stderr, "unmapping file %s, size %d, from %08x\n", BUFFER_FILE, BUF_SIZE, (unsigned int) addr);
    }

    return 0;
}
