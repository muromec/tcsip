RE_CFLAGS = -DHAVE_INTTYPES_H -DHAVE_STDBOOL_H \
    -DHAVE_INET6 -DHAVE_GAI_STRERROR -DRELEASE

INCL = -Ideps/include/ -I./libresip -I../srtp/include/ -I../srtp/crypto/include/ \
   -I../opus-1.0.2/include/  -Ig711

linux=y

LIBS = -lm -lpthread -lcrypto -lssl -lz -lresolv
LIBS += $(shell pkg-config speex --libs)
LIBS-$(apple) += $(shell pkg-config speexdsp --libs)

LIBS-$(linux) += -lasound
LIBS-$(apple) += -framework CoreFoundation
LIBS-$(apple) += -framework SystemConfiguration
LIBS-$(apple) += -framework AudioUnit
LIBS-$(apple) += -framework CoreAudio

DEP = deps-armlinux
LIBS-static += $(DEP)/libre.a  $(DEP)/libsrtp.a $(DEP)/libopus.a
LIBS += $(LIBS-static)

LIBS += $(LIBS-y)

all: cli

%.o: %.c
	$(CC) -arch x86_64 -std=gnu99  $< -o $@ -c $(INCL) $(ADD_INCL) $(RE_CFLAGS) -O0 -g

include libresip/cli.mk
objects += $(patsubst %,libresip/%.o,$(lobj))

objects += g711/g711.o
objects += cli.o

CC := gcc

cli: $(objects) $(LIBS-static)
	$(CC) -o cli $(LIBS) $(objects)

clean:
	rm -f $(objects)
