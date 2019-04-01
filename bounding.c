/*
 * bounding.c
 * ndt: n-dimensional tracer
 *
 * Copyright (c) 2014-2019 Bryan Franklin. All rights reserved.
 */

#include <stdio.h>
#include <math.h>
#include <pthread.h>
#include "object.h"
#include "bounding.h"

static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;

static int bounding_sphere_prepare(bounding_sphere *sph) {
    pthread_mutex_lock(&lock);

    /* fill in any ray invarient parameters */
    if( !sph->prepared ) {
        sph->radius_sqr = (sph->radius*sph->radius);
        sph->prepared = 1;
    }

    pthread_mutex_unlock(&lock);

    return 1;
}

int vect_bounding_sphere_intersect(bounding_sphere *sph, vectNd *o, vectNd *v, double min_dist)
{
    /* see: http://en.wikipedia.org/wiki/Line–bounding_sphere_intersection */
    vectNd oc;
    double d;
    double voc;
    double oc_len2;
    double desc;
    vectNd *center;

    if( !sph->prepared ) {
        bounding_sphere_prepare(sph);
    }

    /* compute d */
    center = &sph->center;

    vectNd_alloc(&oc,o->n);
    vectNd_sub(o,center,&oc); /* (o-c) */
    /* no point in taking a sqrt if it will just be squared again */
    vectNd_dot(&oc,&oc,&oc_len2); /* ||o-c||^2 */

    /* abort if bounding_sphere is too far away based on min_dist */
    if( min_dist > 0 ) {
        double min_dist_r = min_dist+sph->radius;
        if( oc_len2 > min_dist_r*min_dist_r ) {
            vectNd_free(&oc);
            return 0;
        }
    }

    vectNd_dot(v,&oc,&voc); /* v . (o-c) */
    vectNd_free(&oc);

    double r_sqr = sph->radius_sqr;
    desc = (voc*voc) - oc_len2 + r_sqr;
    if( desc < EPSILON ) {
        /* ray misses the bounding_sphere */
        return 0;
    }
    double desc_root = sqrt( desc );
    d = -(voc + desc_root);

    /* can't hit something behind us */
    if( d < EPSILON ) {
        /* try far side of the bounding_sphere, in case we're inside it */
        d = desc_root - voc;

        if( d < EPSILON ) {
            return 0;
        }
    }

    return 1;
}
