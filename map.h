/*
 * map.h
 * ndt: n-dimensional tracer
 *
 * Copyright (c) 2019 Bryan Franklin. All rights reserved.
 */
#ifndef MAP_H
#define MAP_H

#include "image.h"
#include "vectNd.h"
#include "matrix.h"

typedef enum map_type {
    MAP_SPHERICAL,
    MAP_CYLINDRICAL,
    MAP_LINEAR,
    MAP_RANDOM
} map_type_t;

typedef enum smooth_type {
    UNSMOOTHED,
    BILINEAR
} smooth_type_t;

typedef struct map {
    int d;
    vectNd orig;
    vectNd *base;
    map_type_t mode;
    smooth_type_t smoothing;
    image_t image;
} map_t;

int map_init(map_t *map);
int map_load_image(map_t *map, char *fname, int format);
int map_vect(map_t *map, vectNd *in, vectNd *out);
int map_image(map_t *map, vectNd *v, pixel_t *p);

#endif /* MAP_H */
