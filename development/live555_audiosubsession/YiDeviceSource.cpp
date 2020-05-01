#include "DeviceSource.hh"
#include <GroupsockHelper.hh> // for "gettimeofday()"
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>

#define BUF_SIZE 1024
static uint8_t buf[BUF_SIZE];
int audio_stream;

DeviceSource*
DeviceSource::createNew(UsageEnvironment& env, DeviceParameters params)
{
  return new DeviceSource(env, params);
}

EventTriggerId DeviceSource::eventTriggerId = 0;

unsigned DeviceSource::referenceCount = 0;

DeviceSource::DeviceSource(UsageEnvironment& env, DeviceParameters params) : FramedSource(env) , fParams(params)
{ 
  if (referenceCount == 0) 
  {
      audio_stream = open("/tmp/audio_fifo", O_RDONLY);
  }
  ++referenceCount;

  if (eventTriggerId == 0) 
  {
    eventTriggerId = envir().taskScheduler().createEventTrigger(deliverFrame0);
  }
}

DeviceSource::~DeviceSource(void) {
  --referenceCount;
  envir().taskScheduler().deleteEventTrigger(eventTriggerId);
  eventTriggerId = 0;

  if (referenceCount == 0) 
  {
  }
}

int loop_count;

void DeviceSource::doGetNextFrame() 
{
    int res;
    res = read(audio_stream, buf, BUF_SIZE);
    if (res < 0 || res != BUF_SIZE)
    {
        printf("Read result: %d\n", res);
    }
    else
    {
        deliverFrame();
    }
}

void DeviceSource::deliverFrame0(void* clientData) 
{
  ((DeviceSource*)clientData)->deliverFrame();
}

void DeviceSource::deliverFrame() 
{
  if (!isCurrentlyAwaitingData()) return; // we're not ready for the data yet

  u_int8_t* newFrameDataStart = (u_int8_t*) buf;
  unsigned newFrameSize = BUF_SIZE;

  // Deliver the data here:
  if (newFrameSize > fMaxSize) {
    fFrameSize = fMaxSize;
    fNumTruncatedBytes = newFrameSize - fMaxSize;
  } else {
    fFrameSize = newFrameSize;
  }
  gettimeofday(&fPresentationTime, NULL); 
  memmove(fTo, newFrameDataStart, fFrameSize);
  FramedSource::afterGetting(this);
}

