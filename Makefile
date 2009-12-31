CC = gcc
CFLAGS = -g -ansi -Wall -O3
LDFLAGS = -llua
LDFLAGS += -lrtaudio -framework CoreAudio -lpthread # audio
LDFLAGS += -lavformat -lavcodec -lavutil -lswscale -lz -lbz2 -lx264 # sndio
UGEN_OBJECTS=ugen/gain.o ugen/sinosc.o ugen/sndio.o
OBJECTS = ckv.o luabaselite.o ugen/ugen.o $(UGEN_OBJECTS) audio.o pq.o
EXECUTABLE=ckv

$(EXECUTABLE): $(OBJECTS)
	g++ $(LDFLAGS) $(OBJECTS) -o $@

audio.o: audio.cpp
	g++ $(CFLAGS) -c -o audio.o audio.cpp -D__MACOSX_CORE__

ugen/sndio.o: ugen/sndio.c
	$(CC) -g -Wall -O3 -c -o ugen/sndio.o ugen/sndio.c

clean:
	rm -f *.o */*.o $(EXECUTABLE)
