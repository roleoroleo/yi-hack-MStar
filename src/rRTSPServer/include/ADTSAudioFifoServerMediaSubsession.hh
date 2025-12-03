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
 * on demand, from an WAV fifo file.
 */

#ifndef _ADTS_AUDIO_FIFO_SERVER_MEDIA_SUBSESSION_HH
#define _ADTS_AUDIO_FIFO_SERVER_MEDIA_SUBSESSION_HH

#ifndef _FILE_SERVER_MEDIA_SUBSESSION_HH
#include "FileServerMediaSubsession.hh"
#endif
#include "StreamReplicator.hh"

class ADTSAudioFifoServerMediaSubsession: public OnDemandServerMediaSubsession {
public:
    static ADTSAudioFifoServerMediaSubsession*
    createNew(UsageEnvironment& env, StreamReplicator* replicator, Boolean reuseFirstSource);

protected:
    ADTSAudioFifoServerMediaSubsession(UsageEnvironment& env, StreamReplicator* replicator,
                                      Boolean reuseFirstSource);
    // called only by createNew();
    virtual ~ADTSAudioFifoServerMediaSubsession();

protected: // redefined virtual functions
    virtual FramedSource* createNewStreamSource(unsigned clientSessionId,
                                                unsigned& estBitrate);
    virtual RTPSink* createNewRTPSink(Groupsock* rtpGroupsock,
                                      unsigned char rtpPayloadTypeIfDynamic,
                                      FramedSource* inputSource);

protected:

    // The following parameters of the input stream are set after
    // "createNewStreamSource" is called:
    StreamReplicator* fReplicator;
    unsigned fSamplingFrequency;
    unsigned char fNumChannels;
    char fConfigStr[5];
};

#endif
