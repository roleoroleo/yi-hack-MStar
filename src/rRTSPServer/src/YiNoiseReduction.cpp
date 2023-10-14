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
// Source template used to create this filter:
// Copyright (c) 1996-2020 Live Networks, Inc.  All rights reserved.
// Filters for converting between raw PCM audio and uLaw
// Implementation

#include "YiNoiseReduction.hh"
#include "WAVAudioFifoSource.hh"
#include "misc.hh"

extern int debug;

// Level defines the ammount of noise reduction. On YiHome1080 6FUS is was set to 30.
YiNoiseReduction*
YiNoiseReduction::createNew(UsageEnvironment& env, FramedSource* inputSource, unsigned int level) {
    return new YiNoiseReduction(env, inputSource, level);
}

YiNoiseReduction::YiNoiseReduction(UsageEnvironment& env, FramedSource* inputSource, unsigned int level)
    : FramedFilter(env, inputSource) {
    // Allocate a working buffer for the noise reduction functions.
    apBuf = malloc(IaaApc_GetBufferSize());
    if (apBuf == NULL) {
        fprintf(stderr, "%lld: YiNoiseReduction - Failed to allocate memory for buffer\n", current_timestamp());
        return;
    } 

    // Setup audio processing struct
    apStruct.point_number = 256; // Magic
    apStruct.channels = 1; // Mono
    apStruct.rate = ((WAVAudioFifoSource*)(inputSource))->samplingFrequency();
    if (debug & 2) fprintf(stderr, "%lld: YiNoiseReduction - Sampling freq input source: %d Hz\n", current_timestamp(), apStruct.rate);
    // Initialize noise reduction.
    int res = IaaApc_Init((char* const)apBuf, &apStruct);
    if (res != 0) {
        fprintf(stderr, "%lld: YiNoiseReduction - Audio processing init failed\n", current_timestamp());
        return;
    }

    // Enable noise reduction
    res = IaaApc_SetNrEnable(1);
    if (res != 0) {
        fprintf(stderr, "%lld: YiNoiseReduction - Failed setting level for noise reduction\n", current_timestamp());
        return;
    }
 
    // Set level
    res = IaaApc_SetNrMode(level);
    if (res != 0) {
        fprintf(stderr, "%lld: YiNoiseReduction - Failed setting level for noise reduction\n", current_timestamp());
        return;
    }
    if (debug & 2) fprintf(stderr, "%lld: YiNoiseReduction - Noise reduction enabled, level: %d\n", current_timestamp(), level);
}

YiNoiseReduction::~YiNoiseReduction() {
    // Free working buffer and noise reduction.
    IaaApc_Free();
    if (apBuf != NULL) {
        free(apBuf);
    }
}

void YiNoiseReduction::doGetNextFrame() {
    // Arrange to read data directly into the client's buffer:
    fInputSource->getNextFrame(fTo, fMaxSize,
			     afterGettingFrame, this,
                             FramedSource::handleClosure, this);
}

void YiNoiseReduction::afterGettingFrame(void* clientData, unsigned frameSize,
				     unsigned numTruncatedBytes,
				     struct timeval presentationTime,
				     unsigned durationInMicroseconds) {
    YiNoiseReduction* source = (YiNoiseReduction*)clientData;
    source->afterGettingFrame1(frameSize, numTruncatedBytes,
			     presentationTime, durationInMicroseconds);
}

void YiNoiseReduction::afterGettingFrame1(unsigned frameSize, unsigned numTruncatedBytes,
				      struct timeval presentationTime,
				      unsigned durationInMicroseconds) {
    // Provide the sample buffer to IaaApc_Run to execute noise reduction.
    // - The IaaApc_Run function replaces the original data.
    // - IaaApc_Run requires 512 samples to work on
    // - This was the previously hardcoded preferred framesize.
    int res = IaaApc_Run((int16_t*)fTo);
    if (res != 0) {
        fprintf(stderr, "%lld: YiNoiseReduction - Failure during executing IaaApc_Run(): %d\n", current_timestamp(), res);
    }

    // Complete delivery to the client:
    fFrameSize = frameSize;
    fNumTruncatedBytes = numTruncatedBytes + (frameSize - fFrameSize);
    fPresentationTime = presentationTime;
    fDurationInMicroseconds = durationInMicroseconds;
    afterGetting(this);
}
