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
// Copyright (c) 1996-2020 Live Networks, Inc.  All rights reserved.
// A source object for WAV fifo file converted in AAC/ADTS format
// Implementation

#include "ADTSFromWAVAudioFifoSource.hh"
#include "InputFile.hh"
#include "GroupsockHelper.hh"
#include "misc.hh"
#include "fdk-aac/aacenc_lib.h"

#include <fcntl.h>

extern int debug;

////////// ADTSFromWAVAudioFifoSource //////////

static unsigned const samplingFrequencyTable[16] = {
    96000, 88200, 64000, 48000,
    44100, 32000, 24000, 22050,
    16000, 12000, 11025, 8000,
    7350, 0, 0, 0
};

ADTSFromWAVAudioFifoSource*
ADTSFromWAVAudioFifoSource::createNew(UsageEnvironment& env, char const* fileName) {
    FILE* fid = NULL;

    do {
        if (debug & 2) fprintf(stderr, "%lld - ADTSFromWAVAudioFifoSource - Creating new ADTSFromWAVAudioFifoSource\n", current_timestamp());
        fid = OpenInputFile(env, fileName);
        if (fid == NULL) break;

        // Set a static profile
        u_int8_t profile = 1;
        // Set a static sampling_frequency_index
        u_int8_t sampling_frequency_index = 11;
        // Set a static channel_configuration
        u_int8_t channel_configuration = 1;

        if (debug & 2) {
            fprintf(stderr, "%lld - ADTSFromWAVAudioFifoSource - Setting profile: profile %d, "
                "sampling_frequency_index %d => samplingFrequency %d, "
                "channel_configuration %d\n",
                current_timestamp(), profile,
                sampling_frequency_index, samplingFrequencyTable[sampling_frequency_index],
                channel_configuration);
        }

        return new ADTSFromWAVAudioFifoSource(env, fid, profile,
                                   sampling_frequency_index, channel_configuration);
    } while (0);

    // An error occurred:
    CloseInputFile(fid);

    return NULL;
}

ADTSFromWAVAudioFifoSource
::ADTSFromWAVAudioFifoSource(UsageEnvironment& env, FILE* fid, u_int8_t profile,
                      u_int8_t samplingFrequencyIndex, u_int8_t channelConfiguration)
    : FramedSource(env), fFid(fid), fHaveStartedReading(False), fInputBufferSize(0) {

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
    if (debug & 2) fprintf(stderr, "%lld - ADTSFromWAVAudioFifoSource - Read config: audioObjectType %d, samplingFrequencyIndex %d, channelConfiguration %d\n", current_timestamp(), audioObjectType, samplingFrequencyIndex, channelConfiguration);

    // Open encoder
    fHAacEncoder = NULL;
    int ErrorStatus = aacEncOpen(&fHAacEncoder, 0, 0);
    if (ErrorStatus != AACENC_OK) {
        fprintf(stderr, "%lld - ADTSFromWAVAudioFifoSource - Error opening encoder\n", current_timestamp());
        return;
    }

    // Setting parameters
    aacEncoder_SetParam(fHAacEncoder, AACENC_AOT, AOT_AAC_LC); //AOT_AAC_LC or AOT_ER_AAC_LD
    aacEncoder_SetParam(fHAacEncoder, AACENC_BITRATE, 12000);
    aacEncoder_SetParam(fHAacEncoder, AACENC_BITRATEMODE, 0);
    aacEncoder_SetParam(fHAacEncoder, AACENC_SAMPLERATE, 8000);
    aacEncoder_SetParam(fHAacEncoder, AACENC_CHANNELMODE, MODE_1);
    aacEncoder_SetParam(fHAacEncoder, AACENC_TRANSMUX, TT_MP4_ADTS);

    // Init encoder
    ErrorStatus = aacEncEncode(fHAacEncoder, NULL, NULL, NULL, NULL);
    if (ErrorStatus != AACENC_OK) {
        fprintf(stderr, "%lld - ADTSFromWAVAudioFifoSource - Error initializing encoder\n", current_timestamp());
        return;
    }

    aacEncInfo(fHAacEncoder, &fEncInfo);

    fInBuffer[0] = fInputBuffer;
    fInBuffer[1] = fAncillaryBuffer;
    fInBuffer[2] = &fMetaDataSetup;
    fInBufferIds[0] = IN_AUDIO_DATA;
    fInBufferIds[1] = IN_ANCILLRY_DATA;
    fInBufferIds[2] = IN_METADATA_SETUP;
    fInBufferSize[0] = sizeof(fInputBuffer);
    fInBufferSize[1] = sizeof(fAncillaryBuffer);
    fInBufferSize[2] = sizeof(fMetaDataSetup);
    fInBufferElSize[0] = sizeof(INT_PCM);
    fInBufferElSize[1] = sizeof(UCHAR);
    fInBufferElSize[2] = sizeof(AACENC_MetaData);
    fOutBuffer[0] = fOutputBuffer;
    fOutBufferIds[0] = OUT_BITSTREAM_DATA;
    fOutBufferSize[0] = sizeof(fOutputBuffer);
    fOutBufferElSize[0] = sizeof(UCHAR);

    fInBufDesc.numBufs = sizeof(fInBuffer) / sizeof(void*);
    fInBufDesc.bufs = (void**) &fInBuffer;
    fInBufDesc.bufferIdentifiers = fInBufferIds;
    fInBufDesc.bufSizes = fInBufferSize;
    fInBufDesc.bufElSizes = fInBufferElSize;

    fOutBufDesc.numBufs = sizeof(fOutBuffer) / sizeof(void*);
    fOutBufDesc.bufs = (void**) &fOutBuffer;
    fOutBufDesc.bufferIdentifiers = fOutBufferIds;
    fOutBufDesc.bufSizes = fOutBufferSize;
    fOutBufDesc.bufElSizes = fOutBufferElSize;

    fPermanentOutputBufferSize = 0;

    if (debug & 2) {
        fprintf(stderr, "%lld - ADTSFromWAVAudioFifoSource - inargs.numInSamples %d\n", current_timestamp(), fInargs.numInSamples);
        fprintf(stderr, "%lld - ADTSFromWAVAudioFifoSource - encInfo.inputChannels %d\n", current_timestamp(), fEncInfo.inputChannels);
        fprintf(stderr, "%lld - ADTSFromWAVAudioFifoSource - encInfo.frameLength %d\n", current_timestamp(), fEncInfo.frameLength);
        fprintf(stderr, "%lld - ADTSFromWAVAudioFifoSource - sizeof(INT_PCM) %d\n", current_timestamp(), sizeof(INT_PCM));
        fprintf(stderr, "%lld - ADTSFromWAVAudioFifoSource - SAMPLE_BITS %d\n", current_timestamp(), SAMPLE_BITS);
        fprintf(stderr, "%lld - ADTSFromWAVAudioFifoSource - encInfo.inputChannels * encInfo.frameLength %d\n", current_timestamp(), fEncInfo.inputChannels * fEncInfo.frameLength);
        fprintf(stderr, "%lld - ADTSFromWAVAudioFifoSource - FDKmin(encInfo.inputChannels * encInfo.frameLength, sizeof(inputBuffer) / sizeof(INT_PCM) - inargs.numInSamples) %d\n",
                current_timestamp(), FDKmin(fEncInfo.inputChannels * fEncInfo.frameLength, sizeof(fInputBuffer) / sizeof(INT_PCM) - fInargs.numInSamples));
    }

    // All future reads (of audio samples) from the file will be asynchronous:
    if (!makeSocketNonBlocking(fileno(fFid)))
        fprintf(stderr, "%lld - ADTSFromWAVAudioFifoSource - Error setting socket non blocking\n", current_timestamp());
}

ADTSFromWAVAudioFifoSource::~ADTSFromWAVAudioFifoSource() {
    aacEncClose(&fHAacEncoder);
    CloseInputFile(fFid);
}

void ADTSFromWAVAudioFifoSource::doStopGettingFrames() {
    fHaveStartedReading = False;
}

void ADTSFromWAVAudioFifoSource::doGetNextFrameTask(void* clientData) {
    ADTSFromWAVAudioFifoSource *source = (ADTSFromWAVAudioFifoSource *) clientData;
    source->doGetNextFrameEx();
}

void ADTSFromWAVAudioFifoSource::doGetNextFrameEx() {
    doGetNextFrame();
}

void ADTSFromWAVAudioFifoSource::doGetNextFrame() {

    Boolean isFirstReading = !fHaveStartedReading;
    if (!fHaveStartedReading) {
        if (debug & 2) fprintf(stderr, "%lld: ADTSFromWAVAudioFifoSource - doGetNextFrame() 1st start\n", current_timestamp());
        fHaveStartedReading = True;
    }

    int numInBytes;
    int ErrorStatus;
    int numToRead;

    // Feed input buffer with 1 frame
    // PCM 16 bit, 8KHz, mono = 2048 bytes to read per frame
    // 1 aac frame every 128 ms
    numToRead = sizeof(INT_PCM) * (fEncInfo.inputChannels * fEncInfo.frameLength - fInputBufferSize);
    if (debug & 2) fprintf(stderr, "%lld: ADTSFromWAVAudioFifoSource - starting read\n", current_timestamp());
    numInBytes = read(fileno(fFid),
                 ((UCHAR *) fInputBuffer) + sizeof(INT_PCM) * fInputBufferSize,
                 numToRead);
    if (debug & 2) fprintf(stderr, "%lld: ADTSFromWAVAudioFifoSource - read completed\n", current_timestamp());
    if (numInBytes <= -1) {
        // TODO
        fprintf(stderr, "%lld: ADTSFromWAVAudioFifoSource - error reading file, try again later\n", current_timestamp());
        // Trick to avoid segfault with StreamReplicator
        if (isFirstReading) {
            nextTask() = envir().taskScheduler().scheduleDelayedTask(0,
                    (TaskFunc*)FramedSource::afterGetting, this);
        } else {
            nextTask() = envir().taskScheduler().scheduleDelayedTask(fuSecsPerFrame/4,
                    (TaskFunc*) ADTSFromWAVAudioFifoSource::doGetNextFrameTask, this);
        }
        return;
    }
    fInputBufferSize += (numInBytes / sizeof(INT_PCM));
    if (fInputBufferSize != sizeof(fInputBuffer) / sizeof(INT_PCM)) {
        fprintf(stderr, "%lld: ADTSFromWAVAudioFifoSource - buffer not full, read again later\n", current_timestamp());
        // Trick to avoid segfault with StreamReplicator
        if (isFirstReading) {
            nextTask() = envir().taskScheduler().scheduleDelayedTask(0,
                    (TaskFunc*)FramedSource::afterGetting, this);
        } else {
            nextTask() = envir().taskScheduler().scheduleDelayedTask(fuSecsPerFrame/4,
                    (TaskFunc*) ADTSFromWAVAudioFifoSource::doGetNextFrameTask, this);
        }
        return;
    }

    fInargs.numInSamples = fInputBufferSize;
    fInputBufferSize = 0;
    if (debug & 2) {
        fprintf(stderr, "%lld: ADTSFromWAVAudioFifoSource - fInargs.numInSamples %d\n", current_timestamp(), fInargs.numInSamples);
        fprintf(stderr, "%lld: ADTSFromWAVAudioFifoSource - fOutargs.numInSamples %d\n", current_timestamp(), fOutargs.numInSamples);
    }

    // Encode input samples
    ErrorStatus = aacEncEncode(fHAacEncoder, &fInBufDesc, &fOutBufDesc, &fInargs, &fOutargs);
    if (debug & 2) {
        fprintf(stderr, "%lld: ADTSFromWAVAudioFifoSource - ErrorStatus %d\n", current_timestamp(), ErrorStatus);

        fprintf(stderr, "%lld: ADTSFromWAVAudioFifoSource - fInargs.numInSamples %d\n", current_timestamp(), fInargs.numInSamples);
        fprintf(stderr, "%lld: ADTSFromWAVAudioFifoSource - fOutargs.numInSamples %d\n", current_timestamp(), fOutargs.numInSamples);

        fprintf(stderr, "%lld: ADTSFromWAVAudioFifoSource - outargs.numOutBytes %d\n", current_timestamp(), fOutargs.numOutBytes);
    }
    if (fOutargs.numOutBytes > 0) {

        // Begin by reading the 7-byte fixed_variable headers:
        unsigned char *headers = fOutputBuffer; // length = 7
        unsigned char *payload = fOutputBuffer + 7 * sizeof(unsigned char);

        // Extract important fields from the headers:
        Boolean protection_absent = headers[1]&0x01;
        u_int16_t frame_length
            = ((headers[3]&0x03)<<11) | (headers[4]<<3) | ((headers[5]&0xE0)>>5);
        if (debug & 2) {
            u_int16_t syncword = (headers[0]<<4) | (headers[1]>>4);
            fprintf(stderr, "%lld: ADTSFromWAVAudioFifoSource - Read frame: syncword 0x%x, protection_absent %d, frame_length %d\n", current_timestamp(), syncword, protection_absent, frame_length);
            if (syncword != 0xFFF) fprintf(stderr, "%lld: ADTSFromWAVAudioFifoSource - WARNING: Bad syncword!\n", current_timestamp());
        }
        unsigned numBytesToRead
            = frame_length > 7 ? frame_length - 7 : 0;

        // If there's a 'crc_check' field, skip it:
        if (!protection_absent) {
            payload += 2 * sizeof(unsigned char);
            numBytesToRead = numBytesToRead > 2 ? numBytesToRead - 2 : 0;
        }

        memmove(&fPermanentOutputBuffer[fPermanentOutputBufferSize],
                    payload, numBytesToRead);
        fPermanentOutputBufferSize += numBytesToRead;

        // Next, set the pointer to the raw frame data:
        if (fPermanentOutputBufferSize > fMaxSize) {
            fNumTruncatedBytes = fPermanentOutputBufferSize - fMaxSize;
            fFrameSize = fMaxSize;
        } else {
            fNumTruncatedBytes = 0;
            fFrameSize = fPermanentOutputBufferSize;
        }

        memmove(fTo, fPermanentOutputBuffer, fFrameSize);
        if (fNumTruncatedBytes > 0) {
            memmove(fPermanentOutputBuffer, &fPermanentOutputBuffer[fFrameSize], fNumTruncatedBytes);

            if (debug & 2) fprintf(stderr, "%lld: ADTSFromWAVAudioFifoSource - End of buffer: fFrameSize %d, fNumTruncatedBytes %d\n", current_timestamp(), fFrameSize, fNumTruncatedBytes);
        }
        fPermanentOutputBufferSize -= fFrameSize;
    }
    else
    {
        fFrameSize = 0;
        fNumTruncatedBytes = 0;
    }

#ifndef PRES_TIME_CLOCK
    // Set the 'presentation time':
    if (fPresentationTime.tv_sec == 0 && fPresentationTime.tv_usec == 0) {
        // This is the first frame, so use the current time:
        gettimeofday(&fPresentationTime, NULL);
    } else {
        // Increment by the play time of the previous frame:
        unsigned uSeconds = fPresentationTime.tv_usec + fuSecsPerFrame;
        fPresentationTime.tv_sec += uSeconds/1000000;
        fPresentationTime.tv_usec = uSeconds%1000000;
    }
#else
    // Use system clock to set presentation time
    gettimeofday(&fPresentationTime, NULL);
#endif

    fDurationInMicroseconds = fuSecsPerFrame;

    // Switch to another task, and inform the reader that he has data:
    nextTask() = envir().taskScheduler().scheduleDelayedTask(0,
                                (TaskFunc*)FramedSource::afterGetting, this);
}
