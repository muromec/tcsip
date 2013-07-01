include os.mk
include libresip/cli.mk

RE_CFLAGS = -DHAVE_INTTYPES_H -DHAVE_STDBOOL_H \
    -DHAVE_INET6 -DHAVE_GAI_STRERROR -DRELEASE

INCL = -Ideps/include/ -I./libresip -I../srtp/include/ -I../srtp/crypto/include/ \
   -I../opus/include/ -I../speex/include  -Ig711 -I./rehttp/


LIBS = -lm -lz 
LIBS-$(linux) += -lpthread -lcrypto -lssl  -lresolv
LIBS-$(apple) += -lpthread -lcrypto -lssl  -lresolv 
LIBS-static += $(DEP)/libspeex.a $(DEP)/libspeexdsp.a
LIBS-$(linux) += -lasound
LIBS-$(apple) += -framework CoreFoundation
LIBS-$(apple) += -framework SystemConfiguration
LIBS-$(apple) += -framework AudioUnit
LIBS-$(apple) += -framework CoreAudio
LIBS-$(apple) +=  -Wl,-flat_namespace
LIBS-$(android) += -llog -lOpenSLES

DEP = deps-armlinux
LIBS-static += $(DEP)/libre.a  $(DEP)/libsrtp.a $(DEP)/libopus.a
LIBS-static += $(DEP)/libmsgpack.a
LIBS-static-$(android) += $(DEP)/libssl.a $(DEP)/libcrypto.a

LIBS += $(LIBS-static) $(LIBS-static-y)

LIBS += $(LIBS-y)

all: cli

%.o: %.c
	$(CC) -std=gnu99  $< -o $@ -c $(INCL) $(ADD_INCL) $(RE_CFLAGS) -O0

objects += $(patsubst %,libresip/%.o,$(lobj))

objects += g711/g711.o
objects += rehttp/http.o rehttp/auth.o

objects_driver = $(objects)
objects_driver += driver.o

objects_driver_cli = $(objects)
objects_driver_cli += driver.o driver_cli.o


CC := gcc

all: cli driver libredriver.so

cli: cli.o $(objects) $(LIBS-static)
	$(CC) -Wl,-undefined,error  $< $(objects) $(LIBS-static) $(LIBS) -o $@

driver: $(objects_driver_cli) $(LIBS-static)
	$(CC) $(objects_driver_cli) $(LIBS-static) $(LIBS) -o $@

libredriver.so: driver.o $(objects) $(LIBS-static)
	$(CC) -shared -Wl,-soname,libredriver.so -o libredriver.so $< $(objects)  $(LIBS-static) $(LIBS) 



clean:
	rm -f $(objects) cli.o driver.o
