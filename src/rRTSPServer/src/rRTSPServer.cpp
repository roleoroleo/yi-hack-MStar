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
 * Read h264 content from a pipe and send it to live555.
 */

#include "liveMedia.hh"
#include "BasicUsageEnvironment.hh"
#include "ADTSAudioFileServerMediaSubsession_BC.hh"
#include "PCMAudioFileServerMediaSubsession_BC.hh"
#include "WAVAudioFifoServerMediaSubsession.hh"
#include "WAVAudioFifoSource.hh"
#include "ADTSFromWAVAudioFifoServerMediaSubsession.hh"
#include "ADTSFromWAVAudioFifoSource.hh"
#include "StreamReplicator.hh"
#include "YiNoiseReduction.hh"
#include "aLawAudioFilter.hh"
#include "PCMFileSink.hh"

#include <sys/stat.h>
#include <sys/time.h>
#include <getopt.h>
#include <errno.h>
#include <limits.h>

#include "rRTSPServer.h"

#define RESOLUTION_NONE 0
#define RESOLUTION_LOW  360
#define RESOLUTION_HIGH 1080
#define RESOLUTION_BOTH 1440

#define CODEC_NONE -1
#define CODEC_H264 0
#define CODEC_H265 1

int debug = 0;
UsageEnvironment* env;

// To make the second and subsequent client for each stream reuse the same
// input stream as the first client (rather than playing the file from the
// start for each client), change the following "False" to "True":
Boolean reuseFirstSource = True;

// To stream *only* MPEG-1 or 2 video "I" frames
// (e.g., to reduce network bandwidth),
// change the following "False" to "True":
Boolean iFramesOnly = False;

long long current_timestamp() {
    struct timeval te;
    gettimeofday(&te, NULL); // get current time
    long long milliseconds = te.tv_sec*1000LL + te.tv_usec/1000; // calculate mi

    return milliseconds;
}

StreamReplicator* startReplicatorStream(const char* inputAudioFileName, unsigned samplingFrequency,
        unsigned char numChannels, unsigned char bitsPerSample, int convertTo, unsigned int nr_level) {

    FramedSource* resultSource = NULL;

    // Create a single WAVAudioFifo source that will be replicated for mutliple streams
    WAVAudioFifoSource* wavSource = WAVAudioFifoSource::createNew(*env, inputAudioFileName, samplingFrequency, numChannels, bitsPerSample);
    if (wavSource == NULL) {
        fprintf(stderr, "Failed to create Fifo Source \n");
    }

    // Optionally enable the noise reduction filter
    FramedSource* intermediateSource;
    if (nr_level > 0) {
        intermediateSource = YiNoiseReduction::createNew(*env, wavSource, nr_level);
    } else {
        intermediateSource = wavSource;
    }

    // Optionally convert to uLaw or aLaw pcm
    if (convertTo == WA_PCMA) {
        resultSource = aLawFromPCMAudioSource::createNew(*env, intermediateSource, 1/*little-endian*/);
    } else if (convertTo == WA_PCMU) {
        resultSource = uLawFromPCMAudioSource::createNew(*env, intermediateSource, 1/*little-endian*/);
    } else {
        resultSource = EndianSwap16::createNew(*env, intermediateSource);
    }

    // Create and start the replicator that will be given to each subsession
    StreamReplicator* replicator = StreamReplicator::createNew(*env, resultSource);

    // Begin by creating an input stream from our replicator:
    replicator->createStreamReplica();

    return replicator;
}

StreamReplicator* startReplicatorStream(const char* inputAudioFileName) {
    // Create a single ADTSFromWAVAudioFifo source that will be replicated for mutliple streams
    ADTSFromWAVAudioFifoSource* adtsSource = ADTSFromWAVAudioFifoSource::createNew(*env, inputAudioFileName);
    if (adtsSource == NULL) {
        fprintf(stderr, "Failed to create Fifo Source \n");
    }

    // Create and start the replicator that will be given to each subsession
    StreamReplicator* replicator = StreamReplicator::createNew(*env, adtsSource);

    // Begin by creating an input stream from our replicator:
    replicator->createStreamReplica();

    return replicator;
}

static void announceStream(RTSPServer* rtspServer, ServerMediaSession* sms, char const* streamName, char const* inputFileName, int audio, int h26x) {
    char* url = rtspServer->rtspURL(sms);
    fprintf(stderr, "\n\"%s\" stream, from the file \"%s\"\n", streamName, inputFileName);
    if (h26x == CODEC_H265)
        fprintf(stderr, "Video is H265\n");
    else if (h26x == CODEC_H264)
        fprintf(stderr, "Video is H264\n");
    if (audio == 1)
        fprintf(stderr, "PCM audio enabled\n");
    else if (audio == 2)
        fprintf(stderr, "AAC audio enabled\n");
    fprintf(stderr, "Play this stream using the URL \"%s\"\n", url);
    delete[] url;
}

void print_usage(char *progname)
{
    fprintf(stderr, "\nUsage: %s [-r RES] [-c CODEC] [-C CODEC] [-a AUDIO] [-p PORT] [-n LEVEL] [-u USER] [-w PASSWORD] [-d]\n\n", progname);
    fprintf(stderr, "\t-r RES,  --resolution RES\n");
    fprintf(stderr, "\t\tset resolution: low, high or both (default high)\n");
    fprintf(stderr, "\t-c CODEC,  --codec_low CODEC\n");
    fprintf(stderr, "\t\tset codec for low resolution: h264 or h265 (default h264)\n");
    fprintf(stderr, "\t-C CODEC,  --codec_high CODEC\n");
    fprintf(stderr, "\t\tset codec for high resolution: h264 or h265 (default h264)\n");
    fprintf(stderr, "\t-a AUDIO,  --audio AUDIO\n");
    fprintf(stderr, "\t\tset audio: yes, no, alaw, ulaw, pcm or aac (default yes)\n");
    fprintf(stderr, "\t-b CODEC, --audio_back_channel CODEC\n");
    fprintf(stderr, "\t\tenable audio back channel and set codec: alaw, ulaw or aac\n");
    fprintf(stderr, "\t-p PORT, --port PORT\n");
    fprintf(stderr, "\t\tset TCP port (default 554)\n");
    fprintf(stderr, "\t-n LEVEL, --nr_level LEVEL\n");
    fprintf(stderr, "\t\tset noise reduction level (only pcm and xlaw)\n");
    fprintf(stderr, "\t-u USER, --user USER\n");
    fprintf(stderr, "\t\tset username\n");
    fprintf(stderr, "\t-w PASSWORD, --password PASSWORD\n");
    fprintf(stderr, "\t\tset password\n");
    fprintf(stderr, "\t-d DEBUG, --debug DEBUG\n");
    fprintf(stderr, "\t\t0 none, 1 grabber, 2 rtsp library or 3 both\n");
    fprintf(stderr, "\t-h,      --help\n");
    fprintf(stderr, "\t\tprint this help\n");
}

int main(int argc, char** argv)
{
    char *str;
    int nm;
    int back_channel;
    char user[65];
    char pwd[65];
    int c;
    char *endptr;

    char const* inputAudioFileName = "/tmp/audio_fifo";
    char const* outputAudioFileName = "/tmp/audio_in_fifo";
    struct stat stat_buffer;

    // Setting default
    int resolution = RESOLUTION_HIGH;
    int codec_low = CODEC_H264;
    int codec_high = CODEC_H264;
    int audio = 1;
    int convertTo = WA_PCMU;
    int port = 554;
    int nr_level = 0;
    back_channel = 0;

    memset(user, 0, sizeof(user));
    memset(pwd, 0, sizeof(pwd));

    while (1) {
        static struct option long_options[] =
        {
            {"resolution",  required_argument, 0, 'r'},
            {"codec_low",  required_argument, 0, 'c'},
            {"codec_high",  required_argument, 0, 'C'},
            {"audio",  required_argument, 0, 'a'},
            {"audio_back_channel", required_argument, 0, 'b'},
            {"port",  required_argument, 0, 'p'},
            {"nr_level",  required_argument, 0, 'n'},
            {"user",  required_argument, 0, 'u'},
            {"password",  required_argument, 0, 'w'},
            {"debug",  required_argument, 0, 'd'},
            {"help",  no_argument, 0, 'h'},
            {0, 0, 0, 0}
        };
        /* getopt_long stores the option index here. */
        int option_index = 0;

        c = getopt_long (argc, argv, "r:c:C:a:b:p:n:u:w:d:h",
                         long_options, &option_index);

        /* Detect the end of the options. */
        if (c == -1)
            break;

        switch (c) {
        case 'r':
            if (strcasecmp("low", optarg) == 0) {
                resolution = RESOLUTION_LOW;
            } else if (strcasecmp("high", optarg) == 0) {
                resolution = RESOLUTION_HIGH;
            } else if (strcasecmp("both", optarg) == 0) {
                resolution = RESOLUTION_BOTH;
            }
            break;

        case 'c':
            if (strcasecmp("h264", optarg) == 0) {
                codec_low = CODEC_H264;
            } else if (strcasecmp("h265", optarg) == 0) {
                codec_low = CODEC_H265;
            }
            break;

        case 'C':
            if (strcasecmp("h264", optarg) == 0) {
                codec_high = CODEC_H264;
            } else if (strcasecmp("h265", optarg) == 0) {
                codec_high = CODEC_H265;
            }
            break;

        case 'a':
            if (strcasecmp("no", optarg) == 0) {
                audio = 0;
            } else if (strcasecmp("yes", optarg) == 0) {
                audio = 1;
                convertTo = WA_PCMU;
            } else if (strcasecmp("alaw", optarg) == 0) {
                audio = 1;
                convertTo = WA_PCMA;
            } else if (strcasecmp("ulaw", optarg) == 0) {
                audio = 1;
                convertTo = WA_PCMU;
            } else if (strcasecmp("pcm", optarg) == 0) {
                audio = 1;
                convertTo = WA_PCM;
            } else if (strcasecmp("aac", optarg) == 0) {
                audio = 2;
            }
            break;

        case 'b':
            if (strcasecmp("alaw", optarg) == 0) {
                back_channel = 1;
            } else if (strcasecmp("ulaw", optarg) == 0) {
                back_channel = 2;
            } else if (strcasecmp("aac", optarg) == 0) {
                back_channel = 4;
            } else {
                back_channel = 0;
            }
            break;

        case 'p':
            errno = 0;    /* To distinguish success/failure after call */
            port = strtol(optarg, &endptr, 10);

            /* Check for various possible errors */
            if ((errno == ERANGE && (port == LONG_MAX || port == LONG_MIN)) || (errno != 0 && port == 0)) {
                print_usage(argv[0]);
                exit(EXIT_FAILURE);
            }
            if (endptr == optarg) {
                print_usage(argv[0]);
                exit(EXIT_FAILURE);
            }
            break;

        case 'n':
            errno = 0;    /* To distinguish success/failure after call */
            nr_level = strtol(optarg, &endptr, 10);

            /* Check for various possible errors */
            if ((errno == ERANGE && (nr_level == LONG_MAX || nr_level == LONG_MIN)) || (errno != 0 && nr_level == 0)) {
                print_usage(argv[0]);
                exit(EXIT_FAILURE);
            }
            if (endptr == optarg) {
                print_usage(argv[0]);
                exit(EXIT_FAILURE);
            }
            break;

        case 'u':
            if (strlen(optarg) < sizeof(user)) {
                strcpy(user, optarg);
            }
            break;

        case 'w':
            if (strlen(optarg) < sizeof(pwd)) {
                strcpy(pwd, optarg);
            }
            break;

        case 'd':
            errno = 0;    /* To distinguish success/failure after call */
            debug = strtol(optarg, &endptr, 10);

            /* Check for various possible errors */
            if ((errno == ERANGE && (debug == LONG_MAX || debug == LONG_MIN)) || (errno != 0 && debug == 0)) {
                print_usage(argv[0]);
                exit(EXIT_FAILURE);
            }
            if (endptr == optarg) {
                print_usage(argv[0]);
                exit(EXIT_FAILURE);
            }
            if ((debug < 0) || (debug > 3)) {
                print_usage(argv[0]);
                exit(EXIT_FAILURE);
            }
            break;

        case 'h':
            print_usage(argv[0]);
            return -1;
            break;

        case '?':
            /* getopt_long already printed an error message. */
            break;

        default:
            print_usage(argv[0]);
            return -1;
        }
    }

    // Get parameters from environment
    str = getenv("RRTSP_RES");
    if (str != NULL) {
        if (strcasecmp("low", str) == 0) {
            resolution = RESOLUTION_LOW;
        } else if (strcasecmp("high", str) == 0) {
            resolution = RESOLUTION_HIGH;
        } else if (strcasecmp("both", str) == 0) {
            resolution = RESOLUTION_BOTH;
        }
    }

    str = getenv("RRTSP_CODEC_LOW");
    if (str != NULL) {
        if (strcasecmp("h264", str) == 0) {
            codec_low = CODEC_H264;
        } else if (strcasecmp("h265", str) == 0) {
            codec_low = CODEC_H265;
        }
    }

    str = getenv("RRTSP_CODEC_HIGH");
    if (str != NULL) {
        if (strcasecmp("h264", str) == 0) {
            codec_high = CODEC_H264;
        } else if (strcasecmp("h265", str) == 0) {
            codec_high = CODEC_H265;
        }
    }

    str = getenv("RRTSP_AUDIO");
    if (str != NULL) {
        if (strcasecmp("no", str) == 0) {
            audio = 0;
        } else if (strcasecmp("yes", str) == 0) {
            audio = 1;
            convertTo = WA_PCMU;
        } else if (strcasecmp("alaw", str) == 0) {
            audio = 1;
            convertTo = WA_PCMA;
        } else if (strcasecmp("ulaw", str) == 0) {
            audio = 1;
            convertTo = WA_PCMU;
        } else if (strcasecmp("pcm", str) == 0) {
            audio = 1;
            convertTo = WA_PCM;
        } else if (strcasecmp("aac", str) == 0) {
            audio = 2;
        }
    }

    str = getenv("RRTSP_AUDIO_BC");
    if (str != NULL) {
        if (strcasecmp("alaw", str) == 0) {
            back_channel = 1;
        } else if (strcasecmp("ulaw", str) == 0) {
            back_channel = 2;
        } else if (strcasecmp("aac", str) == 0) {
            back_channel = 4;
        } else {
            back_channel = 0;
        }
    }

    str = getenv("RRTSP_PORT");
    if ((str != NULL) && (sscanf (str, "%i", &nm) == 1) && (nm >= 0)) {
        port = nm;
    }

    str = getenv("RRTSP_DEBUG");
    if ((str != NULL) && (sscanf (str, "%i", &nm) == 1) && (nm >= 0)) {
        debug = nm;
    }

    str = getenv("RRTSP_USER");
    if ((str != NULL) && (strlen(str) < sizeof(user))) {
        strcpy(user, str);
    }

    str = getenv("RRTSP_PWD");
    if ((str != NULL) && (strlen(str) < sizeof(pwd))) {
        strcpy(pwd, str);
    }

    str = getenv("NR_LEVEL");
    if ((str != NULL) && (sscanf (str, "%i", &nm) == 1 && nm >= 0 && nm <= 30)) {
        nr_level = nm;
    }

    // Begin by setting up our usage environment:
    TaskScheduler* scheduler = BasicTaskScheduler::createNew();
    env = BasicUsageEnvironment::createNew(*scheduler);

    // If fifo doesn't exist, disable audio
    if (stat (inputAudioFileName, &stat_buffer) != 0) {
        *env << "Audio fifo does not exist, disabling audio.";
        audio = 0;
    }

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
        fprintf(stderr, "Failed to create RTSP server: %s\n", env->getResultMsg());
        exit(1);
    }

    StreamReplicator* replicator = NULL;
    if (audio == 1) {
        // Create and start the replicator that will be given to each subsession
        replicator = startReplicatorStream(inputAudioFileName, 8000, 1, 16, convertTo, nr_level);
    } else if (audio == 2) {
        // Create and start the replicator that will be given to each subsession
        replicator = startReplicatorStream(inputAudioFileName);
    }

    char const* descriptionString = "Session streamed by \"rRTSPServer\"";

    // First, make sure that the RTPSinks' buffers will be large enough to handle the huge size of DV frames (as big as 288000).
    OutPacketBuffer::maxSize = OUTPUT_BUFFER_SIZE_HIGH;

    // Set up each of the possible streams that can be served by the
    // RTSP server.  Each such stream is implemented using a
    // "ServerMediaSession" object, plus one or more
    // "ServerMediaSubsession" objects for each audio/video substream.

    // A H.264/5 video elementary stream:
    if ((resolution == RESOLUTION_HIGH) || (resolution == RESOLUTION_BOTH))
    {
        char const* streamName = "ch0_0.h264";
        char const* inputFileName = "/tmp/h264_high_fifo";

        ServerMediaSession* sms_high
            = ServerMediaSession::createNew(*env, streamName, streamName,
                                        descriptionString);
        if (codec_high == CODEC_H265) {
            sms_high->addSubsession(H265VideoFileServerMediaSubsession
                                            ::createNew(*env, inputFileName, reuseFirstSource));
        } else {
            sms_high->addSubsession(H264VideoFileServerMediaSubsession
                                            ::createNew(*env, inputFileName, reuseFirstSource));
        }
        if (audio == 1) {
            sms_high->addSubsession(WAVAudioFifoServerMediaSubsession
                                       ::createNew(*env, replicator, reuseFirstSource, 8000, 1, 16, convertTo));
        } else if (audio == 2) {
            sms_high->addSubsession(ADTSFromWAVAudioFifoServerMediaSubsession
                                       ::createNew(*env, replicator, reuseFirstSource));
        }
        if (back_channel == 1) {
            PCMAudioFileServerMediaSubsession_BC* smss_bc = PCMAudioFileServerMediaSubsession_BC
                    ::createNew(*env, outputAudioFileName, reuseFirstSource, 16000, 1, ALAW);
            sms_high->addSubsession(smss_bc);
        } else if (back_channel == 2) {
            PCMAudioFileServerMediaSubsession_BC* smss_bc = PCMAudioFileServerMediaSubsession_BC
                    ::createNew(*env, outputAudioFileName, reuseFirstSource, 16000, 1, ULAW);
            sms_high->addSubsession(smss_bc);
        } else if (back_channel == 4) {
            ADTSAudioFileServerMediaSubsession_BC* smss_bc = ADTSAudioFileServerMediaSubsession_BC
                    ::createNew(*env, outputAudioFileName, reuseFirstSource, 16000, 1);
            sms_high->addSubsession(smss_bc);
        }
        rtspServer->addServerMediaSession(sms_high);

        announceStream(rtspServer, sms_high, streamName, inputFileName, audio, codec_high);
    }

    // A H.264/5 video elementary stream:
    if ((resolution == RESOLUTION_LOW) || (resolution == RESOLUTION_BOTH))
    {
        char const* streamName = "ch0_1.h264";
        char const* inputFileName = "/tmp/h264_low_fifo";

        ServerMediaSession* sms_low
        = ServerMediaSession::createNew(*env, streamName, streamName,
                                        descriptionString);
        if (codec_low == CODEC_H265) {
            sms_low->addSubsession(H265VideoFileServerMediaSubsession
                                            ::createNew(*env, inputFileName, reuseFirstSource));
        } else {
            sms_low->addSubsession(H264VideoFileServerMediaSubsession
                                            ::createNew(*env, inputFileName, reuseFirstSource));
        }
        if (audio == 1) {
            sms_low->addSubsession(WAVAudioFifoServerMediaSubsession
                                        ::createNew(*env, replicator, reuseFirstSource, 8000, 1, 16, convertTo));
        } else if (audio == 2) {
            sms_low->addSubsession(ADTSFromWAVAudioFifoServerMediaSubsession
                                       ::createNew(*env, replicator, reuseFirstSource));
        }
        if (resolution == RESOLUTION_LOW) {
            if (back_channel == 1) {
                PCMAudioFileServerMediaSubsession_BC* smss_bc = PCMAudioFileServerMediaSubsession_BC
                        ::createNew(*env, outputAudioFileName, reuseFirstSource, 16000, 1, ALAW);
                sms_low->addSubsession(smss_bc);
            } else if (back_channel == 2) {
                PCMAudioFileServerMediaSubsession_BC* smss_bc = PCMAudioFileServerMediaSubsession_BC
                        ::createNew(*env, outputAudioFileName, reuseFirstSource, 16000, 1, ULAW);
                sms_low->addSubsession(smss_bc);
            } else if (back_channel == 4) {
                ADTSAudioFileServerMediaSubsession_BC* smss_bc = ADTSAudioFileServerMediaSubsession_BC
                        ::createNew(*env, outputAudioFileName, reuseFirstSource, 16000, 1);
                sms_low->addSubsession(smss_bc);
            }
        }
        rtspServer->addServerMediaSession(sms_low);

        announceStream(rtspServer, sms_low, streamName, inputFileName, audio, codec_low);
    }

    // A PCM audio elementary stream:
    if (audio != 0)
    {
        char const* streamName = "ch0_2.h264";

        ServerMediaSession* sms_audio
            = ServerMediaSession::createNew(*env, streamName, streamName,
                                              descriptionString);
        if (audio == 1) {
            sms_audio->addSubsession(WAVAudioFifoServerMediaSubsession
                                       ::createNew(*env, replicator, reuseFirstSource, 8000, 1, 16, convertTo));
        } else if (audio == 2) {
            sms_audio->addSubsession(ADTSFromWAVAudioFifoServerMediaSubsession
                                       ::createNew(*env, replicator, reuseFirstSource));
        }
        if (resolution == RESOLUTION_NONE) {
            if (back_channel == 1) {
                PCMAudioFileServerMediaSubsession_BC* smss_bc = PCMAudioFileServerMediaSubsession_BC
                        ::createNew(*env, outputAudioFileName, reuseFirstSource, 16000, 1, ALAW);
                sms_audio->addSubsession(smss_bc);
            } else if (back_channel == 2) {
                PCMAudioFileServerMediaSubsession_BC* smss_bc = PCMAudioFileServerMediaSubsession_BC
                        ::createNew(*env, outputAudioFileName, reuseFirstSource, 16000, 1, ULAW);
                sms_audio->addSubsession(smss_bc);
            } else if (back_channel == 4) {
                ADTSAudioFileServerMediaSubsession_BC* smss_bc = ADTSAudioFileServerMediaSubsession_BC
                        ::createNew(*env, outputAudioFileName, reuseFirstSource, 16000, 1);
                sms_audio->addSubsession(smss_bc);
            }
        }
        rtspServer->addServerMediaSession(sms_audio);

        announceStream(rtspServer, sms_audio, streamName, inputAudioFileName, audio, CODEC_NONE);
    }

    env->taskScheduler().doEventLoop(); // does not return

    return 0; // only to prevent compiler warning
}
