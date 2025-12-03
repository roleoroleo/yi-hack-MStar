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
 * ServerMediaSubsession object that creates new, unicast, RTPSink
 * on demand, from an ADTS fifo file.
 */

#include "ADTSAudioFifoServerMediaSubsession.hh"
#include "ADTSAudioFileSource.hh"
#include "MPEG4GenericRTPSink.hh"
#include "FramedFilter.hh"
#include "rRTSPServer.h"

extern int debug;

ADTSAudioFifoServerMediaSubsession* ADTSAudioFifoServerMediaSubsession
::createNew(UsageEnvironment& env, StreamReplicator* replicator, Boolean reuseFirstSource) {

    return new ADTSAudioFifoServerMediaSubsession(env, replicator, reuseFirstSource);
}

ADTSAudioFifoServerMediaSubsession
::ADTSAudioFifoServerMediaSubsession(UsageEnvironment& env, StreamReplicator* replicator, Boolean reuseFirstSource)
    : OnDemandServerMediaSubsession(env, reuseFirstSource),
      fReplicator(replicator) {
}

ADTSAudioFifoServerMediaSubsession
::~ADTSAudioFifoServerMediaSubsession() {
}

FramedSource* ADTSAudioFifoServerMediaSubsession
::createNewStreamSource(unsigned /*clientSessionId*/, unsigned& estBitrate) {
    if (debug & 8) fprintf(stderr, "%lld: ADTSAudioFifoServerMediaSubsession - ADTSAudioFifoServerMediaSubsession createNewStreamSource\n", current_timestamp());
    FramedSource* resultSource = NULL;
    ADTSAudioFileSource* originalSource = NULL;
    FramedFilter* previousSource = (FramedFilter*)fReplicator->inputSource();

    if (previousSource == NULL) {
        fprintf(stderr, "%lld: ADTSAudioFifoServerMediaSubsession - Failed to create stream replica\n", current_timestamp());
        Medium::close(previousSource);
        return NULL;
    }

    // Iterate back into the filter chain until a source is found that 
    // has a bits per sample = 16 and expected to be a WAVAudioFifoSource.
    for (int x = 0; x < 10; x++) {
        if (((ADTSAudioFileSource*)(previousSource))->samplingFrequency() > 0) {
            if (debug & 8) fprintf(stderr, "%lld: ADTSAudioFifoServerMediaSubsession - ADTSAudioFileSource found at x = %d\n", current_timestamp(), x);
            originalSource = (ADTSAudioFileSource*)(previousSource);
            break;
        }
        previousSource = (FramedFilter*)previousSource->inputSource();
    }
    if (debug & 8) fprintf(stderr, "%lld: ADTSAudioFifoServerMediaSubsession - fReplicator->inputSource() = %p\n", current_timestamp(), originalSource);
    resultSource = fReplicator->createStreamReplica();

    if (resultSource == NULL) {
        fprintf(stderr, "%lld: ADTSAudioFifoServerMediaSubsession - Failed to create stream replica\n", current_timestamp());
//        Medium::close(resultSource);
        return NULL;
    } else {
        fSamplingFrequency = originalSource->samplingFrequency();
        fNumChannels = originalSource->numChannels();
        memcpy(fConfigStr, originalSource->configStr(), 5);

        if (debug & 8) fprintf(stderr, "%lld: ADTSAudioFifoServerMediaSubsession - Original source samplingFrequency: %d numChannels: %d configStr: %s\n",
                           current_timestamp(), originalSource->samplingFrequency(), originalSource->numChannels(), originalSource->configStr());

        estBitrate = 32; // kbps, estimate
        return resultSource;
    }
}

RTPSink* ADTSAudioFifoServerMediaSubsession
::createNewRTPSink(Groupsock* rtpGroupsock,
                   unsigned char rtpPayloadTypeIfDynamic,
                   FramedSource* /*inputSource*/) {

    if (debug & 8) {
        fprintf(stderr, "%lld: ADTSAudioFifoServerMediaSubsession - samplingFrequency %d\nconfigStr %s\nnumChannels %d\n",
                current_timestamp(),
                fSamplingFrequency,
                fConfigStr,
                fNumChannels);
    }
    return MPEG4GenericRTPSink::createNew(envir(), rtpGroupsock,
                      rtpPayloadTypeIfDynamic,
                      fSamplingFrequency,
                      "audio", "AAC-hbr", fConfigStr,
                      fNumChannels);
}
