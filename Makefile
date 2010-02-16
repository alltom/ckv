
# OSX, LINUX
PLATFORM=OSX

ifeq ($(PLATFORM),OSX)
	EXTRA_CFLAGS = -ansi
	AUDIO_LDFLAGS = -framework CoreAudio
	AUDIO_DEFINE = -D__MACOSX_CORE__
	FFMPEG_LDFLAGS = -lbz2 -lx264
endif

ifeq ($(PLATFORM),LINUX)
	AUDIO_LDFLAGS = -lasound
	AUDIO_DEFINE = -D__LINUX_ALSA__
endif

CC = gcc
CFLAGS = -g -pedantic -Wall -O3 $(EXTRA_CFLAGS)
LDFLAGS = -llua
LDFLAGS += -lrtaudio -lpthread $(AUDIO_LDFLAGS) # audio
LDFLAGS += -lavformat -lavcodec -lavutil -lswscale -lz $(FFMPEG_LDFLAGS) # sndin
UGEN_OBJECTS=audio/ugen/delay.o audio/ugen/follower.o audio/ugen/gain.o \
             audio/ugen/impulse.o audio/ugen/noise.o audio/ugen/osc.o \
             audio/ugen/sndin.o audio/ugen/step.o audio/ugen/ugen.o
AUDIO_OBJECTS=audio/audio.o $(UGEN_OBJECTS)
OBJECTS = ckv.o ckvm.o luabaselite.o $(AUDIO_OBJECTS) rtaudio_wrapper.o pq.o
EXECUTABLE=ckv

$(EXECUTABLE): $(OBJECTS)
	g++ -o $@ $(OBJECTS) $(LDFLAGS)

rtaudio_wrapper.o: rtaudio_wrapper.cpp
	g++ $(CFLAGS) -c -o rtaudio_wrapper.o rtaudio_wrapper.cpp $(AUDIO_DEFINE)

audio/ugen/sndin.o: audio/ugen/sndin.c
	$(CC) -g -Wall -O3 -c -o audio/ugen/sndin.o audio/ugen/sndin.c

clean:
	rm -f *.o */*.o */*/*.o $(EXECUTABLE)
