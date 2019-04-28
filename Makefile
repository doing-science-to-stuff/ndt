#
# Makefile
# ndt: n-dimensional tracer
#
# Copyright (c) 2019 Bryan Franklin. All rights reserved.
#
include common.mk

.PHONY: clean scenes objects test valgrind

all: ndt objects scenes 

ndt: ndt.o vectNd.o image.o scene.o camera.o matrix.o kmeans.o timing.o map.o object.o bounding.o
	gcc -o ndt $(LDFLAGS) $% $^ 

scenes:
	make -C scenes

objects:
	make -C objects

valgrind: ndt objects scenes
	valgrind --leak-check=full --show-reachable=yes --vgdb-error=0 -v ./ndt -w 60 -h 45 -t 4 -k 8 -f 300 -i 64 -e 67 -q f -d 5 -s scenes/random.so

test: scenes ndt objects
	./ndt -w 640 -h 480 -t 4 -k 8 -f 300 -i 64 -e 67 -q f -s scenes/balls.so -d 3

clean:
	rm -f *.o
	make -C scenes clean
	make -C objects clean
