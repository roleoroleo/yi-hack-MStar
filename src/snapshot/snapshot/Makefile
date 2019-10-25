OBJECTS_1 = imggrabber.o
OBJECTS = $(OBJECTS_1)
JPEGLIB = jpeg-9c
JPEGLIB_DIR = ./$(JPEGLIB)
INC_1 = -I$(JPEGLIB_DIR)
LIB_1 = $(JPEGLIB_DIR)/.libs/libjpeg.a

all: imggrabber

imggrabber.o: imggrabber.c $(HEADERS)
	@$(build_jpeglib)
	$(CC) -c $< $(INC_1) -fPIC -O2 -o $@

imggrabber: $(OBJECTS_1)
	$(CC) $(OBJECTS_1) $(LIB_1) -fPIC -O2 -o $@
	$(STRIP) $@

.PHONY: clean

clean:
	rm -f imagegrabber
	rm -f $(OBJECTS)

define build_jpeglib

    # get archive
    if [ ! -f SDK/jpeg.tar.gz ]; then \
        mkdir -p SDK; \
        wget -O ./SDK/jpeg.tar.gz.tmp "https://www.ijg.org/files/jpegsrc.v9c.tar.gz"; \
        mv ./SDK/jpeg.tar.gz.tmp ./SDK/jpeg.tar.gz; \
    fi

    # untar
    if [ ! -f $(JPEGLIB)/README ]; then \
         tar zxvf ./SDK/jpeg.tar.gz; \
    fi

   # build
    if [ ! -f $(JPEGLIB)/.libs/libjpeg.a ]; then \
         cd $(JPEGLIB); \
        ./configure --host=$(CROSS) --enable-shared=yes && \
         make; \
         cd ..;\
    fi
endef
