OBJECTS = h264grabber.o
OBJECTS2 = h264grabber2.o
all: h264grabber h264grabber2

h264grabber.o: h264grabber.c $(HEADERS)
	$(CC) -c $< -fPIC -O1 -o $@

h264grabber2.o: h264grabber2.c $(HEADERS)
	$(CC) -c $< -fPIC -O1 -o $@

h264grabber: $(OBJECTS)
	$(CC) $(OBJECTS) -fPIC -O1 -o $@
	$(STRIP) $@

h264grabber2: $(OBJECTS2)
	$(CC) $(OBJECTS2) -fPIC -O1 -o $@
	$(STRIP) $@

.PHONY: clean

clean:
	rm -f h264grabber h264grabber2
	rm -f $(OBJECTS) $(OBJECTS2)
