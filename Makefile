
# OSX, LINUX
PLATFORM=OSX

ifeq ($(PLATFORM),OSX)
	EXTRA_CFLAGS = -ansi
	AUDIO_DEFINE = -D__MACOSX_CORE__
	AUDIO_LDFLAGS = -lrtaudio -lpthread -framework CoreAudio
	MIDI_DEFINE = -D__MACOSX_CORE__
	MIDI_LDFLAGS = -lpthread -framework CoreMIDI -framework CoreFoundation -framework CoreAudio
	FFMPEG_LDFLAGS = -lbz2 -lx264
endif

ifeq ($(PLATFORM),LINUX)
	EXTRA_CFLAGS =
	AUDIO_DEFINE = -D__LINUX_ALSA__
	AUDIO_LDFLAGS = -lrtaudio -lpthread -lasound
	MIDI_DEFINE = -D__LINUX_ALSASEQ__
	MIDI_LDFLAGS = -lpthread -framework CoreMIDI -framework CoreFoundation -framework CoreAudio
	FFMPEG_LDFLAGS =
endif

CC = gcc
CFLAGS = -g -pedantic -Wall -O3 $(EXTRA_CFLAGS)
LDFLAGS = -llua
LDFLAGS += $(AUDIO_LDFLAGS) $(MIDI_LDFLAGS) # audio
LDFLAGS += -lavformat -lavcodec -lavutil -lswscale -lz $(FFMPEG_LDFLAGS) # sndin
UGEN_OBJECTS=audio/ugen/delay.o audio/ugen/follower.o audio/ugen/gain.o \
             audio/ugen/impulse.o audio/ugen/noise.o audio/ugen/osc.o \
             audio/ugen/sndin.o audio/ugen/step.o audio/ugen/ugen.o
AUDIO_OBJECTS=audio/audio.o $(UGEN_OBJECTS)
MIDI_OBJECTS=rtmidi/RtMidi.o
OBJECTS = ckv.o ckvm.o luabaselite.o $(AUDIO_OBJECTS) rtaudio_wrapper.o $(MIDI_OBJECTS) rtmidi_wrapper.o pq.o
EXECUTABLE=ckv

$(EXECUTABLE): $(OBJECTS)
	g++ -o $@ $(OBJECTS) $(LDFLAGS)

rtmidi/RtMidi.o: rtmidi/RtMidi.cpp rtmidi/RtError.h rtmidi/RtMidi.h
	g++ -O3 -Wall $(MIDI_DEFINE) -c rtmidi/RtMidi.cpp -o rtmidi/RtMidi.o

rtmidi_wrapper.o: rtmidi_wrapper.cpp
	g++ $(CFLAGS) -c -o rtmidi_wrapper.o rtmidi_wrapper.cpp $(MIDI_DEFINE)

rtaudio_wrapper.o: rtaudio_wrapper.cpp
	g++ $(CFLAGS) -c -o rtaudio_wrapper.o rtaudio_wrapper.cpp $(AUDIO_DEFINE)

audio/ugen/sndin.o: audio/ugen/sndin.c
	$(CC) -g -Wall -O3 -c -o audio/ugen/sndin.o audio/ugen/sndin.c

clean:
	rm -f *.o */*.o */*/*.o $(EXECUTABLE)
