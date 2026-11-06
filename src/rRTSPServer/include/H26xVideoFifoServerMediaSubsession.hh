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
 * The fifo is opened O_RDWR so the server is also a writer: when the
 * grabber is restarted read() doesn't return EOF and the stream goes on.
 */

#ifndef _H26X_VIDEO_FIFO_SERVER_MEDIA_SUBSESSION_HH
#define _H26X_VIDEO_FIFO_SERVER_MEDIA_SUBSESSION_HH

#include "H264VideoFileServerMediaSubsession.hh"
#include "H265VideoFileServerMediaSubsession.hh"

class H264VideoFifoServerMediaSubsession: public H264VideoFileServerMediaSubsession {
public:
    static H264VideoFifoServerMediaSubsession*
    createNew(UsageEnvironment& env, char const* fileName, Boolean reuseFirstSource);

protected:
    H264VideoFifoServerMediaSubsession(UsageEnvironment& env,
                                       char const* fileName, Boolean reuseFirstSource);
    // called only by createNew();
    virtual ~H264VideoFifoServerMediaSubsession();

protected: // redefined virtual functions
    virtual FramedSource* createNewStreamSource(unsigned clientSessionId,
                                                unsigned& estBitrate);
};

class H265VideoFifoServerMediaSubsession: public H265VideoFileServerMediaSubsession {
public:
    static H265VideoFifoServerMediaSubsession*
    createNew(UsageEnvironment& env, char const* fileName, Boolean reuseFirstSource);

protected:
    H265VideoFifoServerMediaSubsession(UsageEnvironment& env,
                                       char const* fileName, Boolean reuseFirstSource);
    // called only by createNew();
    virtual ~H265VideoFifoServerMediaSubsession();

protected: // redefined virtual functions
    virtual FramedSource* createNewStreamSource(unsigned clientSessionId,
                                                unsigned& estBitrate);
};

#endif
