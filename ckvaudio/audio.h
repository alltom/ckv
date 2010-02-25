
#ifndef AUDIO_H
#define AUDIO_H

#include "../ckvm.h"

typedef struct CKVAudio *CKVAudio;
CKVAudio ckva_open(CKVM vm, int sample_rate, int channels);
int ckva_sample_rate(CKVAudio audio);
int ckva_channels(CKVAudio audio);
void ckva_fill_buffer(CKVAudio audio, double *outputBuffer, double *inputBuffer, int frames);
void ckva_destroy(CKVAudio audio);

#endif
