linux=n
apple=n
android=n

DEP = deps-armlinux

OS=$(shell uname -s)
ARCH=$(shell uname -m)
ifeq ($(OS),Darwin)
apple=y
DEP := deps
endif

ifeq ($(OS),Linux)
linux=y
endif


B = build-$(OS)-$(ARCH)
