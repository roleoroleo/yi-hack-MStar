/**********
This library is free software; you can redistribute it and/or modify it under
the terms of the GNU Lesser General Public License as published by the
Free Software Foundation; either version 3 of the License, or (at your
option) any later version. (See <http://www.gnu.org/copyleft/lesser.html>.)

This library is distributed in the hope that it will be useful, but WITHOUT
ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public License for
more details.

You should have received a copy of the GNU Lesser General Public License
along with this library; if not, write to the Free Software Foundation, Inc.,
51 Franklin Street, Fifth Floor, Boston, MA 02110-1301  USA
**********/
// "liveMedia"
// Copyright (c) 1996-2023 Live Networks, Inc.  All rights reserved.
// A source object for AAC audio fifo in ADTS format
// Implementation

#include "ADTSAudioFifoSource.hh"
#include "InputFile.hh"
#include <GroupsockHelper.hh>
#include "rRTSPServer.h"
#include <cstring>

#include <fcntl.h>

extern int debug;

////////// ADTSAudioFifoSource //////////

static unsigned const samplingFrequencyTable[16] = {
    96000, 88200, 64000, 48000,
    44100, 32000, 24000, 22050,
    16000, 12000, 11025, 8000,
    7350, 0, 0, 0
};

ADTSAudioFifoSource*
ADTSAudioFifoSource::createNew(UsageEnvironment& env, char const* fileName) {
    FILE* fid = NULL;
    int wd_counter = 0;
    unsigned char headers[7];

    fid = OpenInputFile(env, fileName);
    if (fid == NULL) {
        CloseInputFile(fid);
        return NULL;
    }

    makeSocketNonBlocking(fileno(fid));

    for (;;) {
        wd_counter++;
        if (wd_counter >= 1000) break;

        // Now, having opened the input file, read the header of the first frame,
        // to get the audio stream's parameters:
        if (read(fileno(fid), headers, 1) < 1) {
            usleep(1000);
            continue;
        }
        // Check the 'syncword', 1st byte:
        if (!(headers[0] == 0xFF)) {
            continue;
        }
        if (read(fileno(fid), &headers[1], 1) < 1) {
            usleep(1000);
            continue;
        }
        // Check the 'syncword', 2nd byte:
        if (!((headers[1]&0xF0) == 0xF0)) {
            continue;
        }

        if (read(fileno(fid), &headers[2], sizeof(headers) - 2) < (signed) sizeof(headers) - 2) {
            usleep(1000);
            continue;
        }

        // Get and check the 'profile':
        u_int8_t profile = (headers[2]&0xC0)>>6; // 2 bits
        if (profile == 3) {
            fprintf(stderr, "%lld: ADTSAudioFifoSource - Bad (reserved) 'profile': 3 in first frame of ADTS file", current_timestamp());
            break;
        }

        // Get and check the 'sampling_frequency_index':
        u_int8_t sampling_frequency_index = (headers[2]&0x3C)>>2; // 4 bits
        if (samplingFrequencyTable[sampling_frequency_index] == 0) {
            fprintf(stderr, "%lld: ADTSAudioFifoSource - Bad 'sampling_frequency_index' in first frame of ADTS file", current_timestamp());
            break;
        }

        // Get and check the 'channel_configuration':
        u_int8_t channel_configuration
            = ((headers[2]&0x01)<<2)|((headers[3]&0xC0)>>6); // 3 bits

        u_int16_t frame_length = ((headers[3]&0x03)<<11) | (headers[4]<<3) | ((headers[5]&0xE0)>>5);

        unsigned char *tmpPacket = (unsigned char *) malloc(frame_length * sizeof(unsigned char) - sizeof(headers));
        read(fileno(fid), tmpPacket, frame_length - sizeof(headers));
        free(tmpPacket);

        if (debug & 8)
            fprintf(stderr, "%lld: ADTSAudioFifoSource - Read first frame: frame_length %d, "
                    "profile %d, "
                    "sampling_frequency_index %d => samplingFrequency %d, "
                    "channel_configuration %d\n",
                    current_timestamp(), frame_length, profile,
                    sampling_frequency_index, samplingFrequencyTable[sampling_frequency_index],
                    channel_configuration);

        return new ADTSAudioFifoSource(env, fid, profile,
                                       sampling_frequency_index,
                                       channel_configuration);
    }

    // An error occurred:
    if (debug & 8) fprintf(stderr, "%lld: ADTSAudioFifoSource - Error opening fifo file\n", current_timestamp());
    CloseInputFile(fid);
    return NULL;
}

ADTSAudioFifoSource
::ADTSAudioFifoSource(UsageEnvironment& env, FILE* fid, u_int8_t profile,
                      u_int8_t samplingFrequencyIndex, u_int8_t channelConfiguration)
    : FramedFileSource(env, fid), fHaveStartedReading(False) {
    fSamplingFrequency = samplingFrequencyTable[samplingFrequencyIndex];
    fNumChannels = channelConfiguration == 0 ? 2 : channelConfiguration;
    fuSecsPerFrame
        = (1024/*samples-per-frame*/*1000000) / fSamplingFrequency/*samples-per-second*/;

    // Construct the 'AudioSpecificConfig', and from it, the corresponding ASCII string:
    unsigned char audioSpecificConfig[2];
    u_int8_t const audioObjectType = profile + 1;
    audioSpecificConfig[0] = (audioObjectType<<3) | (samplingFrequencyIndex>>1);
    audioSpecificConfig[1] = (samplingFrequencyIndex<<7) | (channelConfiguration<<3);
    sprintf(fConfigStr, "%02X%02X", audioSpecificConfig[0], audioSpecificConfig[1]);
}

ADTSAudioFifoSource::~ADTSAudioFifoSource() {
    CloseInputFile(fFid);
}

void ADTSAudioFifoSource::cleanFifo() {
    int flags;

    if (debug & 8) fprintf(stderr, "%lld: ADTSAudioFifoSource - cleaning fifo\n", current_timestamp());

    if (fFid == NULL) return;

    // Set non blocking
    if ((flags = fcntl(fileno(fFid), F_GETFL, 0)) < 0) {
        return;
    }
    if (fcntl(fileno(fFid), F_SETFL, flags | O_NONBLOCK) != 0) {
        return;
    }

    // Clean fifo content
    unsigned char null[4];
    while (read(fileno(fFid), null, sizeof(null)) > 0) {}

    // Restore old blocking
    if (fcntl(fileno(fFid), F_SETFL, flags) != 0) {
        return;
    }

    if (debug & 8) fprintf(stderr, "%lld: ADTSAudioFifoSource - fifo cleaned\n", current_timestamp());
}

void ADTSAudioFifoSource::doGetNextFrame() {
    fFrameSize = 0; // until it's set later
    if (!fHaveStartedReading) {
        cleanFifo();
        fHaveStartedReading = True;
    }
    doReadFromFile();
}

void ADTSAudioFifoSource::doStopGettingFrames() {
    if (debug & 8) fprintf(stderr, "%lld: ADTSAudioFifoSource - doStopGettingFrames\n", current_timestamp());
    envir().taskScheduler().unscheduleDelayedTask(nextTask());
    fHaveStartedReading = False;
}

void ADTSAudioFifoSource::doReadFromFile() {
    // Begin by reading the 7-byte fixed_variable headers:
    unsigned char headers[7];
    unsigned char tmp[2];
    Boolean headerIsOk = false;
    int wd_counter = 0;

    while (!headerIsOk) {
        wd_counter++;
        if (wd_counter >= 1000) break;

        if (read(fileno(fFid), headers, 1) < 1) {
            usleep(1000);
fprintf(stderr, "Son qui 1\n");
            continue;
        }
        // Check the 'syncword', 1st byte:
        if (!(headers[0] == 0xFF)) {
fprintf(stderr, "Son qui 2\n");
            continue;
        }
        if (read(fileno(fFid), &headers[1], 1) < 1) {
            usleep(1000);
fprintf(stderr, "Son qui 3\n");
            continue;
        }
        // Check the 'syncword', 2nd byte:
        if (!((headers[1]&0xF0) == 0xF0)) {
fprintf(stderr, "Son qui 4\n");
            continue;
        }

        if (read(fileno(fFid), &headers[2], sizeof(headers) - 2) < (signed) sizeof(headers) - 2) {
            usleep(1000);
fprintf(stderr, "Son qui 5\n");
            continue;
        }

fprintf(stderr, "Son qui 6\n");
        headerIsOk = true;
    }

    if (!headerIsOk) {
        if (debug & 8) fprintf(stderr, "%lld: ADTSAudioFifoSource - do handleClosure()\n", current_timestamp());
        handleClosure();
fprintf(stderr, "Son qui 7\n");
        return;
    }

    // Extract important fields from the headers:
    Boolean protection_absent = headers[1]&0x01;
    u_int16_t frame_length
        = ((headers[3]&0x03)<<11) | (headers[4]<<3) | ((headers[5]&0xE0)>>5);

    if (debug & 8) {
        u_int16_t syncword = (headers[0]<<4) | (headers[1]>>4);
        fprintf(stderr, "%lld: ADTSAudioFifoSource - Read frame: syncword 0x%x, protection_absent %d, frame_length %d\n", current_timestamp(), syncword, protection_absent, frame_length);
        if (syncword != 0xFFF) fprintf(stderr, "%lld: ADTSAudioFifoSource - WARNING: Bad syncword!\n", current_timestamp());
    }
    unsigned numBytesToRead
        = frame_length > sizeof headers ? frame_length - sizeof headers : 0;

    // If there's a 'crc_check' field, skip it:
    if (!protection_absent) {
        read(fileno(fFid), tmp, 2);
        numBytesToRead = numBytesToRead > 2 ? numBytesToRead - 2 : 0;
    }

    // Next, read the raw frame data into the buffer provided:
    if (numBytesToRead > fMaxSize) {
        fNumTruncatedBytes = numBytesToRead - fMaxSize;
        numBytesToRead = fMaxSize;
    }
    int numBytesRead = read(fileno(fFid), fTo, numBytesToRead);
    if (numBytesRead < 0) numBytesRead = 0;
    fFrameSize = numBytesRead;
    fNumTruncatedBytes += numBytesToRead - numBytesRead;

    // Set the 'presentation time':
    gettimeofday(&fPresentationTime, NULL);
    fDurationInMicroseconds = fuSecsPerFrame;

    fprintf(stderr, "%lld: ADTSAudioFifoSource - fMaxSize %d - fFrameSize %d - fNumBytesToRead %d - fNumBytesRead %d - fNumTruncatedBytes %d\n", current_timestamp(), fMaxSize, fFrameSize, numBytesToRead, numBytesRead, fNumTruncatedBytes);

    // Switch to another task, and inform the reader that he has data:
    nextTask() = envir().taskScheduler().scheduleDelayedTask(0,
                         (TaskFunc*)FramedSource::afterGetting, this);
}
