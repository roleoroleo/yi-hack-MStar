OBJECTS = h264grabber.o

all: h264grabber

h264grabber.o: h264grabber.c $(HEADERS)
	$(CC) -c $< -fPIC -O2 -o $@

h264grabber: $(OBJECTS)
	$(CC) $(OBJECTS) -fPIC -O2 -o $@
	$(STRIP) $@

.PHONY: clean

clean:
	rm -f h264grabber
	rm -f $(OBJECTS)
