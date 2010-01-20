CC = gcc
CFLAGS = -g -ansi -pedantic -Wall -O3
LDFLAGS = -llua
LDFLAGS += -lrtaudio -framework CoreAudio -lpthread # audio
LDFLAGS += -lavformat -lavcodec -lavutil -lswscale -lz -lbz2 -lx264 # sndin
UGEN_OBJECTS=audio/ugen/delay.o audio/ugen/follower.o audio/ugen/gain.o \
             audio/ugen/impulse.o audio/ugen/noise.o audio/ugen/osc.o \
             audio/ugen/sndin.o audio/ugen/step.o audio/ugen/ugen.o
AUDIO_OBJECTS=audio/audio.o $(UGEN_OBJECTS)
OBJECTS = ckv.o ckvm.o luabaselite.o $(AUDIO_OBJECTS) rtaudio_wrapper.o pq.o
EXECUTABLE=ckv

$(EXECUTABLE): $(OBJECTS)
	g++ $(LDFLAGS) $(OBJECTS) -o $@

rtaudio_wrapper.o: rtaudio_wrapper.cpp
	g++ $(CFLAGS) -c -o rtaudio_wrapper.o rtaudio_wrapper.cpp -D__MACOSX_CORE__

audio/ugen/sndin.o: audio/ugen/sndin.c
	$(CC) -g -Wall -O3 -c -o audio/ugen/sndin.o audio/ugen/sndin.c

clean:
	rm -f *.o */*.o */*/*.o $(EXECUTABLE)
