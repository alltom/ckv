
#ifndef MIDI_H
#define MIDI_H

#include "../ckvm.h"

typedef struct _CKVMIDI *CKVMIDI;

CKVMIDI ckvmidi_open(CKVM vm);
void ckvmidi_dispatch_note_on(CKVMIDI midi, int channel, int note, float velocity);
void ckvmidi_dispatch_key_pressure(CKVMIDI midi, int channel, int note, float velocity);
void ckvmidi_dispatch_note_off(CKVMIDI midi, int channel, int note);
void ckvmidi_dispatch_control(CKVMIDI midi, int controller, float value);
void ckvmidi_dispatch_channel_pressure(CKVMIDI midi, float value);
void ckvmidi_dispatch_pitch_bend(CKVMIDI midi, float value);

#endif
