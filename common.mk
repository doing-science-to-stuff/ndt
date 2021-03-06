#
# common.mk
# ndt: n-dimensional tracer
#
# Copyright (c) 2019 Bryan Franklin. All rights reserved.
#
LD=gcc
CFLAGS+=-m64
CFLAGS+=-Wall #-Wpadded
CFLAGS+=-O3
CFLAGS+=-DWITH_PNG
CFLAGS+=-DWITH_JPEG
CFLAGS+=-DWITH_YAML
#CFLAGS+=-DWITH_VALGRIND
#CFLAGS+=-DWITHOUT_SSE
#CFLAGS+=-DWITHOUT_INLINE
#CFLAGS+=-msse -msse2 -msse3 -msse4

# Uncomment for MPI support
#CC=mpicc
#LD=mpicc
#CFLAGS+=-DWITH_MPI

# Uncomment for AWS
#CC=/usr/lib64/openmpi/bin/mpicc
#LD=/usr/lib64/openmpi/bin/mpicc
#CFLAGS+=-I/opt/chef/embedded/include
#LDFLAGS+=-L/opt/chef/embedded/lib

# see: https://stackoverflow.com/questions/714100/os-detecting-makefile
UNAME_S := $(shell uname -s)
ifeq ($(UNAME_S),Linux)
    CFLAGS+=-D_GNU_SOURCE -std=c99
    LDFLAGS+=-L/lib/x86_64-linux-gnu -L/usr/lib/x86_64-linux-gnu -lm -lyaml -lpthread -ldl -export-dynamic
    DEP_CMD=sudo apt-get install
    DEPS=libjpeg-dev libpng-dev libyaml-dev
endif
ifeq ($(UNAME_S),Darwin)
    CFLAGS+=-I/opt/local/include
    LDFLAGS+=-L/opt/local/lib
    CFLAGS+=-DOSX
    DEP_CMD=sudo port install
    DEPS=jpeg libpng libyaml
endif

LDFLAGS+=-lm
LDFLAGS+=-lpng
LDFLAGS+=-ljpeg
LDFLAGS+=-lyaml

CFLAGS+=-g

.PHONY: clean all get-deps
.SUFFIXES: .c .o .h .so
.PRECIOUS: %.c %.o %.h

