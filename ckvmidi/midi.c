
#include "midi.h"

#include <stdlib.h>

struct CKVMIDI {
	CKVM vm;
};

CKVMIDI
ckvmidi_open(CKVM vm)
{
	CKVMIDI midi = (CKVMIDI) malloc(sizeof(struct CKVMIDI));
	return midi;
}

void
ckvmidi_dispatch_note_on(int channel, int note, float velocity)
{
}

void
ckvmidi_dispatch_key_pressure(int channel, int note, float velocity)
{
}

void
ckvmidi_dispatch_note_off(int channel, int note)
{
}

void
ckvmidi_dispatch_control(int controller, float value)
{
}

void
ckvmidi_dispatch_channel_pressure(float value)
{
}

void
ckvmidi_dispatch_pitch_bend(float value)
{
}
