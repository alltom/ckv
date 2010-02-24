extern "C" {
#include "ckv.h"
}

#include "rtmidi/RtMidi.h"

static RtMidiIn midi;

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
	midi.getMessage(&rtmsg);
	if(rtmsg.size() == 0)
		return 0;
	
	message->vel = 1;
	message->note = 60;
	
	return 1;
}

} // extern "C"
