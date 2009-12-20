CC = gcc
CFLAGS = -g -ansi -Wall
LDFLAGS = -llua
OBJECTS = ckv.o ckvlib.o
EXECUTABLE=ckv

$(EXECUTABLE): $(OBJECTS)
	$(CC) $(LDFLAGS) $(OBJECTS) -o $@

clean:
	rm -f *.o $(EXECUTABLE)
