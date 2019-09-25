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
 * Read the last h264 i-frame from the buffer and convert it using libavcodec.
 * The position of the frame is written in /tmp/iframe.idx with the following
 * rule:
 * - high res offset (4 bytes)
 * - high res length (4 bytes)
 * - low res offset (4 bytes)
 * - low res length (4 bytes)
 */

#include <stdlib.h>
#include <stdio.h>
#include <sys/mman.h>
#include <getopt.h>

#ifdef HAVE_AV_CONFIG_H
#undef HAVE_AV_CONFIG_H
#endif

#include "libavcodec/avcodec.h"

#define BUFFER_FILE "/dev/fshare_frame_buf"
#define I_FILE "/tmp/iframe.idx"
#define TMP_FILE "/tmp/snapshot.yuv"
#define FF_INPUT_BUFFER_PADDING_SIZE 32

#define RESOLUTION_HIGH 0
#define RESOLUTION_LOW 1

int debug = 0;

int frame_decode(char *outfilename, unsigned char *p, int length)
{
    AVCodec *codec;
    AVCodecContext *c= NULL;
    AVFrame *picture;
    int got_picture, len;
    FILE *fOut;
    uint8_t *inbuf;
    AVPacket avpkt;
    int i, size;

//////////////////////////////////////////////////////////
//                    Reading H264                      //
//////////////////////////////////////////////////////////

    av_init_packet(&avpkt);

    codec = avcodec_find_decoder(AV_CODEC_ID_H264);
    if (!codec) {
        if (debug) fprintf(stderr, "Codec h264 not found\n");
        return -2;
    }

    c = avcodec_alloc_context3(codec);
    picture = av_frame_alloc();

    if((codec->capabilities) & AV_CODEC_CAP_TRUNCATED)
        (c->flags) |= AV_CODEC_FLAG_TRUNCATED;

    if (avcodec_open2(c, codec, NULL) < 0) {
        if (debug) fprintf(stderr, "Could not open codec h264\n");
        return -2;
    }

    inbuf = (uint8_t *) malloc(length + FF_INPUT_BUFFER_PADDING_SIZE);
    if (inbuf == NULL) {
        if (debug) fprintf(stderr, "Error allocating memory\n");
        return -2;
    }
    memset(inbuf + length, 0, FF_INPUT_BUFFER_PADDING_SIZE);

    // Get only 1 frame
    memcpy(inbuf, p, length);
    avpkt.size = length;
    avpkt.data = inbuf;

    // Decode frame
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
        return -2;
    }
    fOut = fopen(outfilename,"w");
    if(!fOut) {
        if (debug) fprintf(stderr, "could not open %s\n", outfilename);
        return -2;
    }

    for(i=0; i<c->height; i++)
        fwrite(picture->data[0] + i * picture->linesize[0], 1, c->width, fOut);
    for(i=0; i<c->height/2; i++)
        fwrite(picture->data[1] + i * picture->linesize[1], 1, c->width/2, fOut);
    for(i=0; i<c->height/2; i++)
        fwrite(picture->data[2] + i * picture->linesize[2], 1, c->width/2, fOut);

    fclose(fOut);

    // Clean memory
    free(inbuf);
    av_frame_free(&picture);
    avcodec_close(c);
    av_free(c);
}

int frame_encode(char *outfilename, char *infilename, int resolution)
{
    AVCodec *codec;
    AVCodecContext *c= NULL;
    AVFrame *picture;
    int got_output;
    FILE *fIn, *fOut;
    AVPacket avpkt;
    int i, size, ret;

//////////////////////////////////////////////////////////
//                    Writing JPEG                      //
//////////////////////////////////////////////////////////

    codec = avcodec_find_encoder(AV_CODEC_ID_MJPEG);
    if (!codec) {
        if (debug) fprintf(stderr, "Codec mjpeg not found\n");
        return -3;
    }

    c = avcodec_alloc_context3(codec);
    if (!c) {
        if (debug) fprintf(stderr, "Could not allocate video codec context\n");
        return -3;
    }

    c->bit_rate = 200000;
    if (resolution == RESOLUTION_LOW) {
        c->width = 640;
        c->height = 360;
    } else {
        c->width = 1920;
        c->height = 1088;
    }
    c->time_base= (AVRational){1,25};
    c->codec_type = AVMEDIA_TYPE_VIDEO;
    c->pix_fmt = AV_PIX_FMT_YUVJ420P;

    ret = avcodec_open2(c, codec, NULL);
    if (ret < 0) {
        if (debug) fprintf(stderr, "Could not open codec mjpeg: %d\n", ret);
        return -3;
    }

    picture = av_frame_alloc();
    if (!picture) {
        if (debug) fprintf(stderr, "Could not allocate video frame\n");
        return -3;
    }
    picture->format = c->pix_fmt;
    picture->width  = c->width;
    picture->height = c->height;

    ret = av_image_alloc(picture->data, picture->linesize, c->width, c->height, c->pix_fmt, 32);
    if (ret < 0) {
        if (debug) fprintf(stderr, "Could not allocate raw picture buffer\n");
        return -3;
    }

    av_init_packet(&avpkt);
    avpkt.data = NULL;
    avpkt.size = 0;

    // Open input file
    fIn = fopen(infilename, "r");
    if(!fIn){
        if (debug) fprintf(stderr, "could not open %s\n", infilename);
        return -3;
    }

    // Y - Cb - Cr
    for(i=0; i<c->height; i++)
        fread(picture->data[0] + i * picture->linesize[0], 1, c->width, fIn);
    for(i=0; i<c->height/2; i++)
        fread(picture->data[1] + i * picture->linesize[1], 1, c->width/2, fIn);
    for(i=0; i<c->height/2; i++)
        fread(picture->data[2] + i * picture->linesize[2], 1, c->width/2, fIn);

    fclose(fIn);

    // Open output file
    if (strcmp("stdout", outfilename) == 0) {
        fOut = stdout;
    } else {
        fOut = fopen(outfilename, "wb");
        if(!fOut){
            if (debug) fprintf(stderr, "could not open %s\n", outfilename);
            return -3;
        }
    }
    got_output = 0;

    // Encode frame
    ret = avcodec_send_frame(c, picture);
    while (ret >= 0) {
        ret = avcodec_receive_packet(c, &avpkt);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
            break;
        } else if (ret < 0) {
            if (debug) fprintf(stderr, "Error encoding frame\n");
            break;
        }

        fwrite(avpkt.data, 1, avpkt.size, fOut);
        av_packet_unref(&avpkt);
    }

    // Tell size
    if (strcmp("stdout", outfilename) != 0) {
        fseek(fOut, 0L, SEEK_END);
        size = ftell(fOut);
        fclose(fOut);

        // Delete file if zero length
        if (size == 0) {
            remove(outfilename);
        }
    }

    // Clean memory
    av_frame_free(&picture);
    avcodec_close(c);
    av_free(c);

    return 0;
}

void usage(char *prog_name)
{
    fprintf(stderr, "Usage: %s [options]\n", prog_name);
    fprintf(stderr, "\t-o, --output=FILE       Set output file name (stdout to print on stdout)\n");
    fprintf(stderr, "\t-r, --res=RES           Set resolution: \"low\" or \"high\" (default \"high\")\n");
    fprintf(stderr, "\t-h, --help              Show this help\n");
}

int main(int argc, char **argv)
{
    FILE *fIdx, *fBuf;
    uint8_t utmp[16];
    uint32_t offset, length;
    unsigned char *addr;
    int size;
    int res = RESOLUTION_HIGH;
    char output_file[1024] = "";

    int c;

    while (1) {
        static struct option long_options[] = {
            {"output",  required_argument, 0, 'o'},
            {"res",     required_argument, 0, 'r'},
            {"help",    no_argument,       0, 'h'},
            {0,         0,                 0,  0 }
        };

        int option_index = 0;
        c = getopt_long(argc, argv, "o:r:h",
            long_options, &option_index);
        if (c == -1)
            break;

        switch (c) {
            case 'o':
                strcpy(output_file, optarg);
                break;

            case 'r':
                if (strcasecmp("low", optarg) == 0)
                    res = RESOLUTION_LOW;
                else
                    res = RESOLUTION_HIGH;
                break;

            case 'h':
            default:
                usage(argv[0]);
                exit(-1);
                break;
        }
    }

    if (output_file[0] == '\0') {
        usage(argv[0]);
        exit(-1);
    }

    fIdx = fopen(I_FILE, "r");
    if (fIdx == NULL) {
        if (debug) fprintf(stderr, "Error opening file %s\n", I_FILE);
        exit(-1);
    }
    if (fread(utmp, 1, sizeof(utmp), fIdx) != sizeof(utmp)) {
        if (debug) fprintf(stderr, "Error reading file %s\n", I_FILE);
        exit(-1);
    }

    if (res == RESOLUTION_HIGH) {
        memcpy(&offset, utmp, sizeof(uint32_t));
        memcpy(&length, utmp + 0x04, sizeof(uint32_t));
    } else {
        memcpy(&offset, utmp + 0x08, sizeof(uint32_t));
        memcpy(&length, utmp + 0x0c, sizeof(uint32_t));
    }

    fBuf = fopen(BUFFER_FILE, "r") ;
    if ( fBuf == NULL ) {
        if (debug) fprintf(stderr, "could not open file %s\n", BUFFER_FILE);
        exit(-1);
    }

    // Tell size
    fseek(fBuf, 0L, SEEK_END);
    size = ftell(fBuf);
    fseek(fBuf, 0L, SEEK_SET);

    // Map file to memory
    addr = (unsigned char*) mmap(NULL, size, PROT_READ, MAP_SHARED, fileno(fBuf), 0);
    if (addr == MAP_FAILED) {
        if (debug) fprintf(stderr, "error mapping file %s\n", BUFFER_FILE);
        exit(-1);
    }
    if (debug) fprintf(stderr, "mapping file %s, size %d, to %08x\n", BUFFER_FILE, size, addr);

    // Closing the file
    if (debug) fprintf(stderr, "closing the file %s\n", BUFFER_FILE) ;
    fclose(fBuf) ;

    // Use separate decode/encode function with a temp file to save memory
    if(frame_decode(TMP_FILE, addr + offset, length) < 0)
        exit(-2);
    if(frame_encode(output_file, TMP_FILE, res) < 0)
        exit(-3);
    remove(TMP_FILE);

    // Unmap file from memory
    if (munmap(addr, size) == -1) {
        if (debug) fprintf(stderr, "error munmapping file");
    } else {
        if (debug) fprintf(stderr, "unmapping file %s, size %d, from %08x\n", BUFFER_FILE, size, addr);
    }

    return 0;
}
