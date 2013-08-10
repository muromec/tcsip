include os.mk
include libresip/cli.mk

RE_CFLAGS = -DHAVE_INTTYPES_H -DHAVE_STDBOOL_H \
    -DHAVE_INET6 -DHAVE_GAI_STRERROR -DRELEASE

INCL = -Ideps/include/ -I./libresip -I../srtp/include/ -I../srtp/crypto/include/ \
   -I../opus/include/ -I../speex/include  -Ig711 -I./rehttp/ \
   -I../json-c/


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
sources = $(patsubst %,libresip/%.c,$(lobj))
sources += g711/g711.c
sources += rehttp/http.c rehttp/auth.c

sources_cli = $(sources) cli.c
sources_libdriver = $(sources) driver.c
sources_daemon = $(sources) driver.c driver_cli.c

objects_cli = $(patsubst %.c,$B/%.o,$(sources_cli))
objects_libdriver = $(patsubst %.c,$B/%.o,$(sources_libdriver))
objects_daemon = $(patsubst %.c,$B/%.o,$(sources_daemon))

CC := gcc

all: texr-cli texr-daemon
	
shared: libredriver.so

$(B)/%.o: %.c
	[ -d $(shell dirname $@) ] || mkdir -p $(shell dirname $@)
	$(CC) -std=gnu99  $< -o $@ -c $(INCL) $(ADD_INCL) $(RE_CFLAGS) $(OPT_FLAGS)


texr-cli: $(objects_cli) $(LIBS-static)
	$(CC) -Wl,-undefined,error $(objects_cli) $(LIBS-static) $(LIBS) -o $@

texr-daemon: $(objects_daemon) $(LIBS-static)
	$(CC) $(objects_daemon) $(LIBS-static) $(LIBS) -o $@

libredriver.so: $(objects_libdriver) $(LIBS-static)
	$(CC) -shared -Wl,-soname,libredriver.so -o libredriver.so $< $(objects_libddriver)  $(LIBS-static) $(LIBS) 


clean:
	rm -f $(objects) texr-cli texr-daemon libredriver.so
