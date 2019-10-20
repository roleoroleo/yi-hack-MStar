/*
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

/*
 * Scans the buffer and temporarily copies the i-frame in memory.
 * When it receives a command via pipes, sends the buffer to the sender.
 */

#include <stdlib.h>
#include <stdio.h>
#include <sys/mman.h>
#include <stdint.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <dirent.h>
#include <pthread.h>

#define RESOLUTION_LOW 360
#define RESOLUTION_HIGH 1080

#define FIFO_FILE "/tmp/idr_fifo"

int debug = 0;

unsigned char SPS[] = { 0x00, 0x00, 0x00, 0x01, 0x67 };
char filLenFileL[1024];
char filLenFileH[1024];
unsigned char *addrL, *addrH;
unsigned char bufferL[131072], bufferH[262144];
int lenL, lenH;

static unsigned char const crc8x_table[] = {
    0x00, 0x31, 0x62, 0x53, 0xc4, 0xf5, 0xa6, 0x97, 0xb9, 0x88, 0xdb, 0xea, 0x7d,
    0x4c, 0x1f, 0x2e, 0x43, 0x72, 0x21, 0x10, 0x87, 0xb6, 0xe5, 0xd4, 0xfa, 0xcb,
    0x98, 0xa9, 0x3e, 0x0f, 0x5c, 0x6d, 0x86, 0xb7, 0xe4, 0xd5, 0x42, 0x73, 0x20,
    0x11, 0x3f, 0x0e, 0x5d, 0x6c, 0xfb, 0xca, 0x99, 0xa8, 0xc5, 0xf4, 0xa7, 0x96,
    0x01, 0x30, 0x63, 0x52, 0x7c, 0x4d, 0x1e, 0x2f, 0xb8, 0x89, 0xda, 0xeb, 0x3d,
    0x0c, 0x5f, 0x6e, 0xf9, 0xc8, 0x9b, 0xaa, 0x84, 0xb5, 0xe6, 0xd7, 0x40, 0x71,
    0x22, 0x13, 0x7e, 0x4f, 0x1c, 0x2d, 0xba, 0x8b, 0xd8, 0xe9, 0xc7, 0xf6, 0xa5,
    0x94, 0x03, 0x32, 0x61, 0x50, 0xbb, 0x8a, 0xd9, 0xe8, 0x7f, 0x4e, 0x1d, 0x2c,
    0x02, 0x33, 0x60, 0x51, 0xc6, 0xf7, 0xa4, 0x95, 0xf8, 0xc9, 0x9a, 0xab, 0x3c,
    0x0d, 0x5e, 0x6f, 0x41, 0x70, 0x23, 0x12, 0x85, 0xb4, 0xe7, 0xd6, 0x7a, 0x4b,
    0x18, 0x29, 0xbe, 0x8f, 0xdc, 0xed, 0xc3, 0xf2, 0xa1, 0x90, 0x07, 0x36, 0x65,
    0x54, 0x39, 0x08, 0x5b, 0x6a, 0xfd, 0xcc, 0x9f, 0xae, 0x80, 0xb1, 0xe2, 0xd3,
    0x44, 0x75, 0x26, 0x17, 0xfc, 0xcd, 0x9e, 0xaf, 0x38, 0x09, 0x5a, 0x6b, 0x45,
    0x74, 0x27, 0x16, 0x81, 0xb0, 0xe3, 0xd2, 0xbf, 0x8e, 0xdd, 0xec, 0x7b, 0x4a,
    0x19, 0x28, 0x06, 0x37, 0x64, 0x55, 0xc2, 0xf3, 0xa0, 0x91, 0x47, 0x76, 0x25,
    0x14, 0x83, 0xb2, 0xe1, 0xd0, 0xfe, 0xcf, 0x9c, 0xad, 0x3a, 0x0b, 0x58, 0x69,
    0x04, 0x35, 0x66, 0x57, 0xc0, 0xf1, 0xa2, 0x93, 0xbd, 0x8c, 0xdf, 0xee, 0x79,
    0x48, 0x1b, 0x2a, 0xc1, 0xf0, 0xa3, 0x92, 0x05, 0x34, 0x67, 0x56, 0x78, 0x49,
    0x1a, 0x2b, 0xbc, 0x8d, 0xde, 0xef, 0x82, 0xb3, 0xe0, 0xd1, 0x46, 0x77, 0x24,
    0x15, 0x3b, 0x0a, 0x59, 0x68, 0xff, 0xce, 0x9d, 0xac};

unsigned int crc8x_fast(unsigned int crc, void const *mem, size_t len) {
    unsigned char const *data = mem;
    if (data == NULL)
        return 0xff;
    crc &= 0xff;
    while (len--)
        crc = crc8x_table[crc ^ *data++];
    return crc;
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
unsigned int rmm_virt2phys(unsigned int inAddr)
{
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

void *checkBufferL(void *arg)
{
    int wait, crc, oldLen;
    FILE *fLen;

    while (1) {
        wait = 10000;
        // Checks if the frame is i-frame (low res buffer)
        if (memcmp(SPS, addrL, sizeof(SPS)) == 0) {
            // Reads the len until buffer is full and copies to memory
            fLen = fopen(filLenFileL, "r");
            fscanf(fLen, "%d", &lenL);
            fclose(fLen);
            oldLen = lenL;
            while (lenL == oldLen) {
                fLen = fopen(filLenFileL, "r");
                fscanf(fLen, "%d", &lenL);
                fclose(fLen);
            }
            if (debug) fprintf(stderr, "Found new len %d for low res\n", lenL);
            if (lenL <= sizeof(bufferL)) {
                memcpy(bufferL, addrL, lenL);
                // Checks if the buffer is "stable" using crc
                crc = crc8x_fast(0, bufferL, (size_t) lenL);
                usleep(100);
                if (crc == crc8x_fast(0, addrL, (size_t) lenL)) {
                    wait = 100000;
                    if (debug) fprintf(stderr, "Found new idr with len %d\n", lenL);
                } else {
                    lenL = 0;
                    wait = 100;
                    if (debug) fprintf(stderr, "Wrong buffer for low res\n");
                }
            } else {
                lenL = 0;
                if (debug) fprintf(stderr, "Buffer too short for low res\n");
            }
        }

        usleep(wait);
    }
}

void *checkBufferH(void *arg)
{
    int wait, crc, oldLen;
    FILE *fLen;

    while (1) {
        wait = 10000;
        // Checks if the frame is i-frame (high res buffer)
        if (memcmp(SPS, addrH, sizeof(SPS)) == 0) {
            // Reads the len until buffer is full and copies to memory
            fLen = fopen(filLenFileH, "r");
            fscanf(fLen, "%d", &lenH);
            fclose(fLen);
            oldLen = lenH;
            while (lenH == oldLen) {
                fLen = fopen(filLenFileH, "r");
                fscanf(fLen, "%d", &lenH);
                fclose(fLen);
            }
            if (debug) fprintf(stderr, "Found new len %d for high res\n", lenH);
            if (lenH <= sizeof(bufferH)) {
                memcpy(bufferH, addrH, lenH);
                // Checks if the buffer is "stable" using crc
                crc = crc8x_fast(0, bufferH, (size_t) lenH);
                usleep(100);
                if (crc == crc8x_fast(0, addrH, (size_t) lenH)) {
                    wait = 100000;
                    if (debug) fprintf(stderr, "Found new idr with len %d\n", lenH);
                } else {
                    lenH = 0;
                    wait = 100;
                    if (debug) fprintf(stderr, "Wrong buffer for high res\n");
                }
            } else {
                lenH = 0;
                if (debug) fprintf(stderr, "Buffer too short for high res\n");
            }
        }

        usleep(wait);
    }
}

// Main
int main(int argc, char **argv)
{
    const char memDevice[] = "/dev/mem";
    FILE *fPtr, *fLen, *fTime, *fOut;
    int fMem;
    unsigned int ivAddr, ipAddrL, ipAddrH;
    unsigned int sizeL, sizeH;
    char timeStampFileL[1024];
    char timeStampFileH[1024];
    unsigned int time, oldTimeL = 0, oldTimeH = 0;

    unsigned int crc;

    char bufchar[8];
    char *idr_fifo = FIFO_FILE;
    int fd, flags, err;
    int nwrite;

    fPtr = fopen("/proc/mstar/OMX/VMFE1/ENCODER_INFO/OBUF_pBuffer", "r");
    fLen = fopen("/proc/mstar/OMX/VMFE1/ENCODER_INFO/OBUF_nAllocLen", "r");
    fscanf(fPtr, "%x", &ivAddr);
    fclose(fPtr);
    fscanf(fLen, "%d", &sizeL);
    fclose(fLen);

    ipAddrL = rmm_virt2phys(ivAddr);
    if (debug) fprintf(stderr, "vaddr: 0x%08x - paddr: 0x%08x - size: %u\n", ivAddr, ipAddrL, sizeL);

    fPtr = fopen("/proc/mstar/OMX/VMFE0/ENCODER_INFO/OBUF_pBuffer", "r");
    fLen = fopen("/proc/mstar/OMX/VMFE0/ENCODER_INFO/OBUF_nAllocLen", "r");
    fscanf(fPtr, "%x", &ivAddr);
    fclose(fPtr);
    fscanf(fLen, "%d", &sizeH);
    fclose(fLen);

    ipAddrH = rmm_virt2phys(ivAddr);
    if (debug) fprintf(stderr, "vaddr: 0x%08x - paddr: 0x%08x - size: %u\n", ivAddr, ipAddrH, sizeH);

    // open /dev/mem and error checking
    fMem = open(memDevice, O_RDONLY); // | O_SYNC);
    if (fMem < 0) {
        fprintf(stderr, "Failed to open the /dev/mem\n");
        return -1;
    }

    // mmap() the opened /dev/mem
    addrL = (unsigned char *) (mmap(NULL, sizeL, PROT_READ, MAP_SHARED, fMem, ipAddrL));
    if (addrL == MAP_FAILED) {
        fprintf(stderr, "Failed to map memory\n");
        return -1;
    }
    addrH = (unsigned char *) (mmap(NULL, sizeH, PROT_READ, MAP_SHARED, fMem, ipAddrH));
    if (addrH == MAP_FAILED) {
        fprintf(stderr, "Failed to map memory\n");
        return -1;
    }

    // close the character device
    close(fMem);

    sprintf(filLenFileL, "/proc/mstar/OMX/VMFE1/ENCODER_INFO/OBUF_nFilledLen");
    sprintf(timeStampFileL, "/proc/mstar/OMX/VMFE1/ENCODER_INFO/OBUF_nTimeStamp");

    sprintf(filLenFileH, "/proc/mstar/OMX/VMFE0/ENCODER_INFO/OBUF_nFilledLen");
    sprintf(timeStampFileH, "/proc/mstar/OMX/VMFE0/ENCODER_INFO/OBUF_nTimeStamp");


    mkfifo(idr_fifo, 0666);
    fd = open(idr_fifo, O_RDWR|O_NONBLOCK);
    pthread_t tid[2];

    err = pthread_create(&(tid[0]), NULL, &checkBufferL, NULL);
    if (err != 0) {
        fprintf(stderr, "can't create thread :[%s]\n", strerror(err));
    }
    err = pthread_create(&(tid[1]), NULL, &checkBufferH, NULL);
    if (err != 0) {
        fprintf(stderr, "can't create thread :[%s]\n", strerror(err));
    }

    flags = fcntl(fd, F_GETFL, 0);

    // Main loop
    while(1) {
        // Checks if a command was received
        nwrite = 0;
        // Set no blocking to read commands
        fcntl(fd, F_SETFL, flags |= O_NONBLOCK);
        if (read(fd, bufchar, 1) == 1) {
            // Set blocking to write frame
            fcntl(fd, F_SETFL, flags &= ~O_NONBLOCK);
            if ((bufchar[0] == 'l') && (lenL > 0)) {
                if (debug) fprintf(stderr, "Sending low res frame to fifo\n");
                while(nwrite < lenL) {
                    if (lenL - nwrite > 4096) {
                        nwrite += write(fd, bufferL + nwrite, 4096);
                    } else {
                        nwrite += write(fd, bufferL + nwrite, lenL - nwrite);
                    }
                }
                if (debug) fprintf(stderr, "Sent %d bytes\n", nwrite);
            } else if ((bufchar[0] == 'h') && (lenH > 0)) {
                if (debug) fprintf(stderr, "Sending high res frame to fifo\n");
                while(nwrite < lenH) {
                    if (lenH - nwrite > 4096) {
                        nwrite += write(fd, bufferH + nwrite, 4096);
                    } else {
                        nwrite += write(fd, bufferH + nwrite, lenH - nwrite);
                    }
                }
                if (debug) fprintf(stderr, "Sent %d bytes\n", nwrite);
            }
        }
//        if (debug) fprintf(stderr, "Wait for the next frame\n");
        usleep(100000);
    }

    close(fd);
    munmap(addrH, sizeH);
    munmap(addrL, sizeL);
}
