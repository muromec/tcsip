include os.mk
include tcsip.mk

RE_CFLAGS = -DHAVE_INTTYPES_H -DHAVE_STDBOOL_H \
    -DHAVE_INET6 -DHAVE_GAI_STRERROR -DRELEASE

INCL = -Ideps/include/ -I./src -I./src/util -I../srtp/include/ -I../srtp/crypto/include/ \
   -I../opus/include/ -I../speex/include  -Ig711 -I./src/rehttp/

DEPI = deps/$(OS)-$(ARCH)/
INCL += -I$(DEPI)/srtp -I$(DEPI)/speex

LIBS = -lm -lz 
LIBS-$(linux) += -lpthread -lcrypto -lssl  -lresolv
LIBS-$(apple) += -lpthread -lcrypto -lssl  -lresolv 
LIBS-static += $(DEP)/libspeex.a $(DEP)/libspeexdsp.a
LIBS-$(linux) += -lasound
LIBS-$(linux) += -ldl
LIBS-$(apple) += -framework CoreFoundation
LIBS-$(apple) += -framework SystemConfiguration
LIBS-$(apple) += -framework AudioUnit
LIBS-$(apple) += -framework CoreAudio
LIBS-$(apple) +=  -Wl,-flat_namespace
LIBS-$(android) += -llog -lOpenSLES


LIBS-static += $(DEP)/libre.a  $(DEP)/libsrtp.a $(DEP)/libopus.a
LIBS-static += $(DEP)/libmsgpack.a
LIBS-static-$(android) += $(DEP)/libssl.a $(DEP)/libcrypto.a

LIBS += $(LIBS-static) $(LIBS-static-y)

LIBS += $(LIBS-y)

all: texr-cli

OPT_FLAGS := -fPIC -O2
sources = $(patsubst %,src/%.c,$(lobj))
sources_sound = $(patsubst %,src/%.c,$(sound-obj))
sources_rtp = $(patsubst %,src/%.c,$(rtp-obj))

sources_cli = $(sources) cli.c
sources_libdriver = $(sources) driver.c
sources_daemon = $(sources) driver.c driver_cli.c
sources_shout = src/tools/tcshout.c $(sources_sound)
sources_ice = src/tools/ice.c

objects_cli = $(patsubst %.c,$B/%.o,$(sources_cli))
objects_libdriver = $(patsubst %.c,$B/%.o,$(sources_libdriver))
objects_daemon = $(patsubst %.c,$B/%.o,$(sources_daemon))
objects_shout = $(patsubst %.c,$B/%.o,$(sources_shout))
objects_ice = $(patsubst %.c,$B/%.o,$(sources_ice))

CC := $(CC)

all: texr-cli texr-daemon
	
shared: libredriver.so

ifeq ($(OS),Linux)
LD_SHARED = -Wl,-soname,libredriver.so
endif

$(B)/%.o: %.c
	[ -d $(shell dirname $@) ] || mkdir -p $(shell dirname $@)
	$(CC) -std=gnu99 -Werror $< -o $@ -c $(INCL) $(ADD_INCL) $(RE_CFLAGS) $(OPT_FLAGS)


texr-cli: $(objects_cli) $(LIBS-static)
	$(CC) -Wl,-undefined,error $(objects_cli) $(LIBS-static) $(LIBS) -o $@

texr-daemon: $(objects_daemon) $(LIBS-static)
	$(CC) $(objects_daemon) $(LIBS-static) $(LIBS) -o $@

tcshout: $(objects_shout)
	$(CC) $(objects_shout) $(LIBS-static) $(LIBS) -o $@

ice: $(objects_ice)
	$(CC) $(objects_ice) $(LIBS-static) $(LIBS) -o $@

libredriver.so: $(objects_libdriver) $(LIBS-static)
	echo $(sources_libdriver)
	$(CC) -shared $(LD_SHARED) -o libredriver.so $(objects_libdriver)  $(LIBS-static) $(LIBS)

tools: tcshout ice

clean:
	rm -f $(objects) texr-cli texr-daemon libredriver.so
