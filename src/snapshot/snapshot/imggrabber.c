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

#include <jpeglib.h>

#define RESOLUTION_LOW 360
#define RESOLUTION_HIGH 1080

#define W_LOW 640
#define H_LOW 360
#define W_HIGH 1920
#define H_HIGH 1080

#define W_MB 16
#define H_MB 16

#define UV_OFFSET_LOW 0x3c00
#define UV_OFFSET_HIGH 0x0023a000

#define PROC_FILE "/proc/umap/vb"
#define JPEG_QUALITY 90

int debug = 0;

/**
 * Converts a YUYV raw buffer to a JPEG buffer.
 * Input is YUYV (YUV 420SP NV12). Output is JPEG binary.
 */
int compressYUYVtoJPEG(uint8_t *input, const int width, const int height)
{
    struct jpeg_compress_struct cinfo;
    struct jpeg_error_mgr jerr;
    JSAMPROW row_ptr[1];
    int row_stride;

    uint8_t* outbuffer = NULL;
    unsigned long outlen = 0;

    unsigned int i, j;
    unsigned int offset;
    unsigned int uv;

    cinfo.err = jpeg_std_error(&jerr);
    jpeg_create_compress(&cinfo);
    jpeg_mem_dest(&cinfo, &outbuffer, &outlen);

    // jrow is a libjpeg row of samples array of 1 row pointer
    cinfo.image_width = width & -1;
    cinfo.image_height = height & -1;
    cinfo.input_components = 3;
    cinfo.in_color_space = JCS_YCbCr; //libJPEG expects YUV 3bytes, 24bit

    jpeg_set_defaults(&cinfo);
    jpeg_set_quality(&cinfo, JPEG_QUALITY, TRUE);
    jpeg_start_compress(&cinfo, TRUE);

    uint8_t tmprowbuf[width * 3];

    JSAMPROW row_pointer[1];
    row_pointer[0] = &tmprowbuf[0];
    while (cinfo.next_scanline < cinfo.image_height) {
        offset = cinfo.next_scanline * cinfo.image_width; //offset to the correct row
        uv = cinfo.image_width * (cinfo.image_height - (cinfo.next_scanline + 1) / 2);

        for (i = 0, j = 0; i < cinfo.image_width; i += 2, j += 6) { //input strides by 2 bytes, output strides by 6 (2 pixels)
            tmprowbuf[j + 0] = input[offset + i];          // Y (unique to this pixel)
            tmprowbuf[j + 1] = input[offset + uv + i];     // U (shared between pixels)
            tmprowbuf[j + 2] = input[offset + uv + i + 1]; // V (shared between pixels)
            tmprowbuf[j + 3] = input[offset + i + 1];      // Y (unique to this pixel)
            tmprowbuf[j + 4] = input[offset + uv + i];     // U (shared between pixels)
            tmprowbuf[j + 5] = input[offset + uv + i + 1]; // V (shared between pixels)
        }
        jpeg_write_scanlines(&cinfo, row_pointer, 1);
    }

    jpeg_finish_compress(&cinfo);
    jpeg_destroy_compress(&cinfo);

    // Reuse input buffer
    memcpy (input, outbuffer, outlen);

    return outlen;
}

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

int main(int argc, char **argv)
{
    const char memDevice[] = "/dev/mem";
    int resolution;
    FILE *fPtr, *fLen;
    int fMem;
    unsigned int ivAddr, ipAddr;
    unsigned int size;
    unsigned char *addr;
    unsigned char *buffer;
    int outlen;

    if ((argc > 3) || (argc == 2)) {
        fprintf(stderr, "Wrong parameters\n");
        return -1;
    } else if (argc == 3) {
        if (strcasecmp("-r", argv[1]) == 0) {
            if (strcasecmp("low", argv[2]) == 0) {
                resolution = RESOLUTION_LOW;
            } else {
                resolution = RESOLUTION_HIGH;
            }
        } else {
            fprintf(stderr, "Wrong parameters\n");
            return -1;
        }
    } else {
        resolution = RESOLUTION_HIGH;
    }

    if (debug) fprintf(stderr, "Resolution: %d\n", resolution);

    if (resolution == RESOLUTION_LOW) {
        fPtr = fopen("/proc/mstar/OMX/VMFE1/ENCODER_INFO/IBUF_pBuffer", "r");
        fLen = fopen("/proc/mstar/OMX/VMFE1/ENCODER_INFO/IBUF_nAllocLen", "r");
    } else {
        fPtr = fopen("/proc/mstar/OMX/VMFE0/ENCODER_INFO/IBUF_pBuffer", "r");
        fLen = fopen("/proc/mstar/OMX/VMFE0/ENCODER_INFO/IBUF_nAllocLen", "r");
    }
    fscanf(fPtr, "%x", &ivAddr);
    fclose(fPtr);
    fscanf(fLen, "%d", &size);
    fclose(fLen);

    ipAddr = rmm_virt2phys(ivAddr);

    if (debug) fprintf(stderr, "vaddr: 0x%08x - paddr: 0x%08x - size: %u\n", ivAddr, ipAddr, size);

    // allocate buffer memory
    buffer = (unsigned char *) malloc(size);

    // open /dev/mem and error checking
    fMem = open(memDevice, O_RDONLY); // | O_SYNC);
    if (fMem < 0) {
        fprintf(stderr, "Failed to open the /dev/mem\n");
        return -1;
    }

    // mmap() the opened /dev/mem
    addr = (unsigned char *) (mmap(NULL, size, PROT_READ, MAP_SHARED, fMem, ipAddr));
    if (addr == MAP_FAILED) {
        fprintf(stderr, "Failed to map memory\n");
        return -1;
    }

    // close the character device
    close(fMem);

    if (debug) fprintf(stderr, "copy buffer: len %d\n", size);
    memcpy(buffer, addr, size);

    if (resolution == RESOLUTION_LOW) {
        // The buffer contains YUV N12 image but the UV part is not in the
        // right position. We need to move it.
        memmove(buffer + W_LOW * H_LOW, buffer + W_LOW * H_LOW + UV_OFFSET_LOW,
            W_LOW * H_LOW / 2);
        // create jpeg
        outlen = compressYUYVtoJPEG(buffer, W_LOW, H_LOW);
    } else {
        // The buffer contains YUV N21 image saved in blocks.
        // We need to convert to standard YUV N12 image.
        outlen = img2YUV(buffer, size, W_HIGH, H_HIGH);
        // create jpeg
        outlen = compressYUYVtoJPEG(buffer, W_HIGH, H_HIGH);
    }

    fwrite(buffer, 1, outlen, stdout);

    // Free memory
    munmap(addr, size);
    free(buffer);
}
