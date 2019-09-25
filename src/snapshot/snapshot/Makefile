OBJECTS_F = framefinder.o
OBJECTS_B = buffer2jpg.o
OBJECTS = $(OBJECTS_F) $(OBJECTS_B)
FFMPEG = ffmpeg-4.0.4
FFMPEG_DIR = ./$(FFMPEG)
INC = -I$(FFMPEG_DIR)
LIB = $(FFMPEG_DIR)/libavcodec/libavcodec.a $(FFMPEG_DIR)/libavutil/libavutil.a -lpthread -lm

all: framefinder buffer2jpg

framefinder.o: framefinder.c $(HEADERS)
	$(CC) -c $< -fPIC -O2 -o $@

buffer2jpg.o: buffer2jpg.c $(HEADERS)
	@$(build_ffmpeg)
	$(CC) -c $< $(INC) -fPIC -O2 -o $@

framefinder: $(OBJECTS_F)
	$(CC) $(OBJECTS_F) -fPIC -O2 -o $@
	$(STRIP) $@

buffer2jpg: $(OBJECTS_B)
	$(CC) $(OBJECTS_B) $(LIB) -fPIC -O2 -o $@
	$(STRIP) $@

.PHONY: clean

clean:
	rm -f framefinder
	rm -f buffer2jpg
	rm -f $(OBJECTS)

define build_ffmpeg

    # get archive
    if [ ! -f SDK/ffmpeg.tar.bz2 ]; then \
        mkdir -p SDK; \
        wget -O ./SDK/ffmpeg.tar.bz2.tmp "http://ffmpeg.org/releases/$(FFMPEG).tar.bz2"; \
        mv ./SDK/ffmpeg.tar.bz2.tmp ./SDK/ffmpeg.tar.bz2; \
    fi

    # untar
    if [ ! -f $(FFMPEG)/README.md ]; then \
         tar jxvf ./SDK/ffmpeg.tar.bz2; \
    fi

   # build
    if [ ! -f $(FFMPEG)/libavcodec/libavcodec.a ] || [ ! -f $(FFMPEG)/libavutil/libavutil.a ]; then \
         cd $(FFMPEG); \
        ./configure --enable-cross-compile --cross-prefix=$(CROSSPREFIX) --arch=armel --target-os=linux --prefix=$(CROSSPATH) --disable-ffplay --disable-ffprobe --disable-doc --disable-decoders --enable-decoder=h264 --disable-encoders --enable-encoder=mjpeg --disable-demuxers --enable-demuxer=h264 --disable-muxers --disable-protocols --disable-parsers --enable-parser=h264 --disable-filters --disable-bsfs --disable-indevs --disable-outdevs && \
         make; \
         cd ..;\
    fi
endef
