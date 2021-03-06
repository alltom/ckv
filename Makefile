
# OSX, LINUX
PLATFORM=OSX

ifeq ($(PLATFORM),OSX)
	EXTRA_CFLAGS = -ansi
	AUDIO_DEFINE = -D__MACOSX_CORE__
	AUDIO_LDFLAGS = -lpthread -framework CoreAudio
	MIDI_DEFINE = -D__MACOSX_CORE__
	MIDI_LDFLAGS = -lpthread -framework CoreMIDI -framework CoreFoundation -framework CoreAudio
	FFMPEG_LDFLAGS = -lbz2 -lx264
endif

ifeq ($(PLATFORM),LINUX)
	EXTRA_CFLAGS =
	AUDIO_DEFINE = -D__LINUX_ALSA__
	AUDIO_LDFLAGS = -lpthread -lasound
	MIDI_DEFINE = -D__LINUX_ALSASEQ__
	MIDI_LDFLAGS = -lpthread -framework CoreMIDI -framework CoreFoundation -framework CoreAudio
	FFMPEG_LDFLAGS =
endif

CC = gcc
CFLAGS = -g -pedantic -Wall -O3 $(EXTRA_CFLAGS)
LDFLAGS = -llua
LDFLAGS += $(AUDIO_LDFLAGS) $(MIDI_LDFLAGS) # audio
LDFLAGS += -lavformat -lavcodec -lavutil -lswresample -lz $(FFMPEG_LDFLAGS) # sndin
OBJECTS = ckv.o ckvm.o luabaselite.o pq.o
OBJECTS += ckvaudio/audio.o rtaudio_wrapper.o rtaudio/RtAudio.o \
           ckvaudio/ugen/delay.o ckvaudio/ugen/follower.o ckvaudio/ugen/gain.o \
           ckvaudio/ugen/impulse.o ckvaudio/ugen/noise.o ckvaudio/ugen/osc.o \
           ckvaudio/ugen/sndin.o ckvaudio/ugen/step.o ckvaudio/ugen/ugen.o
OBJECTS += ckvmidi/midi.o rtmidi_wrapper.o rtmidi/RtMidi.o
EXECUTABLE=ckv

$(EXECUTABLE): $(OBJECTS)
	g++ -o $@ $(OBJECTS) $(LDFLAGS)

rtmidi/RtMidi.o: rtmidi/RtMidi.cpp rtmidi/RtError.h rtmidi/RtMidi.h
	g++ -O3 -Wall $(MIDI_DEFINE) -c rtmidi/RtMidi.cpp -o rtmidi/RtMidi.o

rtaudio/RtAudio.o: rtaudio/RtAudio.cpp rtaudio/RtError.h rtaudio/RtAudio.h
	g++ -O3 -Wall -c rtaudio/RtAudio.cpp -o rtaudio/RtAudio.o $(AUDIO_DEFINE)

rtmidi_wrapper.o: rtmidi_wrapper.cpp
	g++ $(CFLAGS) -c -o rtmidi_wrapper.o rtmidi_wrapper.cpp $(MIDI_DEFINE)

rtaudio_wrapper.o: rtaudio_wrapper.cpp
	g++ $(CFLAGS) -c -o rtaudio_wrapper.o rtaudio_wrapper.cpp $(AUDIO_DEFINE)

ckvaudio/ugen/sndin.o: ckvaudio/ugen/sndin.c
	$(CC) -g -Wall -O3 -c -o ckvaudio/ugen/sndin.o ckvaudio/ugen/sndin.c

clean:
	rm -f *.o */*.o */*/*.o $(EXECUTABLE)
