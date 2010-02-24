extern "C" {
#include "ckv.h"
}

#include "rtmidi/RtMidi.h"

static RtMidiIn midi;

#define MIDI_NOTE_ON (9)
#define MIDI_NOTE_OFF (8)

extern "C" {

/* returns 0 on failure */
int
start_midi()
{
	if(midi.getPortCount() < 1) {
		std::cout << "No MIDI ports available!\n";
		return 0;
	}
	
	midi.openPort(0);
	
	// don't ignore sysex, timing, or active sensing messages
	midi.ignoreTypes(false, false, false);
	
	return 1;
}

/* returns 0 if no new messages, otherwise populates message */
int
get_midi_message(MidiMsg *message)
{
	std::vector<unsigned char> rtmsg;
	
	while(true) {
		midi.getMessage(&rtmsg);
		if(rtmsg.size() == 0)
			return 0;
	
		int type = (rtmsg[0] & 0xf0) >> 4;
	
		if(type == MIDI_NOTE_OFF || (type == MIDI_NOTE_ON && rtmsg[2] == 0)) {
			message->velocity = 0;
		} else if(type == MIDI_NOTE_ON) {
			message->velocity = rtmsg[2] / 127.0;
		} else {
			continue; // unrecognized
		}
		
		message->channel = rtmsg[0] & 0x0f;
		message->note = rtmsg[1];
		
		return 1;
	}
}

} // extern "C"
