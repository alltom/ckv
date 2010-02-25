
#ifndef MIDI_H
#define MIDI_H

#include "../ckvm.h"

typedef struct CKVMIDI *CKVMIDI;

CKVMIDI ckvmidi_open(CKVM vm);
void ckvmidi_dispatch_note_on(int channel, int note, float velocity);
void ckvmidi_dispatch_key_pressure(int channel, int note, float velocity);
void ckvmidi_dispatch_note_off(int channel, int note);
void ckvmidi_dispatch_control(int controller, float value);
void ckvmidi_dispatch_channel_pressure(float value);
void ckvmidi_dispatch_pitch_bend(float value);

#endif
