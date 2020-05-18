/*
 * Copyright (c) 2020 roleo.
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
 * Dump h264 content from rmm memory and copy it to
 * a circular buffer.
 * Then send the circular buffer to live555.
 */

#include "liveMedia.hh"
#include "BasicUsageEnvironment.hh"

#include "H264VideoCBMemoryServerMediaSubsession.hh"
#include "WAVAudioFifoServerMediaSubsession.hh"

#include <getopt.h>
#include <pthread.h>
#include <fcntl.h>
#include <dirent.h>
#include <errno.h>
#include <limits.h>
#include <sys/stat.h>

#include "rRTSPServer.h"

unsigned char SPS[]    = { 0x00, 0x00, 0x00, 0x01, 0x67 };
unsigned char SEI_F0[] = { 0x00, 0x00, 0x00, 0x01, 0x06, 0xF0 };

int debug;                                  /* Set to 1 to debug this .c */
int resolution;
int audio;
int port;
int nr_level;

cb_output_buffer output_buffer_low;
cb_output_buffer output_buffer_high;

UsageEnvironment* env;

// To make the second and subsequent client for each stream reuse the same
// input stream as the first client (rather than playing the file from the
// start for each client), change the following "False" to "True":
Boolean reuseFirstSource = True;

// To stream *only* MPEG-1 or 2 video "I" frames
// (e.g., to reduce network bandwidth),
// change the following "False" to "True":
Boolean iFramesOnly = False;

void cb_dest_memcpy(cb_output_buffer *dest, unsigned char *src, size_t n)
{
    unsigned char *uc_dest = dest->write_index;

    if (uc_dest + n > dest->buffer + dest->size) {
        memcpy(uc_dest, src, dest->buffer + dest->size - uc_dest);
        memcpy(dest->buffer, src + (dest->buffer + dest->size - uc_dest), n - (dest->buffer + dest->size - uc_dest));
        dest->write_index = n + uc_dest - dest->size;
    } else {
        memcpy(uc_dest, src, n);
        dest->write_index += n;
    }
    if (dest->write_index == dest->buffer + dest->size) {
        dest->write_index = dest->buffer;
    }
}

// Returns the 1st process id corresponding to pname
int pidof(const char *pname)
{
  DIR *dirp;
  FILE *fp;
  struct dirent *entry;
  char path[1024], read_buf[1024];
  int ret = 0;

  dirp = opendir ("/proc/");
  if (dirp == NULL) {
    fprintf(stderr, "error opening /proc");
    return 0;
  }

  while ((entry = readdir (dirp)) != NULL) {
    if (atoi(entry->d_name) > 0) {
      sprintf(path, "/proc/%s/comm", entry->d_name);

      /* A file may not exist, Ait may have been removed.
       * dut to termination of the process. Actually we need to
       * make sure the error is actually file does not exist to
       * be accurate.
       */
      fp = fopen (path, "r");
      if (fp != NULL) {
        fscanf (fp, "%s", read_buf);
        if (strcmp (read_buf, pname) == 0) {
            ret = atoi(entry->d_name);
            fclose (fp);
            break;
        }
        fclose (fp);
      }
    }
  }

  closedir (dirp);
  return ret;
}

// Converts virtual address to physical address
unsigned int rmm_virt2phys(unsigned int inAddr) {
    int pid;
    unsigned int outAddr;
    char sInAddr[16];
    char sMaps[1024];
    FILE *fMaps;
    char *p;
    char *line;
    size_t lineSize;

    line = (char  *) malloc(1024);

    pid = pidof("rmm");
    sprintf(sMaps, "/proc/%d/maps", pid);
    fMaps = fopen(sMaps, "r");
    sprintf(sInAddr, "%08x", inAddr);
    while (getline(&line, &lineSize, fMaps) != -1) {
        if (strncmp(line, sInAddr, 8) == 0)
            break;
    }

    p = line;
    p = strchr(p, ' ');
    p++;
    p = strchr(p, ' ');
    p++;
    p[8] = '\0';
    sscanf(p, "%x", &outAddr);
    free(line);
    fclose(fMaps);

    return outAddr;
}

long long current_timestamp() {
    struct timeval te; 
    gettimeofday(&te, NULL); // get current time
    long long milliseconds = te.tv_sec*1000LL + te.tv_usec/1000; // calculate milliseconds

    return milliseconds;
}

void *capture(void *ptr)
{
    int remove_sf0 = 1;
    const char memDevice[] = "/dev/mem";
    FILE *fPtr, *fLen, *fTime;
    int fMem;
    unsigned int ivAddr, ipAddr;
    unsigned int size;
    unsigned char *addr;
    char filLenFile[1024];
    char timeStampFile[1024];
    unsigned char buffer[131072];
    int len;
    unsigned int time, oldTime = 0;
    int stream_started = 0;
    cb_output_buffer *cb_current;
    int res;

    res = *((int *) ptr);
    if (res == RESOLUTION_LOW) {
        fPtr = fopen("/proc/mstar/OMX/VMFE1/ENCODER_INFO/OBUF_pBuffer", "r");
        fLen = fopen("/proc/mstar/OMX/VMFE1/ENCODER_INFO/OBUF_nAllocLen", "r");
    } else {
        fPtr = fopen("/proc/mstar/OMX/VMFE0/ENCODER_INFO/OBUF_pBuffer", "r");
        fLen = fopen("/proc/mstar/OMX/VMFE0/ENCODER_INFO/OBUF_nAllocLen", "r");
    }
    fscanf(fPtr, "%x", &ivAddr);
    fclose(fPtr);
    fscanf(fLen, "%d", &size);
    fclose(fLen);

    ipAddr = rmm_virt2phys(ivAddr);

    if (debug) fprintf(stderr, "vaddr: 0x%08x - paddr: 0x%08x - size: %u\n", ivAddr, ipAddr, size);

    // open /dev/mem and error checking
    fMem = open(memDevice, O_RDONLY); // | O_SYNC);
    if (fMem < 0) {
        fprintf(stderr, "Failed to open the /dev/mem\n");
        exit (EXIT_FAILURE);
    }

    // mmap() the opened /dev/mem
    addr = (unsigned char *) (mmap(NULL, size, PROT_READ, MAP_SHARED, fMem, ipAddr));
    if (addr == MAP_FAILED) {
        fprintf(stderr, "Failed to map memory\n");
        exit (EXIT_FAILURE);
    }

    // close the character device
    close(fMem);

    if (res == RESOLUTION_LOW) {
        sprintf(filLenFile, "/proc/mstar/OMX/VMFE1/ENCODER_INFO/OBUF_nFilledLen");
        sprintf(timeStampFile, "/proc/mstar/OMX/VMFE1/ENCODER_INFO/OBUF_nTimeStamp");
    } else {
        sprintf(filLenFile, "/proc/mstar/OMX/VMFE0/ENCODER_INFO/OBUF_nFilledLen");
        sprintf(timeStampFile, "/proc/mstar/OMX/VMFE0/ENCODER_INFO/OBUF_nTimeStamp");
    }

    if (res == RESOLUTION_LOW) {
        cb_current = &output_buffer_low;
    } else {
        cb_current = &output_buffer_high;
    }

    while (1) {
        fTime = fopen(timeStampFile, "r");
        fscanf(fTime, "%u", &time);
        fclose(fTime);
        oldTime = time;

        stream_started = 0;

        while(!stream_started) {
            fTime = fopen(timeStampFile, "r");
            fscanf(fTime, "%u", &time);
            fclose(fTime);

            if (time == oldTime) {
                usleep(8000);
                continue;
            }

            usleep(1000);

            fLen = fopen(filLenFile, "r");
            fscanf(fLen, "%d", &len);
            fclose(fLen);

            memcpy(buffer, addr, len);
            oldTime = time;
            if (memcmp(SPS, buffer, sizeof(SPS)) == 0) {
                if (debug) fprintf(stderr, "SPS - time: %u - len: %d\n", time, len);
                if ((unsigned) len > cb_current->size) {
                    fprintf(stderr, "Frame size exceeds buffer size\n");
                }
                if (remove_sf0) {
                    pthread_mutex_lock(&(cb_current->mutex));
                    cb_dest_memcpy(cb_current, buffer, 24);
                    cb_dest_memcpy(cb_current, buffer + 76, len - 76);
                    pthread_mutex_unlock(&(cb_current->mutex));
                } else {
                    pthread_mutex_lock(&(cb_current->mutex));
                    cb_dest_memcpy(cb_current, buffer, len);
                    pthread_mutex_unlock(&(cb_current->mutex));
                }
                stream_started = 1;
            }
        }

        while(1) {
            fTime = fopen(timeStampFile, "r");
            fscanf(fTime, "%u", &time);
            fclose(fTime);
//            if (debug) fprintf(stderr, "time: %u\n", time);

            if (time == oldTime) {
                usleep(8000);
                continue;
            } else if (time - oldTime > 125000 ) {
                // If time - oldTime > 125000 (125 ms) assume sync lost
                if (debug) fprintf(stderr, "sync lost: %u - %u\n", time, oldTime);
                break;
            }

            usleep(1000);

            fLen = fopen(filLenFile, "r");
            fscanf(fLen, "%d", &len);
            fclose(fLen);
            if (debug) fprintf(stderr, "time: %u - len: %d - milliseconds: %lld\n", time, len, current_timestamp());

            memcpy(buffer, addr, len);
            oldTime = time;

            if (remove_sf0) {
                if (memcmp(SEI_F0, buffer, sizeof(SEI_F0)) == 0) {
                    pthread_mutex_lock(&(cb_current->mutex));
                    cb_dest_memcpy(cb_current, buffer + 52, len - 52);
                    pthread_mutex_unlock(&(cb_current->mutex));
                } else if (memcmp(SPS, buffer, sizeof(SPS)) == 0) {
                    pthread_mutex_lock(&(cb_current->mutex));
                    cb_dest_memcpy(cb_current, buffer, 24);
                    cb_dest_memcpy(cb_current, buffer + 76, len - 76);
                    pthread_mutex_unlock(&(cb_current->mutex));
                } else {
                    pthread_mutex_lock(&(cb_current->mutex));
                    cb_dest_memcpy(cb_current, buffer, len);
                    pthread_mutex_unlock(&(cb_current->mutex));
                }
            } else {
                pthread_mutex_lock(&(cb_current->mutex));
                cb_dest_memcpy(cb_current, buffer, len);
                pthread_mutex_unlock(&(cb_current->mutex));
            }
        }
    }

    munmap(addr, size);
}

static void announceStream(RTSPServer* rtspServer, ServerMediaSession* sms, char const* streamName, int audio)
{
    char* url = rtspServer->rtspURL(sms);
    UsageEnvironment& env = rtspServer->envir();
    env << "\n\"" << streamName << "\" stream, from memory\n";
    if (audio)
        env << "Audio enabled\n";
    env << "Play this stream using the URL \"" << url << "\"\n";
    delete[] url;
}

void print_usage(char *progname)
{
    fprintf(stderr, "\nUsage: %s [-r RES] [-p PORT] [-d]\n\n", progname);
    fprintf(stderr, "\t-r RES, --resolution RES\n");
    fprintf(stderr, "\t\tset resolution: low, high or both (default high)\n");
    fprintf(stderr, "\t-a RES, --audio RES\n");
    fprintf(stderr, "\t\tenable/disable audio for specific resolution: low, high or none (default none)\n");
    fprintf(stderr, "\t-p PORT, --port PORT\n");
    fprintf(stderr, "\t\tset TCP port (default 554)\n");
    fprintf(stderr, "\t-n LEVEL, --noisereduction LEVEL\n");
    fprintf(stderr, "\t\tset a noise reduction level from 0-30: 0 = disabled, 30 = maximum noise reduction (default 0)\n");
    fprintf(stderr, "\t-d,     --debug\n");
    fprintf(stderr, "\t\tenable debug\n");
    fprintf(stderr, "\t-h,     --help\n");
    fprintf(stderr, "\t\tprint this help\n");
}

int main(int argc, char** argv)
{
    char *str;
    int nm;
    char user[65];
    char pwd[65];
    int pth_ret;
    int c;
    char *endptr;

    pthread_t capture_thread_low, capture_thread_high;
    int res_low = RESOLUTION_LOW;
    int res_high = RESOLUTION_HIGH;

    Boolean convertToULaw = True;
    char const* inputAudioFileName = "/tmp/audio_fifo";
    struct stat stat_buffer;

    // Setting default
    resolution = RESOLUTION_HIGH;
    audio = RESOLUTION_NONE;
    port = 554;
    debug = 0;
    nr_level = 0;

    while (1) {
        static struct option long_options[] =
        {
            {"resolution",  required_argument, 0, 'r'},
            {"audio",  required_argument, 0, 'a'},
            {"port",  required_argument, 0, 'p'},
            {"noisereduction", required_argument, 0, 'n'},
            {"debug",  no_argument, 0, 'd'},
            {"help",  no_argument, 0, 'h'},
            {0, 0, 0, 0}
        };
        /* getopt_long stores the option index here. */
        int option_index = 0;

        c = getopt_long (argc, argv, "r:a:p:n:dh",
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

        case 'a':
            if (strcasecmp("low", optarg) == 0) {
                audio = RESOLUTION_LOW;
            } else if (strcasecmp("high", optarg) == 0) {
                audio = RESOLUTION_HIGH;
            } else if (strcasecmp("none", optarg) == 0) {
                audio = RESOLUTION_NONE;
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
            if ((errno == ERANGE && (nr_level > 30 || nr_level < 0)) || (errno != 0 && nr_level == 0)) {
                print_usage(argv[0]);
                exit(EXIT_FAILURE);
            }
            if (endptr == optarg) {
                print_usage(argv[0]);
                exit(EXIT_FAILURE);
            }
            break;

        case 'd':
            fprintf (stderr, "debug on\n");
            debug = 1;
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

    str = getenv("RRTSP_AUDIO");
    if (str != NULL) {
        if (strcasecmp("low", str) == 0) {
            audio = RESOLUTION_LOW;
        } else if (strcasecmp("high", str) == 0) {
            audio = RESOLUTION_HIGH;
        } else if (strcasecmp("none", str) == 0) {
            audio = RESOLUTION_NONE;
        }
    }

    str = getenv("RRTSP_PORT");
    if ((str != NULL) && (sscanf (str, "%i", &nm) == 1) && (nm >= 0)) {
        port = nm;
    }

    str = getenv("RRTSP_DEBUG");
    if ((str != NULL) && (sscanf (str, "%i", &nm) == 1) && (nm == 1)) {
        debug = nm;
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

    // If fifo doesn't exist, disable audio
    if (stat (inputAudioFileName, &stat_buffer) != 0) {
        audio = RESOLUTION_NONE;
    }

    if ((resolution == RESOLUTION_LOW) || (resolution == RESOLUTION_BOTH)) {
        // Fill output buffer struct
        output_buffer_low.resolution = RESOLUTION_LOW;
        output_buffer_low.size = OUTPUT_BUFFER_SIZE_LOW;
        output_buffer_low.buffer = (unsigned char *) malloc(OUTPUT_BUFFER_SIZE_LOW * sizeof(unsigned char));
        output_buffer_low.read_index = output_buffer_low.buffer;
        output_buffer_low.write_index = output_buffer_low.buffer;

        if (output_buffer_low.buffer == NULL) {
            fprintf(stderr, "could not alloc memory\n");
            exit(EXIT_FAILURE);
        }

        // Start capture threads
        if (pthread_mutex_init(&(output_buffer_low.mutex), NULL) != 0) { 
            *env << "Failed to create mutex\n";
            exit(EXIT_FAILURE);
        }
        pth_ret = pthread_create(&capture_thread_low, NULL, capture, (void*) &res_low);
        if (pth_ret != 0) {
            *env << "Failed to create capture thread for low resolution\n";
            exit(EXIT_FAILURE);
        }
        pthread_detach(capture_thread_low);
    }

    if ((resolution == RESOLUTION_HIGH) || (resolution == RESOLUTION_BOTH)) {
        // Fill output buffer struct
        output_buffer_high.resolution = RESOLUTION_HIGH;
        output_buffer_high.size = OUTPUT_BUFFER_SIZE_HIGH;
        output_buffer_high.buffer = (unsigned char *) malloc(OUTPUT_BUFFER_SIZE_HIGH * sizeof(unsigned char));
        output_buffer_high.read_index = output_buffer_high.buffer;
        output_buffer_high.write_index = output_buffer_high.buffer;

        if (output_buffer_high.buffer == NULL) {
            fprintf(stderr, "could not alloc memory\n");
            exit(EXIT_FAILURE);
        }

        if (pthread_mutex_init(&(output_buffer_high.mutex), NULL) != 0) { 
            *env << "Failed to create mutex\n";
            exit(EXIT_FAILURE);
        }
        pth_ret = pthread_create(&capture_thread_high, NULL, capture, (void*) &res_high);
        if (pth_ret != 0) {
            *env << "Failed to create capture thread for high resolution\n";
            exit(EXIT_FAILURE);
        }
        pthread_detach(capture_thread_high);
    }

    sleep(2);

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

    char const* descriptionString = "Session streamed by \"rRTSPServer\"";

    // Set up each of the possible streams that can be served by the
    // RTSP server.  Each such stream is implemented using a
    // "ServerMediaSession" object, plus one or more
    // "ServerMediaSubsession" objects for each audio/video substream.

    // A H.264 video elementary stream:
    if ((resolution == RESOLUTION_HIGH) || (resolution == RESOLUTION_BOTH))
    {
        char const* streamName = "ch0_0.h264";

        // First, make sure that the RTPSinks' buffers will be large enough to handle the huge size of DV frames (as big as 288000).
        OutPacketBuffer::maxSize = 300000;

        ServerMediaSession* sms_high
            = ServerMediaSession::createNew(*env, streamName, streamName,
                                              descriptionString);
        sms_high->addSubsession(H264VideoCBMemoryServerMediaSubsession
                                   ::createNew(*env, &output_buffer_high, reuseFirstSource));
        if (audio == RESOLUTION_HIGH) {
            sms_high->addSubsession(WAVAudioFifoServerMediaSubsession
                                   ::createNew(*env, inputAudioFileName, reuseFirstSource, convertToULaw, nr_level));
        }
        rtspServer->addServerMediaSession(sms_high);

        announceStream(rtspServer, sms_high, streamName, audio == RESOLUTION_HIGH);
    }

    // A H.264 video elementary stream:
    if ((resolution == RESOLUTION_LOW) || (resolution == RESOLUTION_BOTH))
    {
        char const* streamName = "ch0_1.h264";

        // First, make sure that the RTPSinks' buffers will be large enough to handle the huge size of DV frames (as big as 288000).
        OutPacketBuffer::maxSize = 300000;

        ServerMediaSession* sms_low
            = ServerMediaSession::createNew(*env, streamName, streamName,
                                              descriptionString);
        sms_low->addSubsession(H264VideoCBMemoryServerMediaSubsession
                                   ::createNew(*env, &output_buffer_low, reuseFirstSource));
        if (audio == RESOLUTION_LOW) {
            sms_low->addSubsession(WAVAudioFifoServerMediaSubsession
                                   ::createNew(*env, inputAudioFileName, reuseFirstSource, convertToULaw, nr_level));
        }
        rtspServer->addServerMediaSession(sms_low);

        announceStream(rtspServer, sms_low, streamName, audio == RESOLUTION_LOW);
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

    if ((resolution == RESOLUTION_LOW) || (resolution == RESOLUTION_BOTH)) {
        pthread_mutex_destroy(&(output_buffer_low.mutex));
    } else if ((resolution == RESOLUTION_HIGH) || (resolution == RESOLUTION_BOTH)) {
        pthread_mutex_destroy(&(output_buffer_high.mutex));
    }

    // Free buffers
    if ((resolution == RESOLUTION_LOW) || (resolution == RESOLUTION_BOTH)) {
        free(output_buffer_low.buffer);
    }

    if ((resolution == RESOLUTION_HIGH) || (resolution == RESOLUTION_BOTH)) {
        free(output_buffer_high.buffer);
    }

    return 0; // only to prevent compiler warning
}
