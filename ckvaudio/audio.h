
#ifndef AUDIO_H
#define AUDIO_H

#include "../ckvm.h"

/*

the audio module for ckv.
it does not handle interaction with the sound card, only the synthesis.

ckva_fill_buffer() invokes ckv until enough time has passed to fill
the buffer with audio. after running each ckv thread, it synthesizes
audio until audio time has caught up with ckv time.

*/

typedef struct _CKVAudio *CKVAudio;

/*
hard_clip: if non-zero, output samples are clipped at abs(hard_clip); otherwise, no clipping is performed
print_time: if true, prints the virtual time to stderr every simulated second
*/
CKVAudio ckva_open(CKVM vm, int sample_rate, int channels, double hard_clip, int print_time /* boolean */);
void ckva_destroy(CKVAudio audio);

int ckva_sample_rate(CKVAudio audio); /* returns the current sample rate */
int ckva_channels(CKVAudio audio); /* returns the number of output channels */

/* invokes ckv to simulate enough time to fill the buffer with audio */
void ckva_fill_buffer(CKVAudio audio, double *outputBuffer, double *inputBuffer, int frames);

#endif
