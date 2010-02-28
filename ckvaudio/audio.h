
#ifndef AUDIO_H
#define AUDIO_H

#include "../ckvm.h"

typedef struct _CKVAudio *CKVAudio;

CKVAudio ckva_open(CKVM vm, int sample_rate, int channels, double hard_clip, int print_time /* boolean */);
int ckva_sample_rate(CKVAudio audio);
int ckva_channels(CKVAudio audio);
void ckva_fill_buffer(CKVAudio audio, double *outputBuffer, double *inputBuffer, int frames);
void ckva_destroy(CKVAudio audio);

#endif
