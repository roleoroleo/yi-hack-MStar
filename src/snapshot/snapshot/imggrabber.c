/*
 * Copyright (c) 2022 roleo.
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
 * Reads the YUV buffer, extracts the last frame and converts it to jpg.
 */

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <dirent.h>
#include <math.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <getopt.h>
#include <errno.h>
#include <jpeglib.h>
#include <time.h>

#ifdef HAVE_AV_CONFIG_H
#undef HAVE_AV_CONFIG_H
#endif

#include "libavcodec/avcodec.h"

#include "convert2jpg.h"
#include "water_mark.h"

#define GENERIC 0
#define H305R 1

#define RESOLUTION_LOW 360
#define RESOLUTION_HIGH 1080

#define W_LOW 640
#define H_LOW 360
#define W_HIGH 1920
#define H_HIGH 1088

#define W_MB 16
#define H_MB 16
#define W_B 4
#define H_B 4

#define UV_OFFSET_LOW 0x3c00
#define UV_OFFSET_HIGH 0x0023a000

#define PROC_FILE "/proc/umap/vb"

#define PATH_RES_LOW  "/home/yi-hack/etc/wm_res/low/wm_540p_"
#define PATH_RES_HIGH "/home/yi-hack/etc/wm_res/high/wm_540p_"

#define FF_INPUT_BUFFER_PADDING_SIZE 32

typedef struct {
    int sps_addr;
    int sps_len;
    int pps_addr;
    int pps_len;
    int vps_addr;
    int vps_len;
    int idr_addr;
    int idr_len;
} frame;

struct __attribute__((__packed__)) frame_header {
    uint32_t len;
    uint32_t counter;
    uint32_t time;
    uint16_t type;
    uint16_t stream_counter;
};

int debug = 0;

/**
 * The image is stored in this manner:
 * - Y component is at the base address, 8 bpp, organized in macro block 16 x 16
 * - every macroblock is splitted in 4 sub blocks 8 x 8
 * - UV component is at 0x23a000 and, 64 bytes U followed by 64 bytes V (8 x 8)
 * This function reuse input buffer to save memory.
 */
int img2YUV(unsigned char *bufIn, int size, int width, int height)
{
    unsigned char bufOut[width*H_MB];
    int i, j, k, l;
    int r, c;
    unsigned char *ptr, *ptrIn, *ptrOut;

    ptrIn = bufIn;

    for (i=0; i<height/H_MB; i++) {
        ptrOut = bufOut;
        for (j=0; j<width/W_MB; j++) {
            for (k=0; k<4; k++) {
                for (l=0; l<W_MB*H_MB/4; l++) {
                    r = l/(W_MB/2);
                    c = l%(W_MB/2);
                    ptr = ptrOut + j * W_MB + ((k%2) * W_MB/2) + ((k/2) * H_MB/2 * width) +
                        r * width + c;
                    *ptr = *ptrIn;
                    ptrIn++;
                }
            }
        }
        memmove(bufIn + i * width * H_MB, bufOut, width * H_MB);
    }

    ptrIn = bufIn + UV_OFFSET_HIGH;

    for (i=0; i<height/H_MB; i++) {
        ptrOut = bufOut;
        for (j=0; j<width/W_MB; j++) {
            for (l=0; l<W_MB*H_MB/4; l++) {
                r = l/(W_MB/2);
                c = l%(W_MB/2);
                ptr = ptrOut + j * W_MB + r * width + (c * 2);
                *ptr = *ptrIn;
                ptrIn++;
            }
            for (l=0; l<W_MB*H_MB/4; l++) {
                r = l/(W_MB/2);
                c = l%(W_MB/2);
                ptr = ptrOut + j * W_MB + r * width + (c * 2) + 1;
                *ptr = *ptrIn;
                ptrIn++;
            }
        }
        memmove(bufIn + width * height + i * width * H_MB / 2, bufOut, width * H_MB / 2);
    }

    return width * height * 3 / 2;
}

/**
 * The image is stored in this manner:
 * - both Y and UV are stored in block 4 x 4
 * - Y component is at the base address, 8 bpp
 * - UV component is at 0x23a000, 1 byte U followed by 1 byte V
 * - every line is left/right reversed (bytes 3, 2, 1 and 0)
 * This function reuse input buffer to save memory.
 */
int img2YUV_2(unsigned char *bufIn, int size, int width, int height)
{
    unsigned char bufOut[width*H_MB];
    int i, j, k, l;
    int r, c;
    unsigned char *ptr, *ptrIn, *ptrOut;

    ptrIn = bufIn;

    for (i=0; i<height/H_B; i++) {
        ptrOut = bufOut;
        for (j=0; j<width/W_B; j++) {
            for (l=0; l<W_B*H_B; l++) {
                r = l/(W_B);
                c = l%(W_B);
                ptr = ptrOut + j * W_B + (3 - r) * width + (3 - c);
                *ptr = *ptrIn;
                ptrIn++;
            }
        }
        memmove(bufIn + i * width * H_B, bufOut, width * H_B);
    }

    ptrIn = bufIn + UV_OFFSET_HIGH;

    for (i=0; i<(height/2)/H_B; i++) {
        ptrOut = bufOut;
        for (j=0; j<(width/2)/W_B; j++) {
            for (l=0; l<W_B*H_B; l++) {
                r = l/(W_B);
                c = l%(W_B);
                ptr = ptrOut + (j * W_B + r * width/2 + 3 - c) * 2;
                *(ptr + 1) = *ptrIn;
                ptrIn++;
                *ptr = *ptrIn;
                ptrIn++;
            }
        }
        memmove(bufIn + width * height + i * width * H_B, bufOut, width * H_B);
    }

    return width * height * 3 / 2;
}

// Returns the 1st process id corresponding to pname
int pidof(const char *pname)
{
    DIR *dirp;
    FILE *fp;
    struct dirent *entry;
    char path[1024], read_buf[1024];
    int ret = 0;

    dirp = opendir ("/proc/");
    if (dirp == NULL) {
        fprintf(stderr, "error opening /proc");
        return 0;
    }

    while ((entry = readdir (dirp)) != NULL) {
        if (atoi(entry->d_name) > 0) {
            sprintf(path, "/proc/%s/comm", entry->d_name);

              /* A file may not exist, Ait may have been removed.
               * dut to termination of the process. Actually we need to
               * make sure the error is actually file does not exist to
               * be accurate.
               */
              fp = fopen (path, "r");
              if (fp != NULL) {
                  fscanf (fp, "%s", read_buf);
                  if (strcmp (read_buf, pname) == 0) {
                      ret = atoi(entry->d_name);
                      fclose (fp);
                      break;
                  }
                  fclose (fp);
              }
          }
    }

    closedir (dirp);
    return ret;
}

// Converts virtual address to physical address
unsigned int rmm_virt2phys(unsigned int inAddr) {
    int pid;
    unsigned int outAddr;
    char sInAddr[16];
    char sMaps[1024];
    FILE *fMaps;
    char *p;
    char *line;
    size_t lineSize;

    line = (char  *) malloc(1024);

    pid = pidof("rmm");
    sprintf(sMaps, "/proc/%d/maps", pid);
    fMaps = fopen(sMaps, "r");
    sprintf(sInAddr, "%08x", inAddr);
    while (getline(&line, &lineSize, fMaps) != -1) {
        if (strncmp(line, sInAddr, 8) == 0)
            break;
    }

    p = line;
    p = strchr(p, ' ');
    p++;
    p = strchr(p, ' ');
    p++;
    p[8] = '\0';
    sscanf(p, "%x", &outAddr);
    free(line);
    fclose(fMaps);

    return outAddr;
}

int frame_decode(unsigned char *outbuffer, unsigned char *p, int length, int h26x)
{
    AVCodec *codec;
    AVCodecContext *c= NULL;
    AVFrame *picture;
    int got_picture, len;
    FILE *fOut;
    uint8_t *inbuf;
    AVPacket avpkt;
    int i, j, size;

//////////////////////////////////////////////////////////
//                    Reading H264                      //
//////////////////////////////////////////////////////////

    if (debug) fprintf(stderr, "Starting decode\n");

    av_init_packet(&avpkt);

    if (h26x == 4) {
        codec = avcodec_find_decoder(AV_CODEC_ID_H264);
        if (!codec) {
            if (debug) fprintf(stderr, "Codec h264 not found\n");
            return -2;
        }
    } else {
        codec = avcodec_find_decoder(AV_CODEC_ID_HEVC);
        if (!codec) {
            if (debug) fprintf(stderr, "Codec hevc not found\n");
            return -2;
        }
    }

    c = avcodec_alloc_context3(codec);
    picture = av_frame_alloc();

    if((codec->capabilities) & AV_CODEC_CAP_TRUNCATED)
        (c->flags) |= AV_CODEC_FLAG_TRUNCATED;

    if (avcodec_open2(c, codec, NULL) < 0) {
        if (debug) fprintf(stderr, "Could not open codec h264\n");
        av_free(c);
        return -2;
    }

    // inbuf is already allocated in the main function
    inbuf = p;
    memset(inbuf + length, 0, FF_INPUT_BUFFER_PADDING_SIZE);

    // Get only 1 frame
    memcpy(inbuf, p, length);
    avpkt.size = length;
    avpkt.data = inbuf;

    // Decode frame
    if (debug) fprintf(stderr, "Decode frame\n");
    if (c->codec_type == AVMEDIA_TYPE_VIDEO ||
         c->codec_type == AVMEDIA_TYPE_AUDIO) {

        len = avcodec_send_packet(c, &avpkt);
        if (len < 0 && len != AVERROR(EAGAIN) && len != AVERROR_EOF) {
            if (debug) fprintf(stderr, "Error decoding frame\n");
            return -2;
        } else {
            if (len >= 0)
                avpkt.size = 0;
            len = avcodec_receive_frame(c, picture);
            if (len >= 0)
                got_picture = 1;
        }
    }
    if(!got_picture) {
        if (debug) fprintf(stderr, "No input frame\n");
        av_frame_free(&picture);
        avcodec_close(c);
        av_free(c);
        return -2;
    }

    if (debug) fprintf(stderr, "Writing yuv buffer\n");
    memset(outbuffer, 0x80, c->width * c->height * 3 / 2);
    memcpy(outbuffer, picture->data[0], c->width * c->height);
    for(i=0; i<c->height/2; i++) {
        for(j=0; j<c->width/2; j++) {
            outbuffer[c->width * c->height + c->width * i +  2 * j] = *(picture->data[1] + i * picture->linesize[1] + j);
            outbuffer[c->width * c->height + c->width * i +  2 * j + 1] = *(picture->data[2] + i * picture->linesize[2] + j);
        }
    }

    // Clean memory
    if (debug) fprintf(stderr, "Cleaning ffmpeg memory\n");
    av_frame_free(&picture);
    avcodec_close(c);
    av_free(c);

    return 0;
}

int add_watermark(unsigned char *buffer, int w_res, int h_res, struct tm *watermark_tm)
{
    char path_res[1024];
    FILE *fBuf;
    WaterMarkInfo WM_info;

    if (w_res != W_LOW) {
        strcpy(path_res, PATH_RES_HIGH);
    } else {
        strcpy(path_res, PATH_RES_LOW);
    }

    if (WMInit(&WM_info, path_res) < 0) {
        fprintf(stderr, "water mark init error\n");
        return -1;
    } else {
        if (w_res != W_LOW) {
            AddWM(&WM_info, w_res, h_res, buffer,
                buffer + w_res*h_res, w_res-460, h_res-40, watermark_tm);
        } else {
            AddWM(&WM_info, w_res, h_res, buffer,
                buffer + w_res*h_res, w_res-230, h_res-20, watermark_tm);
        }
        WMRelease(&WM_info);
    }

    return 0;
}

pid_t proc_find(const char* process_name, pid_t process_pid)
{
    DIR* dir;
    struct dirent* ent;
    char* endptr;
    char buf[512];

    if (!(dir = opendir("/proc"))) {
        perror("can't open /proc");
        return -1;
    }

    while((ent = readdir(dir)) != NULL) {
        /* if endptr is not a null character, the directory is not
         * entirely numeric, so ignore it */
        long lpid = strtol(ent->d_name, &endptr, 10);
        if (*endptr != '\0') {
            continue;
        }

        /* try to open the cmdline file */
        snprintf(buf, sizeof(buf), "/proc/%ld/cmdline", lpid);
        FILE* fp = fopen(buf, "r");

        if (fp) {
            if (fgets(buf, sizeof(buf), fp) != NULL) {
                /* check the first token in the file, the program name */
                char* first = strtok(buf, " ");
                if ((strcmp(first, process_name) == 0) && ((pid_t) lpid != process_pid)) {
                    fclose(fp);
                    closedir(dir);
                    return (pid_t) lpid;
                }
            }
            fclose(fp);
        }

    }

    closedir(dir);
    return -1;
}

void print_usage(char *progname)
{
    fprintf(stderr, "\nUsage: %s [-m MODEL] [-r RES] [-w] [-t watermark_time] [-d]\n\n", progname);
    fprintf(stderr, "\t-m, --model MODEL\n");
    fprintf(stderr, "\t\tset model: unused parameter\n");
    fprintf(stderr, "\t-f, --file FILE\n");
    fprintf(stderr, "\t\tRead frame from file FILE\n");
    fprintf(stderr, "\t-r RES, --resolution RES\n");
    fprintf(stderr, "\t\tset resolution: LOW or HIGH (default HIGH)\n");
    fprintf(stderr, "\t-w, --watermark\n");
    fprintf(stderr, "\t\tadd watermark to image\n");
    fprintf(stderr, "\t-t, --watermark_time\n");
    fprintf(stderr, "\t\tstring to print\n");
    fprintf(stderr, "\t-d, --debug\n");
    fprintf(stderr, "\t\tenable debug\n");
}

void fillCheck(unsigned char *check, int checkSize, unsigned char *buf, int bufSize)
{
    int i;

    for (i=0; i<checkSize; i++) {
        check[i] = buf[bufSize/checkSize*i];
    }
}

int main(int argc, char **argv)
{
    int model = GENERIC;
    int c;
    int errno;
    char *endptr;
    const char memDevice[] = "/dev/mem";
    char file[256];
    int resolution = RESOLUTION_HIGH;
    int watermark = 0;
    int watermark_time = 0;
    struct tm watermark_tm;
    int priority = 0;
    int width, height;
    FILE *fPtr, *fLen, *fHF;
    int fMem;
    unsigned int ivAddr, ipAddr;
    unsigned int size;
    unsigned char *addr;
    unsigned char *bufferyuv, *bufferh26x;
    unsigned char check1[64], check2[64];
    int outlen;

    WaterMarkInfo WM_info;

    struct frame_header fh, fhs, fhp, fhv, fhi;
    unsigned char *fhs_addr, *fhp_addr, *fhv_addr, *fhi_addr;

    int sps_start_found = -1, sps_end_found = -1;
    int pps_start_found = -1, pps_end_found = -1;
    int vps_start_found = -1, vps_end_found = -1;
    int idr_start_found = -1;
    int i, j, f, start_code;
    unsigned char *h26x_file_buffer;
    long h26x_file_size;
    size_t nread;

    memset(file, '\0', sizeof(file));

    while (1) {
        static struct option long_options[] =
        {
            {"model",     required_argument, 0, 'm'},
            {"file",      required_argument, 0, 'f'},
            {"resolution",  required_argument, 0, 'r'},
            {"watermark",  no_argument, 0, 'w'},
            {"watermark_time",  required_argument, 0, 't'},
            {"priority",  required_argument, 0, 'p'},
            {"debug",  no_argument, 0, 'd'},
            {"help",  no_argument, 0, 'h'},
            {0, 0, 0, 0}
        };
        /* getopt_long stores the option index here. */
        int option_index = 0;

        c = getopt_long (argc, argv, "m:f:r:wt:p:dh",
                         long_options, &option_index);

        /* Detect the end of the options. */
        if (c == -1)
            break;

        switch (c) {
        case 'm':
            if (strcasecmp("h305r", optarg) == 0) {
                model = H305R;
            } else {
                model = GENERIC;
            }
            break;

        case 'f':
            if (strlen(optarg) < sizeof(file)) {
                strcpy(file, optarg);
            }

        case 'r':
            if (strcasecmp("low", optarg) == 0) {
                resolution = RESOLUTION_LOW;
            } else if (strcasecmp("high", optarg) == 0) {
                resolution = RESOLUTION_HIGH;
            }
            break;

        case 'w':
            fprintf (stderr, "watermark on\n");
            watermark = 1;
            break;

        case 't':
            {
                int d0, d1, d2, d3, d4, d5, d6;
                d0 = sscanf(optarg, "%d-%d-%d %d:%d:%d", &d1, &d2, &d3, &d4, &d5 ,&d6);
                if (d0 == 6) {
                    watermark_tm.tm_year = d1 - 1900;
                    watermark_tm.tm_mon = d2 - 1;
                    watermark_tm.tm_mday = d3;
                    watermark_tm.tm_hour = d4;
                    watermark_tm.tm_min = d5;
                    watermark_tm.tm_sec = d6;
                    watermark_time = 1;
                } else {
                    print_usage(argv[0]);
                    exit(EXIT_FAILURE);
                }
                break;
            }
        case 'p':
            errno = 0;    /* To distinguish success/failure after call */
            priority = strtol(optarg, &endptr, 10);

            /* Check for various possible errors */
            if ((errno == ERANGE && (priority == LONG_MAX || priority == LONG_MIN)) || (errno != 0 && priority == 0)) {
                print_usage(argv[0]);
                exit(EXIT_FAILURE);
            }
            if (endptr == optarg) {
                print_usage(argv[0]);
                exit(EXIT_FAILURE);
            }
            if ((priority > 19) || (priority < -20)) {
                print_usage(argv[0]);
                exit(EXIT_FAILURE);
            }
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

    if (debug) fprintf(stderr, "Starting program\n");

    // Set low priority
    if (debug) fprintf(stderr, "Setting process priority to %d\n", priority);
    setpriority(PRIO_PROCESS, 0, priority);

    // Check if snapshot is disabled
    if (access("/tmp/snapshot.disabled", F_OK ) == 0 ) {
        fprintf(stderr, "Snapshot is disabled\n");
        return 0;
    }

    // Check if snapshot is low res
    if (access("/tmp/snapshot.low", F_OK ) == 0 ) {
        fprintf(stderr, "Snapshot is low res\n");
        resolution = RESOLUTION_LOW;
    }

    // Check if the process is already running
    pid_t my_pid = getpid();
    if (proc_find(basename(argv[0]), my_pid) != -1) {
        fprintf(stderr, "Process is already running\n");
        return 0;
    }

    if (resolution == RESOLUTION_LOW) {
        width = W_LOW;
        height = H_LOW;
    } else {
        width = W_HIGH;
        height = H_HIGH;
    }

    if (file[0] == '\0') {
        if (debug) fprintf(stderr, "Resolution: %d\n", resolution);

        if (model == H305R) {
            if (resolution == RESOLUTION_LOW) {
                if (debug) fprintf(stderr, "VMFE0\n");
                fPtr = fopen("/proc/mstar/OMX/VMFE0/ENCODER_INFO/IBUF_pBuffer", "r");
                fLen = fopen("/proc/mstar/OMX/VMFE0/ENCODER_INFO/IBUF_nAllocLen", "r");
            } else {
                if (debug) fprintf(stderr, "VVHE0\n");
                fPtr = fopen("/proc/mstar/OMX/VVHE0/ENCODER_INFO/IBUF_pBuffer", "r");
                fLen = fopen("/proc/mstar/OMX/VVHE0/ENCODER_INFO/IBUF_nAllocLen", "r");
            }
        } else {
            if (resolution == RESOLUTION_LOW) {
                if (debug) fprintf(stderr, "VMFE1\n");
                fPtr = fopen("/proc/mstar/OMX/VMFE1/ENCODER_INFO/IBUF_pBuffer", "r");
                fLen = fopen("/proc/mstar/OMX/VMFE1/ENCODER_INFO/IBUF_nAllocLen", "r");
            } else {
                if (debug) fprintf(stderr, "VMFE0\n");
                fPtr = fopen("/proc/mstar/OMX/VMFE0/ENCODER_INFO/IBUF_pBuffer", "r");
                fLen = fopen("/proc/mstar/OMX/VMFE0/ENCODER_INFO/IBUF_nAllocLen", "r");
            }
        }
    if ((fPtr == NULL) || (fLen == NULL)) {
            fprintf(stderr, "Unable to open /proc files\n");
            return -1;
        }

        fscanf(fPtr, "%x", &ivAddr);
        fclose(fPtr);
        fscanf(fLen, "%d", &size);
        fclose(fLen);

        ipAddr = rmm_virt2phys(ivAddr);

        if (debug) fprintf(stderr, "vaddr: 0x%08x - paddr: 0x%08x - size: %u\n", ivAddr, ipAddr, size);

        // allocate buffer memory (header + data buffer)
        bufferyuv = (unsigned char *) malloc(size * sizeof(unsigned char));

        // open /dev/mem and error checking
        fMem = open(memDevice, O_RDONLY); // | O_SYNC);
        if (fMem < 0) {
            fprintf(stderr, "Failed to open the /dev/mem\n");
            return -2;
        }

        // mmap() the opened /dev/mem
        addr = (unsigned char *) (mmap(NULL, size, PROT_READ, MAP_SHARED, fMem, ipAddr));
        if (addr == MAP_FAILED) {
            fprintf(stderr, "Failed to map memory\n");
            return -3;
        }

        // close the character device
        close(fMem);

        if (debug) fprintf(stderr, "copy buffer: len %d\n", size);
        memcpy(bufferyuv, addr, size);
        memset(check1, '\0', sizeof(check1));
        fillCheck(check2, sizeof(check2), bufferyuv, size);

        while (memcmp(check1, check2, sizeof(check1)) != 0) {
            if (debug) fprintf(stderr, "copy again buffer: len %d\n", size);
            memcpy(bufferyuv, addr, size);
            memcpy(check1, check2, sizeof(check1));
            fillCheck(check2, sizeof(check2), bufferyuv, size);
        }

        if (resolution == RESOLUTION_LOW) {
            // The buffer contains YUV NV12 image but the UV part is not in the
            // right position. We need to move it.
            if (debug) fprintf(stderr, "convert YUV image\n");
            memmove(bufferyuv + W_LOW * H_LOW, bufferyuv + W_LOW * H_LOW + UV_OFFSET_LOW,
                    W_LOW * H_LOW / 2);
        } else {
            // The buffer contains YUV NV21 image saved in blocks.
            // We need to convert to standard YUV NV12 image.
            if (debug) fprintf(stderr, "convert YUV image\n");
            if (model == H305R) {
                outlen = img2YUV_2(bufferyuv, size, W_HIGH, H_HIGH);
            } else {
                outlen = img2YUV(bufferyuv, size, W_HIGH, H_HIGH);
            }
        }
    } else {
        // Read frames from h26x file
        fhs.len = 0;
        fhp.len = 0;
        fhv.len = 0;
        fhi.len = 0;
        fhs_addr = NULL;
        fhp_addr = NULL;
        fhv_addr = NULL;
        fhi_addr = NULL;

        fHF = fopen(file, "r");
        if ( fHF == NULL ) {
            fprintf(stderr, "Could not get size of %s\n", file);
            return -4;
        }
        fseek(fHF, 0, SEEK_END);
        h26x_file_size = ftell(fHF);
        fseek(fHF, 0, SEEK_SET);
        h26x_file_buffer = (unsigned char *) malloc(h26x_file_size);
        nread = fread(h26x_file_buffer, 1, h26x_file_size, fHF);
        fclose(fHF);
        if (debug) fprintf(stderr, "The size of the file is %d\n", h26x_file_size);

        if (nread != h26x_file_size) {
            fprintf(stderr, "Read error %s\n", file);
            return -5;
        }

        for (f=0; f<h26x_file_size; i++) {
            for (i=f; i<h26x_file_size; i++) {
                if(h26x_file_buffer[i] == 0 && h26x_file_buffer[i+1] == 0 && h26x_file_buffer[i+2] == 0 && h26x_file_buffer[i+3] == 1) {
                    start_code = 4;
                } else {
                    continue;
                }

                if ((h26x_file_buffer[i+start_code]&0x7E) == 0x40) {
                    vps_start_found = i;
                    break;
                } else if (((h26x_file_buffer[i+start_code]&0x1F) == 0x7) || ((h26x_file_buffer[i+start_code]&0x7E) == 0x42)) {
                    sps_start_found = i;
                    break;
                } else if (((h26x_file_buffer[i+start_code]&0x1F) == 0x8) || ((h26x_file_buffer[i+start_code]&0x7E) == 0x44)) {
                    pps_start_found = i;
                    break;
                } else if (((h26x_file_buffer[i+start_code]&0x1F) == 0x5) || ((h26x_file_buffer[i+start_code]&0x7E) == 0x26)) {
                    idr_start_found = i;
                    break;
                }
            }

            for (j = i + 4; j<h26x_file_size; j++) {
                if (h26x_file_buffer[j] == 0 && h26x_file_buffer[j+1] == 0 && h26x_file_buffer[j+2] == 0 && h26x_file_buffer[j+3] == 1) {
                    start_code = 4;
                } else {
                    continue;
                }

                if ((h26x_file_buffer[j+start_code]&0x7E) == 0x42) {
                    vps_end_found = j;
                    break;
                } else if (((h26x_file_buffer[j+start_code]&0x1F) == 0x8) || ((h26x_file_buffer[j+start_code]&0x7E) == 0x44)) {
                    sps_end_found = j;
                    break;
                } else if (((h26x_file_buffer[j+start_code]&0x1F) == 0x5) || ((h26x_file_buffer[j+start_code]&0x7E) == 0x26)) {
                    pps_end_found = j;
                    break;
                }
            }
            f = j;
        }

        if ((sps_start_found >= 0) && (pps_start_found >= 0) && (idr_start_found >= 0) &&
                (sps_end_found >= 0) && (pps_end_found >= 0)) {

            if ((vps_start_found >= 0) && (vps_end_found >= 0)) {
                fhv.len = vps_end_found - vps_start_found;
                fhv_addr = &h26x_file_buffer[vps_start_found];
            }
            fhs.len = sps_end_found - sps_start_found;
            fhp.len = pps_end_found - pps_start_found;
            fhi.len = h26x_file_size - idr_start_found;
            fhs_addr = &h26x_file_buffer[sps_start_found];
            fhp_addr = &h26x_file_buffer[pps_start_found];
            fhi_addr = &h26x_file_buffer[idr_start_found];

            if (debug) {
                fprintf(stderr, "Found SPS at %d, len %d\n", sps_start_found, fhs.len);
                fprintf(stderr, "Found PPS at %d, len %d\n", pps_start_found, fhp.len);
                if (fhv_addr != NULL) {
                    fprintf(stderr, "Found VPS at %d, len %d\n", vps_start_found, fhv.len);
                }
                fprintf(stderr, "Found IDR at %d, len %d\n", idr_start_found, fhi.len);
            }
        } else {
            if (debug) fprintf(stderr, "No frame found\n");
            return -6;
        }

        // Add FF_INPUT_BUFFER_PADDING_SIZE to make the size compatible with ffmpeg conversion
        bufferh26x = (unsigned char *) malloc(fhv.len + fhs.len + fhp.len + fhi.len + FF_INPUT_BUFFER_PADDING_SIZE);
        if (bufferh26x == NULL) {
            fprintf(stderr, "Unable to allocate memory\n");
            return -7;
        }

        bufferyuv = (unsigned char *) malloc(width * height * 3 / 2);
        if (bufferyuv == NULL) {
            fprintf(stderr, "Unable to allocate memory\n");
            return -8;
        }

        if (fhv_addr != NULL) {
            memcpy(bufferh26x, fhv_addr, fhv.len);
        }
        memcpy(bufferh26x + fhv.len, fhs_addr, fhs.len);
        memcpy(bufferh26x + fhv.len + fhs.len, fhp_addr, fhp.len);
        memcpy(bufferh26x + fhv.len + fhs.len + fhp.len, fhi_addr, fhi.len);

        free(h26x_file_buffer);

        if (fhv_addr == NULL) {
            if (debug) fprintf(stderr, "Decoding h264 frame\n");
            if(frame_decode(bufferyuv, bufferh26x, fhs.len + fhp.len + fhi.len, 4) < 0) {
                fprintf(stderr, "Error decoding h264 frame\n");
                return -9;
            }
        } else {
            if (debug) fprintf(stderr, "Decoding h265 frame\n");
            if(frame_decode(bufferyuv, bufferh26x, fhv.len + fhs.len + fhp.len + fhi.len, 5) < 0) {
                fprintf(stderr, "Error decoding h265 frame\n");
                return -9;
            }
        }
        free(bufferh26x);

    }

    if (watermark) {
        if (debug) fprintf(stderr, "Adding watermark\n");
        if (add_watermark(bufferyuv, width, height, &watermark_tm) < 0) {
            fprintf(stderr, "Error adding watermark\n");
            return -10;
        }
    }

    if (debug) fprintf(stderr, "Encoding jpeg image\n");
    if(YUVtoJPG("stdout", bufferyuv, width, height, width, height) < 0) {
        fprintf(stderr, "Error encoding jpeg file\n");
        return -11;
    }

    free(bufferyuv);

    if (file[0] == '\0') {
        // Free memory
        if (debug) fprintf(stderr, "Free memory\n");
        munmap(addr, size);
    }

    return 0;
}
