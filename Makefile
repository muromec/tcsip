RE_CFLAGS = -DRELEASE -DUSE_OPENSSL -DUSE_TLS -DUSE_ZLIB \
    -DHAVE_PTHREAD -DHAVE_GETIFADDRS -DHAVE_STRERROR_R -DHAVE_GETOPT \
    -DHAVE_INTTYPES_H -DHAVE_NET_ROUTE_H -DHAVE_SYS_SYSCTL_H -DHAVE_STDBOOL_H \
    -DHAVE_INET6 -DHAVE_LIBRESOLV -DHAVE_SYSLOG -DHAVE_FORK -DHAVE_INET_NTOP \
    -DHAVE_PWD_H -DHAVE_POLL -DHAVE_INET_PTON -DHAVE_SELECT -DHAVE_SELECT_H \
    -DHAVE_SETRLIMIT -DHAVE_SIGNAL -DHAVE_SYS_TIME_H -DHAVE_EPOLL \
    -DHAVE_UNAME -DHAVE_UNISTD_H -DHAVE_STRINGS_H -DHAVE_GAI_STRERROR -DHAVE_ROUTE_LIST -DUSE_OPENSSL_DTLS=1

INCL = -Ideps/include/ -I./libresip -I../srtp/include/ -I../srtp/crypto/include/ \
   -I../opus-1.0.2/include/  -Ig711

all: cli

%.o: %.c
	gcc $< -o $@ -c $(INCL) $(RE_CFLAGS) -O0 -g

objects = $(patsubst %.c,%.o,$(wildcard libresip/*.c))
objects += g711/g711.o

DEP = deps-armlinux

static = $(DEP)/libre.a  $(DEP)libsrtp.a $(DEP)libopus.a


cli: cli.o $(objects) lib*.a
	gcc cli.o -o cli libresip/*.o g711/*.o $(static) -lm -lpthread -lcrypto -lssl -lz -lspeex -lresolv -lasound
