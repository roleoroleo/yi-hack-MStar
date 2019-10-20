OBJECTS_1 = imggrabber.o
OBJECTS_2 = imggrabbercmd.o
OBJECTS = $(OBJECTS_1) $(OBJECTS_2)
FFMPEG = ffmpeg-4.0.4
FFMPEG_DIR = ./$(FFMPEG)
INC = -I$(FFMPEG_DIR)
LIB_1 = -lpthread -lm
LIB_2 = $(FFMPEG_DIR)/libavcodec/libavcodec.a $(FFMPEG_DIR)/libavutil/libavutil.a -lpthread -lm

all: imggrabber imggrabbercmd

imggrabber.o: imggrabber.c $(HEADERS)
	$(CC) -c $< -fPIC -O2 -o $@

imggrabbercmd.o: imggrabbercmd.c $(HEADERS)
	@$(build_ffmpeg)
	$(CC) -c $< $(INC) -fPIC -O2 -o $@

imggrabber: $(OBJECTS_1)
	$(CC) $(OBJECTS_1) $(LIB_1) -fPIC -O2 -o $@
	$(STRIP) $@

imggrabbercmd: $(OBJECTS_2)
	$(CC) $(OBJECTS_2) $(LIB_2) -fPIC -O2 -o $@
	$(STRIP) $@

.PHONY: clean

clean:
	rm -f imagegrabber
	rm -f imagegrabbercmd
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
