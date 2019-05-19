/*
 * sphere.c
 * ndt: n-dimensional tracer
 *
 * Copyright (c) 2019 Bryan Franklin. All rights reserved.
 */
#include <stdio.h>
#include <math.h>
#include <pthread.h>
#include "object.h"

static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;

typedef struct prepared_data {
    double radius_sqr;
} prepped_t;

static int prepare(object *obj) {
    pthread_mutex_lock(&lock);

    /* fill in any ray invarient parameters */
    if( !obj->prepared ) {
        obj->prepped = calloc(1,sizeof(prepped_t));
        double radius = obj->size[0];
        ((prepped_t*)obj->prepped)->radius_sqr = pow(radius,2.0);
        obj->prepared = 1;
    }

    pthread_mutex_unlock(&lock);

    return 1;
}

int type_name(char *name, int size) {
    strncpy(name,"sphere",size);
    return 0;
}

int params(object *obj, int *n_pos, int *n_dir, int *n_size, int *n_flags, int *n_obj) {
    *n_pos = 1; /* center */
    *n_dir = 0;
    *n_size = 1;   /* radius */
    *n_flags = 0;
    *n_obj = 0;

    return 0;
}

int get_bounds(object *obj)
{
    /* bounding a sphere is silly, but do it anyway */
    double radius = obj->size[0];
    obj->bounds.radius = radius + EPSILON;
    vectNd_copy(&obj->bounds.center, &obj->pos[0]);

    return 1;
}

int intersect(object *obj, vectNd *o, vectNd *v, vectNd *res, vectNd *normal, object **ptr)
{
    if( !obj->prepared ) {
        prepare(obj);
    }

    /* see: http://en.wikipedia.org/wiki/Line–sphere_intersection */
    double d;
    double voc;
    double oc_len2;
    double desc;
    vectNd *oc = res; /* usurp res vector for oc */
    vectNd *center = &obj->pos[0];

    /* compute d */
    vectNd_sub(o,center,oc); /* (o-c) */
    /* no point in taking a sqrt if it will just be squared again */
    vectNd_dot(oc,oc,&oc_len2); /* ||o-c||^2 */
    vectNd_dot(v,oc,&voc); /* v . (o-c) */

    double r_sqr = ((prepped_t*)obj->prepped)->radius_sqr;
    desc = (voc*voc) - oc_len2 + r_sqr;
    if( desc < 0.0 ) {
        /* ray misses the sphere */
        return 0;
    }
    double desc_root = sqrt( desc );
    d = -(voc + desc_root);

    /* can't hit something behind us */
    if( d < EPSILON ) {
        /* try far side of the sphere, in case we're inside it */
        d = desc_root - voc;

        if( d < EPSILON ) {
            if( res!=NULL )
                vectNd_reset(res);
            if( normal!=NULL )
                vectNd_reset(normal);
            return 0;
        }
    }

    /* compute intersection and normal */
    if( normal!=NULL ) {
        vectNd_scale(v,d,res);
        vectNd_add(o,res,res);

        vectNd_sub(res,center,normal);
    }

    if( ptr != NULL )
        *ptr = obj;

    return 1;
}
