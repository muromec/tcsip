linux=n
apple=n
android=n

OS=$(shell uname -s)
ARCH=$(shell uname -m)
ifeq ($(OS),Darwin)
apple=y
endif

ifeq ($(OS),Linux)
linux=y
endif


DEP := bin/$(OS)-$(ARCH)
B = build-$(OS)-$(ARCH)
