CC = gcc
CFLAGS = -g -ansi -Wall -O3
LDFLAGS = -llua -lrtaudio -framework CoreAudio -lpthread
UGEN_OBJECTS=ugen/gain.o ugen/sinosc.o
OBJECTS = ckv.o luabaselite.o ugen/ugen.o $(UGEN_OBJECTS) audio.o pq.o
EXECUTABLE=ckv

$(EXECUTABLE): $(OBJECTS)
	g++ $(LDFLAGS) $(OBJECTS) -o $@

audio.o: audio.cpp
	g++ $(CFLAGS) -c -o audio.o audio.cpp -D__MACOSX_CORE__

clean:
	rm -f *.o */*.o $(EXECUTABLE)
