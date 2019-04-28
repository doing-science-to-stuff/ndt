#
# common.mk
# ndt: n-dimensional tracer
#
# Copyright (c) 2019 Bryan Franklin. All rights reserved.
#
CFLAGS=-m64 -I/opt/local/include
CFLAGS+=-Wall #-Wpadded
CFLAGS+=-O3
CFLAGS+=-DWITH_PNG
CFLAGS+=-DWITH_JPEG
CFLAGS+=-DWITH_YAML
#CFLAGS+=-DWITH_VALGRIND
#CFLAGS+=-DWITHOUT_SSE
#CFLAGS+=-DWITHOUT_INLINE
#CFLAGS+=-msse -msse2 -msse3 -msse4

LDFLAGS=-lm
LDFLAGS+=-Wall
LDFLAGS+=-L/opt/local/lib -lpng
LDFLAGS+=-L/opt/local/lib -ljpeg
LDFLAGS+=-L/opt/local/lib -lyaml

CFLAGS+=-g
LDFLAGS+=-g

.PHONY: clean all
.SUFFIXES: .c .o .h .so
.PRECIOUS: %.c %.o %.h

# see: https://stackoverflow.com/questions/714100/os-detecting-makefile
UNAME_S := $(shell uname -s)
ifeq ($(UNAME_S),Linux)
    CFLAGS+=-D_GNU_SOURCE -std=c99
    LDFLAGS+=-lm -lyaml -lpthread -ldl -export-dynamic
    DEP_CMD=sudo apt-get install
    DEPS=libjpeg-dev libpng-dev libyaml-dev
endif
ifeq ($(UNAME_S),Darwin)
    CCFLAGS += -D OSX
    DEP_CMD=sudo port install
    DEPS=jpeg libpng libyaml
endif
