/*
 * bounding.h
 * ndt: n-dimensional tracer
 *
 * Copyright (c) 2014-2019 Bryan Franklin. All rights reserved.
 */

#ifndef BOUNDING_SPHERE_H
#define BOUNDING_SPHERE_H
#include "vectNd.h"

typedef struct bounding_sphere_t
{
    vectNd center;
    double radius;

    int prepared:1;
    double radius_sqr;
} bounding_sphere;

int vect_bounding_sphere_intersect(bounding_sphere *sph, vectNd *o, vectNd *v, double min_dist);

#endif /* BOUNDING_SPHERE_H */
