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
 * Sends command via pipes and receive an i-frame.
 * Then converts it to JPG using libavcodec and print to stdout.
 */

#include <stdlib.h>
#include <stdio.h>
#include <sys/mman.h>
#include <stdint.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#ifdef HAVE_AV_CONFIG_H
#undef HAVE_AV_CONFIG_H
#endif

#include "libavcodec/avcodec.h"

#define RESOLUTION_LOW 360
#define RESOLUTION_HIGH 1080

#define TMP_FILE "/tmp/snapshot.yuv"
#define FIFO_FILE "/tmp/idr_fifo"

#define FF_INPUT_BUFFER_PADDING_SIZE 32

int debug = 0;
unsigned char bufimg[262144];

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

    if (debug) fprintf(stderr, "Opening output file %s\n", outfilename);
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
    if (debug) fprintf(stderr, "Cleaning memory\n");
    free(inbuf);
    av_frame_free(&picture);
    avcodec_close(c);
    av_free(c);

    return 0;
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

int buffer2jpg(unsigned char *buffer, int length, int res)
{

    FILE *fgh;

    fgh = fopen("/tmp/snapshot.264", "w");
    fwrite(buffer, 1, length, fgh);
    fclose(fgh);

    // Use separate decode/encode function with a temp file to save memory
    if(frame_decode(TMP_FILE, buffer, length) < 0) {
        if (debug) fprintf(stderr, "Error decoding h264 frame\n");
        exit(-11);
    }

    if(frame_encode("stdout", TMP_FILE, res) < 0) {
        if (debug) fprintf(stderr, "Error encoding jpg image\n");
        exit(-12);
    }
//    remove(TMP_FILE);

    return 0;
}

int main(int argc, char **argv)
{
    int p[2], fd, nread, len, i;
    char bufchar[8];
    char bufread[1024];
    unsigned char *ptr;
    char *idr_fifo = FIFO_FILE; 

    if (argc < 2) {
        bufchar[0] = 'h';
    } else {
        bufchar[0] = argv[1][0];
    }

    if (debug) fprintf(stderr, "Creating fifo\n");
    mkfifo(idr_fifo, 0666);

    if (debug) fprintf(stderr, "Opening fifo\n");
    fd = open(idr_fifo, O_RDWR|O_NONBLOCK);
    if (fd == -1) {
        fprintf(stderr, "Error opening fifo\n");
    }
    write(fd, bufchar, 1);
    usleep(200000);

    i = 0;
    while ((nread = read(fd, bufread, sizeof(bufread))) == -1) {
        usleep(100000);
        if (i >= 100) {
            fprintf(stderr, "Timeout expired\n");
            exit(-1);
        }
        i++;
    }
    ptr = bufimg;
    if (nread > sizeof(bufimg)) {
        fprintf(stderr, "Buffer overflow\n");
        exit(-2);
    }
    memcpy(ptr, bufread, nread);
    ptr += nread;

    while ((nread = read(fd, bufread, sizeof(bufread))) != -1) {
        if ((ptr - bufimg) + nread > sizeof(bufimg)) {
            fprintf(stderr, "Buffer overflow\n");
            exit(-3);
        }
        memcpy(ptr, bufread, nread);
        ptr += nread;
    }
    len = ptr - bufimg;

    if (debug) fprintf(stderr, "Read %d bytes\n", len);
    if ((bufchar[0] == 'l') && (len > 0)) {
        buffer2jpg(bufimg, len, RESOLUTION_LOW);
    } else if ((bufchar[0] == 'h') && (len > 0)) {
        buffer2jpg(bufimg, len, RESOLUTION_HIGH);
    }

    if (debug) fprintf(stderr, "Done\n");
    close(fd);
}
