include os.mk
include libresip/cli.mk

RE_CFLAGS = -DHAVE_INTTYPES_H -DHAVE_STDBOOL_H \
    -DHAVE_INET6 -DHAVE_GAI_STRERROR -DRELEASE

INCL = -Ideps/include/ -I./libresip -I../srtp/include/ -I../srtp/crypto/include/ \
   -I../opus-1.0.2/include/  -Ig711


LIBS = -lm -lpthread -lcrypto -lssl -lz -lresolv
LIBS-$(linux) += $(shell pkg-config speex --libs)
LIBS-static-$(apple) += $(DEP)/libspeex.a $(DEP)/libspeexdsp.a
LIBS-$(linux) += -lasound
LIBS-$(apple) += -framework CoreFoundation
LIBS-$(apple) += -framework SystemConfiguration
LIBS-$(apple) += -framework AudioUnit
LIBS-$(apple) += -framework CoreAudio

DEP = deps-armlinux
LIBS-static += $(DEP)/libre.a  $(DEP)/libsrtp.a $(DEP)/libopus.a
LIBS += $(LIBS-static) $(LIBS-static-y)

LIBS += $(LIBS-y)

all: cli

%.o: %.c
	$(CC) -std=gnu99  $< -o $@ -c $(INCL) $(ADD_INCL) $(RE_CFLAGS) -O0

objects += $(patsubst %,libresip/%.o,$(lobj))

objects += g711/g711.o
objects += cli.o

CC := gcc

cli: $(objects) $(LIBS-static)
	$(CC) -o cli $(LIBS) $(objects)

clean:
	rm -f $(objects)
