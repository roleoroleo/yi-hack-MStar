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
//
// Source template used to create this filter:
// Copyright (c) 1996-2020 Live Networks, Inc.  All rights reserved.
// Filters for converting between raw PCM audio and uLaw
// C++ header

#ifndef _AUDIO_FILTER_HH
#define _AUDIO_FILTER_HH

#ifndef _FRAMED_FILTER_HH
#include "FramedFilter.hh"
#endif

#include "../../../dummylib/YiAudioLibFuncs.h"

#define ALSA_BUF_SIZE 512

class YiNoiseReduction: public FramedFilter {
public:
  static YiNoiseReduction* createNew(UsageEnvironment& env, FramedSource* inputSource, unsigned int level);

protected:
  YiNoiseReduction(UsageEnvironment& env, FramedSource* inputSource, unsigned int level);
  // called only by createNew()
  virtual ~YiNoiseReduction();

private:
  // Redefined virtual functions:
  virtual void doGetNextFrame();

private:
  ApcStruct apStruct;
  void* apBuf;
  int apHandle;
  static void afterGettingFrame(void* clientData, unsigned frameSize,
				unsigned numTruncatedBytes,
				struct timeval presentationTime,
				unsigned durationInMicroseconds);
  void afterGettingFrame1(unsigned frameSize,
			  unsigned numTruncatedBytes,
			  struct timeval presentationTime,
			  unsigned durationInMicroseconds);
};

#endif
