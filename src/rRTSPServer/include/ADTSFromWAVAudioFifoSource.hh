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
 * A source object for WAV fifo file converted in AAC/ADTS format
 */

#ifndef _ADTS_FROM_WAV_AUDIO_FIFO_SOURCE_HH
#define _ADTS_FROM_WAV_AUDIO_FIFO_SOURCE_HH

#ifndef _FRAMED_FILE_SOURCE_HH
#include "FramedFileSource.hh"
#endif

#include "fdk-aac/aacenc_lib.h"

class ADTSFromWAVAudioFifoSource: public FramedSource {
public:
    static ADTSFromWAVAudioFifoSource* createNew(UsageEnvironment& env,
                                                 char const* fileName);

    unsigned samplingFrequency() const { return fSamplingFrequency; }
    unsigned numChannels() const { return fNumChannels; }
    char const* configStr() const { return fConfigStr; }
    static void doGetNextFrameTask(void *clientData);
    void doGetNextFrameEx();

private:
    ADTSFromWAVAudioFifoSource(UsageEnvironment& env, FILE* fid, u_int8_t profile,
                               u_int8_t samplingFrequencyIndex, u_int8_t channelConfiguration);
                               // called only by createNew()

    virtual ~ADTSFromWAVAudioFifoSource();

    static void fileReadableHandler(ADTSFromWAVAudioFifoSource* source, int mask);
    void doReadFromFile();

private:
    // redefined virtual functions:
    virtual void doGetNextFrame();
    virtual void doStopGettingFrames();
    void cleanFifo();

private:
    FILE *fFid;
    unsigned fSamplingFrequency;
    unsigned fNumChannels;
    unsigned fuSecsPerFrame;
    char fConfigStr[5];
    Boolean fHaveStartedReading;

    // fdk-aac
    HANDLE_AACENCODER fHAacEncoder;
    AACENC_InfoStruct fEncInfo;

    INT_PCM fInputBuffer[1024];
    UINT fInputBufferSize;
    UCHAR fAncillaryBuffer[50];
    AACENC_MetaData fMetaDataSetup;
    UCHAR fOutputBuffer[1024];

    UCHAR fPermanentOutputBuffer[1024];
    UINT fPermanentOutputBufferSize;

    void *fInBuffer[3];
    INT fInBufferIds[3];
    INT fInBufferSize[3];
    INT fInBufferElSize[3];
    void *fOutBuffer[1];
    INT fOutBufferIds[1];
    INT fOutBufferSize[1];
    INT fOutBufferElSize[1];

    AACENC_BufDesc fInBufDesc;
    AACENC_BufDesc fOutBufDesc;

    AACENC_InArgs fInargs;
    AACENC_OutArgs fOutargs;
};

#endif
