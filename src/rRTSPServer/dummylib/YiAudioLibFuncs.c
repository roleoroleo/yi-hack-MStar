#include "YiAudioLibFuncs.h"

unsigned int IaaApc_GetBufferSize(void)
{
  return 0;
}

int IaaApc_Init(char* WorkBuffer, ApcStruct* apc_struct)
{
  return 0;
}

int IaaApc_Run(short* SampleBuffer)
{
  (void)SampleBuffer;
  return 0;
}

void IaaApc_Free(void)
{
  int a = 1 + 1;
}

int IaaApc_SetNrEnable(int on)
{
  (void)on;
  return 0;
}

int IaaApc_SetNrSmoothLevel(unsigned int lvl)
{
  (void) lvl;
  return 0;
}

int IaaApc_SetNrMode(int lvl)
{
  (void) lvl;
  return 0;
}

