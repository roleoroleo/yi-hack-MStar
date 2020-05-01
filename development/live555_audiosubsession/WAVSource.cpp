#include <GroupsockHelper.hh>
#include <FramedSource.hh>
#include <iostream>

#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>

#include "WAVSource.hh"
#include <sched.h>

// Audio device setting hardcoded for testing
unsigned char WAVSource::audioformat = WA_PCM;
unsigned int WAVSource::frequency = 16000;
unsigned int WAVSource::channels = 1;
unsigned int WAVSource::bitspersample = 16;
unsigned WAVSource::fPreferredFrameSize;

#define AUDIO_FIFO "/tmp/audio_fifo"
#define QueueCapacity 10

using namespace std;

class AudioFrameCls
{
public:
        uint8_t *Frame;
        uint32_t FrameSize;
        struct timeval FramePTime;

public:
        AudioFrameCls()
        {
                Frame = NULL;
                FrameSize = 0;
                gettimeofday(&FramePTime, NULL);
        }

        ~AudioFrameCls()
        {
                if (Frame != NULL)
                {
                        delete Frame;
                        Frame = NULL;
                }
        }
};

EventTriggerId WAVSource::s_frameReceivedTrigger = 0;
WAVSource *_L_AudioFrameSource = NULL;
/* Frame Queue */
pthread_mutex_t RWQueueMutex;
std::queue<AudioFrameCls *> AudioFrameQueue;
unsigned WAVSource::referenceCount = 0;

unsigned char WAVSource::getAudioFormat()
{
        return audioformat;
}

unsigned int WAVSource::bitsPerSample()
{
        return bitspersample;
}

unsigned int WAVSource::samplingFrequency()
{
        return frequency;
}
unsigned int WAVSource::numChannels()
{
        return channels;
}

WAVSource *WAVSource::createNew(UsageEnvironment &env)
{
        return new WAVSource(env);
}

WAVSource::WAVSource(UsageEnvironment &env) : AudioInputDevice(env, 0, 0, 0, 0)
{
        printf("audioformat:%d,channels:%d,bitspersample:%d,frequency:%d\n", audioformat, channels, bitspersample, frequency);

        fSamplingFrequency = frequency;
        fBitsPerSample = bitspersample;
        fNumChannels = channels;      
        fPlayTimePerSample = 1e6 / (double)frequency;
        fPreferredFrameSize = (ALSA_SAMPLES * 1 * 16) / 8;
        printf("fPreferredFrameSize:%d\n", fPreferredFrameSize);
        
        if (s_frameReceivedTrigger == 0)
        {
                s_frameReceivedTrigger = envir().taskScheduler().createEventTrigger(deliverFrame0);
        }

        if (referenceCount == 0) 
        {
                fPresentationTime.tv_sec = 0;
                fPresentationTime.tv_usec = 0;
                _L_AudioFrameSource = static_cast<WAVSource *>(this);
                if (OpenSource() ==  false)
                {
                        printf("Source Open Failed\r\n");
                }
                pthread_mutex_init(&RWQueueMutex, NULL);
                mThreadrun = True;
                start();
        }
        referenceCount++;
}

WAVSource::~WAVSource()
{
        printf("~WAVSource ref %d\n", referenceCount);
        referenceCount--;
        if (referenceCount == 0)
        {
                envir().taskScheduler().deleteEventTrigger(s_frameReceivedTrigger);
                s_frameReceivedTrigger = 0;
                pthread_mutex_destroy(&RWQueueMutex);

                while (!AudioFrameQueue.empty())
                {
                        AudioFrameCls *AudioFrame = AudioFrameQueue.front();
                        delete AudioFrame;
                        AudioFrameQueue.pop();
                }
                mThreadrun = False;
                join();
        }        
}

void WAVSource::doGetNextFrame()
{
        //printf("dGNFrm\n");
        deliverFrame();
}

void WAVSource::deliverFrame0(void *clientData)
{
        ((WAVSource *)clientData)->deliverFrame();
}

bool WAVSource::OpenSource()
{
        fFifoHandle = fopen(AUDIO_FIFO, "rb");
        return fFifoHandle != NULL;
}

bool WAVSource::CloseSource()
{
        if (fFifoHandle != NULL)
        {
                fclose(fFifoHandle);
                return true;
        }

        return false;
}

void WAVSource::deliverFrame()
{
        if (!isCurrentlyAwaitingData())
                return; // we're not ready for the data yet

        // Deliver the frame
        pthread_mutex_lock(&RWQueueMutex);

        if (!AudioFrameQueue.empty())
        {
                AudioFrameCls *AudioFrame = AudioFrameQueue.front();

                if (AudioFrame->FrameSize > fMaxSize)
                {
                        fFrameSize = fMaxSize;
                        fNumTruncatedBytes = AudioFrame->FrameSize - fMaxSize;
                }
                else
                {
                        fFrameSize = AudioFrame->FrameSize;
                }
#if 1
                fPresentationTime.tv_sec = AudioFrame->FramePTime.tv_sec;
                fPresentationTime.tv_usec = AudioFrame->FramePTime.tv_usec;
#else
                double fPlayTimePerSample = 1e6/(double)fSamplingFrequency;
                unsigned bytesPerSample = (fNumChannels*fBitsPerSample)/8;
                fFrameSize = ALSA_SAMPLES * bytesPerSample;
                fNumTruncatedBytes = 0;
                fDurationInMicroseconds = fLastPlayTime = (unsigned)((fPlayTimePerSample*fFrameSize) / bytesPerSample);

                // Set the 'presentation time' and 'duration' of this frame:
                if (fPresentationTime.tv_sec == 0 && fPresentationTime.tv_usec == 0) {
                     // This is the first frame, so use the current time:
                        gettimeofday(&fPresentationTime, NULL);
                } else {
                        // Increment by the play time of the previous data:
                        unsigned uSeconds = fPresentationTime.tv_usec + fDurationInMicroseconds;
                        
                        fPresentationTime.tv_sec += uSeconds/1000000;
                        fPresentationTime.tv_usec = uSeconds%1000000;

                        printf("sec: %d, usec: %d, %u\n", fPresentationTime.tv_sec, fPresentationTime.tv_usec, uSeconds);
                }
#endif
		//printf("Audio presentation sec: %d.%d\n", fPresentationTime.tv_sec, fPresentationTime.tv_usec);

		memmove(fTo, AudioFrame->Frame, fFrameSize);
                delete AudioFrame;
                AudioFrameQueue.pop();
        }
        else
        {
                fFrameSize = 0;
        }

        pthread_mutex_unlock(&RWQueueMutex);

        // After delivering the data, inform the reader that it is now available:
        if (fFrameSize > 0)
        {
                FramedSource::afterGetting(this);
        }
}

Boolean WAVSource::setInputPort(int portIndex)
{
        return True;
}

double WAVSource::getAverageLevel() const
{
        return 0.0;
}

void WAVSource::readThread()
{
        int res;
	struct sched_param params;
	pthread_t this_thread = pthread_self();
	params.sched_priority = sched_get_priority_max(SCHED_FIFO);
	res = pthread_setschedparam(this_thread, SCHED_FIFO, &params);
	if (res != 0) {
         std::cout << "Unsuccessful in setting thread realtime prio" << std::endl;
         return;     
     	}

        std::cout << "Buffer thread is running" << std::endl;
        std::cout << "Start reading.." << std::endl;
        do
        {
                pthread_mutex_lock(&RWQueueMutex);
                while (AudioFrameQueue.size() >= QueueCapacity)
                {
                        //std::cout << "dump" << std::endl;
                        AudioFrameCls *AudioFrame = AudioFrameQueue.front();
                        delete AudioFrame;
                        AudioFrameQueue.pop();
                }
                pthread_mutex_unlock(&RWQueueMutex);

                AudioFrameCls *AudioFrame = new AudioFrameCls();
                AudioFrame->FrameSize = ALSA_SAMPLES * (fBitsPerSample / 8);
                AudioFrame->Frame = (u_int8_t *)malloc(AudioFrame->FrameSize);
                res = fread(AudioFrame->Frame, (fBitsPerSample / 8), ALSA_SAMPLES, fFifoHandle);
                //std::cout << "rd:" << res << " fse:" << AudioFrameQueue.size() << std::endl;

                //memcpy(AudioFrame->Frame,Frame,AudioFrame->FrameSize);
                pthread_mutex_lock(&RWQueueMutex);
                AudioFrameQueue.push(AudioFrame);
                pthread_mutex_unlock(&RWQueueMutex);

                envir().taskScheduler().triggerEvent(s_frameReceivedTrigger, _L_AudioFrameSource);
        } while (mThreadrun);
}
