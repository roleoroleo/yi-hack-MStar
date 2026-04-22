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
 * A 'ServerMediaSubsession' object that creates new, unicast, "RTPSink"s
 * on demand, from an AAC fifo file in ADTS format
 * Implementation
 */

#include "ADTSFromWAVAudioFifoServerMediaSubsession.hh"
#include "ADTSFromWAVAudioFifoSource.hh"
#include "MPEG4GenericRTPSink.hh"
#include "FramedFilter.hh"
#include "rRTSPServer.h"

extern int debug;

ADTSFromWAVAudioFifoServerMediaSubsession*
ADTSFromWAVAudioFifoServerMediaSubsession::createNew(UsageEnvironment& env,
					     StreamReplicator* replicator,
					     Boolean reuseFirstSource) {
    return new ADTSFromWAVAudioFifoServerMediaSubsession(env, replicator, reuseFirstSource);
}

ADTSFromWAVAudioFifoServerMediaSubsession
::ADTSFromWAVAudioFifoServerMediaSubsession(UsageEnvironment& env,
				    StreamReplicator* replicator, Boolean reuseFirstSource)
    : OnDemandServerMediaSubsession(env, reuseFirstSource),
      fReplicator(replicator) {

    if (debug & 2) fprintf(stderr, "%lld: ADTSFromWAVAudioFifoServerMediaSubsession - New ADTSFromWAVAudioFifoServerMediaSubsession\n", current_timestamp());
}

ADTSFromWAVAudioFifoServerMediaSubsession
::~ADTSFromWAVAudioFifoServerMediaSubsession() {
}

FramedSource* ADTSFromWAVAudioFifoServerMediaSubsession
::createNewStreamSource(unsigned /*clientSessionId*/, unsigned& estBitrate) {
    if (debug & 2) fprintf(stderr, "%lld: ADTSFromWAVAudioFifoServerMediaSubsession - ADTSFromWAVAudioFifoServerMediaSubsession createNewStreamSource\n", current_timestamp());
    FramedSource* resultSource = NULL;
    ADTSFromWAVAudioFifoSource* originalSource = NULL;
    FramedFilter* previousSource = (FramedFilter*)fReplicator->inputSource();
    estBitrate = 32; // kbps, estimate

    // Iterate back into the filter chain until a source is found that 
    // has a sample frequency and expected to be a ADTSFromWAVAudioFifoSource.
    for (int x = 0; x < 10; x++) {
        if (((ADTSFromWAVAudioFifoSource*)(previousSource))->samplingFrequency() > 0) {
            if (debug & 2) fprintf(stderr, "%lld: ADTSFromWAVAudioFifoServerMediaSubsession - ADTSFromWAVAudioFifoSource found at x = %d\n", current_timestamp(), x);
            originalSource = (ADTSFromWAVAudioFifoSource*)(previousSource);
            break;
        }
        previousSource = (FramedFilter*)previousSource->inputSource();
    }
    if (debug & 2) fprintf(stderr, "%lld: ADTSFromWAVAudioFifoServerMediaSubsession - fReplicator->inputSource() = %p\n", current_timestamp(), originalSource);
    resultSource = fReplicator->createStreamReplica();

    if (resultSource == NULL) {
        fprintf(stderr, "%lld: ADTSFromWAVAudioFifoServerMediaSubsession - Failed to create stream replica\n", current_timestamp());
        Medium::close(resultSource);
        return NULL;
    } else {
        fSamplingFrequency = originalSource->samplingFrequency();
        fNumChannels = originalSource->numChannels();
        memcpy(fConfigStr, originalSource->configStr(), 5);

        if (debug & 2) fprintf(stderr, "%lld: ADTSFromWAVAudioFifoServerMediaSubsession - Original source samplingFrequency: %d numChannels: %d configStr: %s\n",
                           current_timestamp(), originalSource->samplingFrequency(), originalSource->numChannels(), originalSource->configStr());

        estBitrate = 32; // kbps, estimate
        return resultSource;
    }
}

RTPSink* ADTSFromWAVAudioFifoServerMediaSubsession
::createNewRTPSink(Groupsock* rtpGroupsock,
		   unsigned char rtpPayloadTypeIfDynamic,
		   FramedSource* /*inputSource*/) {

    if (debug & 2) {
        fprintf(stderr, "%lld: ADTSFromWAVAudioFifoServerMediaSubsession - samplingFrequency %d\nconfigStr %s\nnumChannels %d\n",
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
