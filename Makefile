CC = gcc
CFLAGS = -g -ansi -pedantic -Wall -O3
LDFLAGS = -llua
LDFLAGS += -lrtaudio -framework CoreAudio -lpthread # audio
LDFLAGS += -lavformat -lavcodec -lavutil -lswscale -lz -lbz2 -lx264 # sndin
UGEN_OBJECTS=ugen/delay.o ugen/follower.o ugen/gain.o ugen/impulse.o ugen/noise.o ugen/osc.o ugen/sndin.o ugen/step.o
OBJECTS = ckv.o luabaselite.o ugen/ugen.o $(UGEN_OBJECTS) audio.o pq.o
EXECUTABLE=ckv

$(EXECUTABLE): $(OBJECTS)
	g++ $(LDFLAGS) $(OBJECTS) -o $@

audio.o: audio.cpp
	g++ $(CFLAGS) -c -o audio.o audio.cpp -D__MACOSX_CORE__

ugen/sndin.o: ugen/sndin.c
	$(CC) -g -Wall -O3 -c -o ugen/sndin.o ugen/sndin.c

clean:
	rm -f *.o */*.o $(EXECUTABLE)
