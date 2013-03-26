ifeq ($(shell uname -s),Darwin)
apple=y
endif

ifeq ($(shell uname -s),Linux)
linux=y
endif
