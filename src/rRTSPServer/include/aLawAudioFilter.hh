/*
 * Copyright (c) 2024 roleo.
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
 * Filter for converting between raw PCM audio and aLaw
 */

#ifndef _ALAW_AUDIO_FILTER_HH
#define _ALAW_AUDIO_FILTER_HH

#ifndef _FRAMED_FILTER_HH
#include "FramedFilter.hh"
#endif

////////// 16-bit PCM (in various byte orderings) -> 8-bit a-Law //////////

class aLawFromPCMAudioSource: public FramedFilter {
public:
  static aLawFromPCMAudioSource*
  createNew(UsageEnvironment& env, FramedSource* inputSource,
	    int byteOrdering = 0);
  // "byteOrdering" == 0 => host order (the default)
  // "byteOrdering" == 1 => little-endian order
  // "byteOrdering" == 2 => network (i.e., big-endian) order

protected:
  aLawFromPCMAudioSource(UsageEnvironment& env, FramedSource* inputSource,
			 int byteOrdering);
      // called only by createNew()
  virtual ~aLawFromPCMAudioSource();

private:
  // Redefined virtual functions:
  virtual void doGetNextFrame();

private:
  static void afterGettingFrame(void* clientData, unsigned frameSize,
				unsigned numTruncatedBytes,
				struct timeval presentationTime,
				unsigned durationInMicroseconds);
  void afterGettingFrame1(unsigned frameSize,
			  unsigned numTruncatedBytes,
			  struct timeval presentationTime,
			  unsigned durationInMicroseconds);

private:
  int fByteOrdering;
  unsigned char* fInputBuffer;
  unsigned fInputBufferSize;
};


////////// a-Law -> 16-bit PCM (in host order) //////////

class PCMFromaLawAudioSource: public FramedFilter {
public:
  static PCMFromaLawAudioSource*
  createNew(UsageEnvironment& env, FramedSource* inputSource);

protected:
  PCMFromaLawAudioSource(UsageEnvironment& env,
			 FramedSource* inputSource);
      // called only by createNew()
  virtual ~PCMFromaLawAudioSource();

private:
  // Redefined virtual functions:
  virtual void doGetNextFrame();

private:
  static void afterGettingFrame(void* clientData, unsigned frameSize,
				unsigned numTruncatedBytes,
				struct timeval presentationTime,
				unsigned durationInMicroseconds);
  void afterGettingFrame1(unsigned frameSize,
			  unsigned numTruncatedBytes,
			  struct timeval presentationTime,
			  unsigned durationInMicroseconds);

private:
  unsigned char* fInputBuffer;
  unsigned fInputBufferSize;
};

#endif
