#ifndef _WAV_SOURCE_HH
#define _WAV_SOURCE_HH

#ifndef _AUDIO_INPUT_DEVICE_HH
#include "AudioInputDevice.hh"
#endif

#include <pthread.h>
#include <vector>
#include <queue> 
#include <stdint.h>

typedef enum {
  WA_PCM = 0x01,
  WA_PCMA = 0x06,
  WA_PCMU = 0x07,
  WA_IMA_ADPCM = 0x11,
  WA_UNKNOWN
} WAV_AUDIO_FORMAT;

#define ALSA_SAMPLES 320 // Equals 20ms at 16000Hz

class WAVSource : public AudioInputDevice
{
public:
  static EventTriggerId s_frameReceivedTrigger;
  static WAVSource *createNew(UsageEnvironment &env);

  static unsigned char getAudioFormat();
  static unsigned int bitsPerSample();
  static unsigned int samplingFrequency();
  static unsigned int numChannels();

  void start()
  {
      pthread_create(&m_tid, 0, helper, this);
  }

  void join()
  {
      pthread_join(m_tid, 0);
  }

protected:
  WAVSource(UsageEnvironment &env);
  // called only by createNew()

  virtual ~WAVSource();
  bool OpenSource();
  bool CloseSource();

private:
  // redefined virtual functions:
  virtual void doGetNextFrame();
  virtual Boolean setInputPort(int portIndex);
  virtual double getAverageLevel() const;

  static void* helper(void* arg)
  {
      WAVSource* mt = reinterpret_cast<WAVSource*>(arg);
      mt->readThread();
      return 0;
  }

  void readThread();

protected:

private:
  void deliverFrame();
  static void deliverFrame0(void *clientData);
  double fPlayTimePerSample; // useconds
  double fLastPlayTime;
  static unsigned char audioformat;
  static unsigned int bitspersample;
  static unsigned int frequency;
  static unsigned int channels;
  static unsigned int fPreferredFrameSize;
  static unsigned int s_readError;
  FILE* fFifoHandle;
  pthread_t m_tid;
  Boolean mThreadrun;
  static unsigned referenceCount;
};

#endif
