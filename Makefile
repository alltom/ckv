CC = gcc
CFLAGS = -g -ansi -Wall -O3
LDFLAGS = -llua -lrtaudio -framework CoreAudio -lpthread
OBJECTS = ckv.o ckvbaselite.o ugen/ugen.o audio.o pq.o
EXECUTABLE=ckv

$(EXECUTABLE): $(OBJECTS)
	g++ $(LDFLAGS) $(OBJECTS) -o $@

audio.o: audio.cpp
	g++ $(CFLAGS) -c -o audio.o audio.cpp -D__MACOSX_CORE__

clean:
	rm -f *.o */*.o $(EXECUTABLE)
