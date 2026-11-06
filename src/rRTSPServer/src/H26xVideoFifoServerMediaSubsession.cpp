/*
 * Copyright (c) 2026 roleo.
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
 * ServerMediaSubsession objects that create new, unicast, RTPSink
 * on demand, from a H264/H265 fifo.
 */

#include "H26xVideoFifoServerMediaSubsession.hh"
#include "ByteStreamFileSource.hh"
#include "H264VideoStreamFramer.hh"
#include "H265VideoStreamFramer.hh"

#include <fcntl.h>
#include <unistd.h>

// Open the fifo O_RDWR | O_NONBLOCK:
// - open() never blocks, even if the grabber isn't running yet
// - we are a writer too, so read() never returns 0 (EOF) when the grabber
//   closes the fifo: ByteStreamFileSource doesn't call handleClosure()
//   and live555 doesn't reclaim the stream
// - a restarted grabber doesn't hang in open() waiting for a reader
static ByteStreamFileSource* openFifoSource(UsageEnvironment& env, char const* fileName)
{
    int fd = open(fileName, O_RDWR | O_NONBLOCK);
    if (fd < 0) {
        env.setResultMsg("unable to open fifo \"", fileName, "\"");
        return NULL;
    }
    FILE* fid = fdopen(fd, "rb");
    if (fid == NULL) {
        close(fd);
        env.setResultMsg("unable to fdopen fifo \"", fileName, "\"");
        return NULL;
    }

    return ByteStreamFileSource::createNew(env, fid);
}

H264VideoFifoServerMediaSubsession*
H264VideoFifoServerMediaSubsession::createNew(UsageEnvironment& env,
                                              char const* fileName,
                                              Boolean reuseFirstSource) {
    return new H264VideoFifoServerMediaSubsession(env, fileName, reuseFirstSource);
}

H264VideoFifoServerMediaSubsession::H264VideoFifoServerMediaSubsession(UsageEnvironment& env,
                                                                       char const* fileName, Boolean reuseFirstSource)
    : H264VideoFileServerMediaSubsession(env, fileName, reuseFirstSource) {
}

H264VideoFifoServerMediaSubsession::~H264VideoFifoServerMediaSubsession() {
}

FramedSource* H264VideoFifoServerMediaSubsession::createNewStreamSource(unsigned /*clientSessionId*/, unsigned& estBitrate) {
    estBitrate = 500; // kbps, estimate

    // Create the video source:
    ByteStreamFileSource* fileSource = openFifoSource(envir(), fFileName);
    if (fileSource == NULL) return NULL;
    fFileSize = 0;

    // Create a framer for the Video Elementary Stream:
    return H264VideoStreamFramer::createNew(envir(), fileSource);
}

H265VideoFifoServerMediaSubsession*
H265VideoFifoServerMediaSubsession::createNew(UsageEnvironment& env,
                                              char const* fileName,
                                              Boolean reuseFirstSource) {
    return new H265VideoFifoServerMediaSubsession(env, fileName, reuseFirstSource);
}

H265VideoFifoServerMediaSubsession::H265VideoFifoServerMediaSubsession(UsageEnvironment& env,
                                                                       char const* fileName, Boolean reuseFirstSource)
    : H265VideoFileServerMediaSubsession(env, fileName, reuseFirstSource) {
}

H265VideoFifoServerMediaSubsession::~H265VideoFifoServerMediaSubsession() {
}

FramedSource* H265VideoFifoServerMediaSubsession::createNewStreamSource(unsigned /*clientSessionId*/, unsigned& estBitrate) {
    estBitrate = 500; // kbps, estimate

    // Create the video source:
    ByteStreamFileSource* fileSource = openFifoSource(envir(), fFileName);
    if (fileSource == NULL) return NULL;
    fFileSize = 0;

    // Create a framer for the Video Elementary Stream:
    return H265VideoStreamFramer::createNew(envir(), fileSource);
}
