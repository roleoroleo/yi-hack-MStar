CC=/opt/yi/arm-linux-gnueabihf-4.8.3-201404/bin/arm-linux-gnueabihf-gcc

all: fifo install

fifo:
	$(CC) mkfifo.c -o mkfifo

install:
	mkdir -p _install/bin
	cp mkfifo _install/bin
