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
// A fifo file source that is a plain byte stream (rather than frames)
// Implementation

#include "ByteStreamFifoSource.hh"
#include "InputFile.hh"
#include "GroupsockHelper.hh"
#include "rRTSPServer.h"

#include <fcntl.h>

extern int debug;

////////// ByteStreamFifoSource //////////

ByteStreamFifoSource*
ByteStreamFifoSource::createNew(UsageEnvironment& env, char const* fileName,
                                unsigned playTimePerFrame) {
    FILE* fid = OpenInputFile(env, fileName);
    if (fid == NULL) return NULL;

    makeSocketNonBlocking(fileno(fid));

    ByteStreamFifoSource* newSource
        = new ByteStreamFifoSource(env, fid, playTimePerFrame);

    return newSource;
}

ByteStreamFifoSource*
ByteStreamFifoSource::createNew(UsageEnvironment& env, FILE* fid,
                                unsigned playTimePerFrame) {
  if (fid == NULL) return NULL;

  ByteStreamFifoSource* newSource = new ByteStreamFifoSource(env, fid, playTimePerFrame);

  return newSource;
}

ByteStreamFifoSource::ByteStreamFifoSource(UsageEnvironment& env, FILE* fid,
                                           unsigned playTimePerFrame)
    : FramedFileSource(env, fid), fPlayTimePerFrame(playTimePerFrame),
      fHaveStartedReading(False) {
}

ByteStreamFifoSource::~ByteStreamFifoSource() {
    if (fFid == NULL) return;

    CloseInputFile(fFid);
}

void ByteStreamFifoSource::cleanFifo() {
    int flags;

    if (debug & 4) fprintf(stderr, "%lld: ByteStreamFifoSource - cleaning fifo\n", current_timestamp());

    if (fFid == NULL) return;

    // Set non blocking
    if ((flags = fcntl(fileno(fFid), F_GETFL, 0)) < 0) {
        return;
    }
    if (fcntl(fileno(fFid), F_SETFL, flags | O_NONBLOCK) != 0) {
        return;
    }

    // Clean fifo content
    unsigned char null[4];
    while (read(fileno(fFid), null, sizeof(null)) > 0) {}

    // Restore old blocking
    if (fcntl(fileno(fFid), F_SETFL, flags) != 0) {
        return;
    }

    if (debug & 4) fprintf(stderr, "%lld: ByteStreamFifoSource - fifo cleaned\n", current_timestamp());
}

void ByteStreamFifoSource::doGetNextFrame() {
    fFrameSize = 0; // until it's set later
    if (!fHaveStartedReading) {
        cleanFifo();
        fHaveStartedReading = True;
    }
    doReadFromFile();
}

void ByteStreamFifoSource::doStopGettingFrames() {
    if (debug & 4) fprintf(stderr, "%lld: ByteStreamFifoSource - doStopGettingFrames\n", current_timestamp());
    envir().taskScheduler().unscheduleDelayedTask(nextTask());
    fHaveStartedReading = False;
}

void ByteStreamFifoSource::doReadFromFile() {
    int wd_counter = 0;

    // Try to read as many bytes as will fit in the buffer provided
    fFrameSize = 0;
    while ((signed) fFrameSize <= 0) {
        usleep(2000);
        wd_counter++;
        if (wd_counter >= 1000) break;
        fFrameSize = read(fileno(fFid), fTo, fMaxSize);
    }
    if (debug & 4) fprintf(stderr, "%lld: ByteStreamFifoSource - fFrameSize %d\n", current_timestamp(), fFrameSize);
    if ((signed) fFrameSize <= 0) {
        handleClosure();
        return;
    }

    // Set the 'presentation time':
    // We don't know a specific play time duration for this data,
    // so just record the current time as being the 'presentation time':
    //fDurationInMicroseconds = fPlayTimePerFrame;
    gettimeofday(&fPresentationTime, NULL);

    // Switch to another task, and inform the reader that he has data:
    nextTask() = envir().taskScheduler().scheduleDelayedTask(0,
                         (TaskFunc*)FramedSource::afterGetting, this);

}
