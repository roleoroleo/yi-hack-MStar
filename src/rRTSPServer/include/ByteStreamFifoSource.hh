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
// A fifo source that is a plain byte stream (rather than frames)
// C++ header

#ifndef _BYTE_STREAM_FIFO_SOURCE_HH
#define _BYTE_STREAM_FIFO_SOURCE_HH

#ifndef _FRAMED_FILE_SOURCE_HH
#include "FramedFileSource.hh"
#endif

class ByteStreamFifoSource: public FramedFileSource {
public:
    static ByteStreamFifoSource* createNew(UsageEnvironment& env,
                                            char const* fileName,
                                            unsigned playTimePerFrame = 0);
    // "preferredFrameSize" == 0 means 'no preference'
    // "playTimePerFrame" is in microseconds

    static ByteStreamFifoSource* createNew(UsageEnvironment& env,
                                           FILE* fid,
                                           unsigned playTimePerFrame = 0);
    // an alternative version of "createNew()" that's used if you already have
    // an open file.

protected:
    ByteStreamFifoSource(UsageEnvironment& env,
                         FILE* fid,
                         unsigned playTimePerFrame);
    // called only by createNew()

    virtual ~ByteStreamFifoSource();

    void doReadFromFile();

private:
    // redefined virtual functions:
    virtual void doGetNextFrame();
    virtual void doStopGettingFrames();
    void cleanFifo();

private:
    unsigned fPlayTimePerFrame;
    Boolean fHaveStartedReading;
};

#endif
