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
// A 'ServerMediaSubsession' object that creates new, unicast, "RTPSink"s
// on demand, from an WAV audio file.
// Implementation

#include "YiAudioMediaSubsession.hh"
#include "SimpleRTPSink.hh"
#include "WAVSource.hh"
#include <uLawAudioFilter.hh>

YiAudioMediaSubsession* YiAudioMediaSubsession
::createNew(UsageEnvironment& env, Boolean reuseFirstSource) {
  return new YiAudioMediaSubsession(env, reuseFirstSource);
}

YiAudioMediaSubsession
::~YiAudioMediaSubsession() {
}

FramedSource* YiAudioMediaSubsession::createNewStreamSource(unsigned /*clientSessionId*/, unsigned& estBitrate) {
  FramedSource* resultSource = NULL;
  WAVSource* source = WAVSource::createNew(envir());
  if (source == NULL) {
    return NULL;
  }
  // 16000Hz * 1 channel * 16 bit and / 2 for uLaw
  estBitrate = (((WAVSource::samplingFrequency() * WAVSource::numChannels() * 16) + 500 ) / 1000); // kbps
#if 0
  resultSource = EndianSwap16::createNew(envir(), source);
#else	
  estBitrate = estBitrate / 2;
  resultSource = uLawFromPCMAudioSource::createNew(envir(), source, 1/*little-endian*/);
#endif
  return resultSource;
}

RTPSink* YiAudioMediaSubsession::createNewRTPSink(Groupsock* rtpGroupsock, unsigned char rtpPayloadTypeIfDynamic, FramedSource* /*inputSource*/) {
#if 0
  return SimpleRTPSink::createNew(envir(), rtpGroupsock,
          rtpPayloadTypeIfDynamic, WAVSource::samplingFrequency(),
          "audio", "L16", WAVSource::numChannels());
#else
  return SimpleRTPSink::createNew(envir(), rtpGroupsock,
          rtpPayloadTypeIfDynamic, WAVSource::samplingFrequency(),
          "audio", "PCMU", WAVSource::numChannels());
#endif
}
