#
# Makefile
# ndt: n-dimensional tracer
#
# Copyright (c) 2019 Bryan Franklin. All rights reserved.
#
include ../common.mk

# see: https://stackoverflow.com/questions/714100/os-detecting-makefile
UNAME_S := $(shell uname -s)
ifeq ($(UNAME_S),Linux)
    CFLAGS+=-fPIC
    LDFLAGS+=-fPIC
endif
ifeq ($(UNAME_S),Darwin)
    LDFLAGS+=-shared -undefined dynamic_lookup
endif

# see: http://nuclear.mutantstargoat.com/articles/make/
src = $(wildcard *.c)
obj = $(src:.c=.so)
all: $(obj)

%.so: %.o
	$(CC) $(LDFLAGS) -shared -o $@ $<

clean:
	rm -f *.o *.so

