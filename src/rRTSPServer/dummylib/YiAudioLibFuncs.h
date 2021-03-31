#ifndef YI_AUDIO_LIB_FUNCS_H_
#define YI_AUDIO_LIB_FUNCS_H_

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
	unsigned int point_number;
	unsigned int channels;
	unsigned int rate;
} ApcStruct;

unsigned int IaaApc_GetBufferSize(void);
int IaaApc_Init(char* WorkBuffer, ApcStruct* apc_struct);
int IaaApc_Run(short* SampleBuffer);
void IaaApc_Free(void);
int IaaApc_SetNrEnable(int on);
int IaaApc_SetNrSmoothLevel(unsigned int lvl);
int IaaApc_SetNrMode(int lvl);

#ifdef __cplusplus
}
#endif

#endif
