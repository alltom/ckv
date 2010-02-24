extern "C" {
#include "ckv.h"
}

#include "rtmidi/RtMidi.h"

static RtMidiIn midi;

#define MIDI_NOTE_ON (0x9)
#define MIDI_NOTE_OFF (0x8)
#define MIDI_KEY_PRESSURE (0xA)
#define MIDI_CONTROL (0xB)
#define MIDI_PROGRAM (0xC)
#define MIDI_CHANNEL_PRESSURE (0xD)
#define MIDI_PITCH_BEND (0xE)

extern "C" {

/* returns 0 on failure */
int
start_midi(int port)
{
	if(midi.getPortCount() < 1) {
		std::cout << "No MIDI ports available!\n";
		return 0;
	}
	
	try {
		midi.openPort(port);
	} catch(RtError &error) {
		return 0;
	}
	
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
	
		message->control = 0;
		message->pitch_bend = 0;
		
		if(type == MIDI_NOTE_OFF || (type == MIDI_NOTE_ON && rtmsg[2] == 0)) {
			message->velocity = 0;
			
		} else if(type == MIDI_NOTE_ON) {
			message->velocity = rtmsg[2] / 127.0;
			
		} else if(type == MIDI_KEY_PRESSURE) {
			message->velocity = rtmsg[2] / 127.0;
			
		} else if(type == MIDI_CONTROL) {
			message->control = 1;
			message->velocity = rtmsg[2] / 127.0;
			
		} else if(type == MIDI_PITCH_BEND) {
			message->pitch_bend = 1;
			message->velocity = rtmsg[2] / 127.0;
			
		} else {
			printf("unknown: %d size %ld [%d %d %d]\n", type, rtmsg.size(), rtmsg[0], rtmsg[1], rtmsg[2]);
			continue; // unrecognized
		}
		
		message->channel = rtmsg[0] & 0x0f;
		message->note = rtmsg[1];
		
		return 1;
	}
}

} // extern "C"
