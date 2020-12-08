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
// Copyright (c) 1996-2019, Live Networks, Inc.  All rights reserved
// A test program that demonstrates how to stream - via unicast RTP
// - various kinds of file on demand, using a built-in RTSP server.
// main program

#include <sys/stat.h>

#include "liveMedia.hh"
#include "BasicUsageEnvironment.hh"
#include "WAVAudioFifoServerMediaSubsession.hh"
#include "WAVAudioFifoSource.hh"
#include "StreamReplicator.hh"
#include "YiNoiseReduction.hh"
#include "DummySink.hh"
#include "aLawAudioFilter.hh"

UsageEnvironment* env;

// To make the second and subsequent client for each stream reuse the same
// input stream as the first client (rather than playing the file from the
// start for each client), change the following "False" to "True":
Boolean reuseFirstSource = True;

// To stream *only* MPEG-1 or 2 video "I" frames
// (e.g., to reduce network bandwidth),
// change the following "False" to "True":
Boolean iFramesOnly = False;

StreamReplicator* startReplicatorStream(const char* inputAudioFileName, int convertToxLaw, unsigned int nr_level) {
    // Create a single WAVAudioFifo source that will be replicated for mutliple streams
    WAVAudioFifoSource* wavSource = WAVAudioFifoSource::createNew(*env, inputAudioFileName);
    if (wavSource == NULL) {
        *env << "Failed to create Fifo Source \n";
    }

    // Optionally enable the noise reduction filter
    FramedSource* intermediateSource;
    if (nr_level > 0) {
        intermediateSource = YiNoiseReduction::createNew(*env, wavSource, nr_level);
    } else {
        intermediateSource = wavSource;
    }

    // Optionally convert to uLaw or aLaw pcm
    FramedSource* resultSource;
    if (convertToxLaw == WA_PCMA) {
        resultSource = aLawFromPCMAudioSource::createNew(*env, intermediateSource, 1/*little-endian*/);
    } else if (convertToxLaw == WA_PCMU) {
        resultSource = uLawFromPCMAudioSource::createNew(*env, intermediateSource, 1/*little-endian*/);
    } else {
        resultSource = EndianSwap16::createNew(*env, intermediateSource);
    }

    // Create and start the replicator that will be given to each subsession
    StreamReplicator* replicator = StreamReplicator::createNew(*env, resultSource);

    // Begin by creating an input stream from our replicator:
    FramedSource* source = replicator->createStreamReplica();

    // Then create a 'dummy sink' object to receive thie replica stream:
    MediaSink* sink = DummySink::createNew(*env, "dummy");

    // Now, start playing, feeding the sink object from the source:
    sink->startPlaying(*source, NULL, NULL);

    return replicator;
}


static void announceStream(RTSPServer* rtspServer, ServerMediaSession* sms,
                                char const* streamName, char const* inputFileName, int audio) {
    char* url = rtspServer->rtspURL(sms);
    UsageEnvironment& env = rtspServer->envir();
    env << "\n\"" << streamName << "\" stream, from the file \""
        << inputFileName << "\"\n";
    if (audio)
        env << "Audio enabled\n";
    env << "Play this stream using the URL \"" << url << "\"\n";
    delete[] url;
}

int main(int argc, char** argv) {

    char *str;
    int res = 0;
    int port = 554;
    int audio = 1;
    int nm;
    char user[65];
    char pwd[65];
    int nr_level = 0;

    int convertToxLaw = WA_PCMU;
    char const* inputAudioFileName = "/tmp/audio_fifo";
    struct stat stat_buffer;

    str = getenv("RRTSP_RES");
    if (str && sscanf (str, "%i", &nm) == 1 && nm >= 0) {
        if ((nm >= 0) && (nm <=2)) {
            res = nm;
        }
    }

    str = getenv("RRTSP_AUDIO");
    if (str != NULL) {
        if (strcasecmp("alaw", str) == 0) {
            audio = 1;
            convertToxLaw = WA_PCMA;
        } else if (strcasecmp("ulaw", str) == 0) {
            audio = 1;
            convertToxLaw = WA_PCMU;
        } else if (strcasecmp("pcm", str) == 0) {
            audio = 1;
            convertToxLaw = WA_PCM;
        }
    }

    str = getenv("RRTSP_PORT");
    if (str && sscanf (str, "%i", &nm) == 1 && nm >= 0) {
        port = nm;
    }

    memset(user, 0, sizeof(user));
    str = getenv("RRTSP_USER");
    if ((str != NULL) && (strlen(str) < sizeof(user))) {
        strcpy(user, str);
    }

    memset(pwd, 0, sizeof(pwd));
    str = getenv("RRTSP_PWD");
    if ((str != NULL) && (strlen(str) < sizeof(pwd))) {
        strcpy(pwd, str);
    }

    str = getenv("NR_LEVEL");
    if (str && sscanf (str, "%i", &nm) == 1 && nm >= 0 && nm <= 30) {
        nr_level = nm;
    }

    // If fifo doesn't exist, disable audio
    if (stat (inputAudioFileName, &stat_buffer) != 0) {
        *env << "Audio fifo does not exist, disabling audio.n";
        audio = 0;
    }

    // Begin by setting up our usage environment:
    TaskScheduler* scheduler = BasicTaskScheduler::createNew();
    env = BasicUsageEnvironment::createNew(*scheduler);

    UserAuthenticationDatabase* authDB = NULL;

    if ((user[0] != '\0') && (pwd[0] != '\0')) {
        // To implement client access control to the RTSP server, do the following:
        authDB = new UserAuthenticationDatabase;
        authDB->addUserRecord(user, pwd);
        // Repeat the above with each <username>, <password> that you wish to allow
        // access to the server.
    }

    // Create the RTSP server:
    RTSPServer* rtspServer = RTSPServer::createNew(*env, port, authDB);
    if (rtspServer == NULL) {
        *env << "Failed to create RTSP server: " << env->getResultMsg() << "\n";
        exit(1);
    }

    // Create and start the replicator that will be given to each subsession
    StreamReplicator* replicator = startReplicatorStream(inputAudioFileName, convertToxLaw, nr_level);

    char const* descriptionString
        = "Session streamed by \"rRTSPServer\"";

    // Set up each of the possible streams that can be served by the
    // RTSP server.  Each such stream is implemented using a
    // "ServerMediaSession" object, plus one or more
    // "ServerMediaSubsession" objects for each audio/video substream.

    // A H.264 video elementary stream:
    if ((res == 0) || (res == 2))
    {
        char const* streamName = "ch0_0.h264";
        char const* inputFileName = "/tmp/h264_high_fifo";

        // First, make sure that the RTPSinks' buffers will be large enough to handle the huge size of DV frames (as big as 288000).
        OutPacketBuffer::maxSize = 300000;

        ServerMediaSession* sms_high
        = ServerMediaSession::createNew(*env, streamName, streamName,
                                descriptionString);
        sms_high->addSubsession(H264VideoFileServerMediaSubsession
                                ::createNew(*env, inputFileName, reuseFirstSource));
        if (audio == 1) {
            sms_high->addSubsession(WAVAudioFifoServerMediaSubsession
                                   ::createNew(*env, replicator, reuseFirstSource, convertToxLaw));
        }
        rtspServer->addServerMediaSession(sms_high);

        announceStream(rtspServer, sms_high, streamName, inputFileName, audio);
    }

    // A H.264 video elementary stream:
    if ((res == 1) || (res == 2))
    {
        char const* streamName = "ch0_1.h264";
        char const* inputFileName = "/tmp/h264_low_fifo";

        // First, make sure that the RTPSinks' buffers will be large enough to handle the huge size of DV frames (as big as 288000).
        OutPacketBuffer::maxSize = 300000;

        ServerMediaSession* sms_low
        = ServerMediaSession::createNew(*env, streamName, streamName,
                                descriptionString);
        sms_low->addSubsession(H264VideoFileServerMediaSubsession
                                ::createNew(*env, inputFileName, reuseFirstSource));
        if (audio == 1) {
            sms_low->addSubsession(WAVAudioFifoServerMediaSubsession
                                ::createNew(*env, replicator, reuseFirstSource, convertToxLaw));
        }
        rtspServer->addServerMediaSession(sms_low);

        announceStream(rtspServer, sms_low, streamName, inputFileName, audio);
    }

    // A PCM audio elementary stream:
    if (audio)
    {
        char const* streamName = "ch0_2.h264";

        // First, make sure that the RTPSinks' buffers will be large enough to handle the huge size of DV frames (as big as 288000).
        OutPacketBuffer::maxSize = 300000;

        ServerMediaSession* sms_audio
            = ServerMediaSession::createNew(*env, streamName, streamName,
                                              descriptionString);
        sms_audio->addSubsession(WAVAudioFifoServerMediaSubsession
                                   ::createNew(*env, replicator, reuseFirstSource, convertToxLaw));
        rtspServer->addServerMediaSession(sms_audio);

        announceStream(rtspServer, sms_audio, streamName, inputAudioFileName, audio);
    }

    // Also, attempt to create a HTTP server for RTSP-over-HTTP tunneling.
    // Try first with the default HTTP port (80), and then with the alternative HTTP
    // port numbers (8000 and 8080).
/*
    if (rtspServer->setUpTunnelingOverHTTP(80) || rtspServer->setUpTunnelingOverHTTP(8000) || rtspServer->setUpTunnelingOverHTTP(8080)) {
        *env << "\n(We use port " << rtspServer->httpServerPortNum() << " for optional RTSP-over-HTTP tunneling.)\n";
    } else {
        *env << "\n(RTSP-over-HTTP tunneling is not available.)\n";
    }
*/
    env->taskScheduler().doEventLoop(); // does not return

    return 0; // only to prevent compiler warning
}
