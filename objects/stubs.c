/*
 * stubs.c
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
    /* data that is ray invariant and can be pre-computed in prepare function */
} prepped_t;

static int prepare(object *obj) {
    pthread_mutex_lock(&lock);

    /* fill in any ray invarient parameters */
    if( !obj->prepared ) {
        /* allocate prepared data structure */
        obj->prepped = calloc(1,sizeof(prepped_t));

        /* compute pre-computable (i.e. ray-invarient) data values */

        /* mark object as prepared */
        obj->prepared = 1;
    }

    pthread_mutex_unlock(&lock);

    return 1;
}

int cleanup(object *obj) {
    if( obj->prepared == 0 )
        return -1;

    /* free any buffers allocated (e.g., anything pointed to in prepped_t) */

    /* Note: if obj->prepped is freed, you must set prepped to NULL. */
    if( obj->prepped ) {
        free(obj->prepped); obj->prepped = NULL;
    }

    return 0;
}

int type_name(char *name, int size) {
    strncpy(name,"stubs",size);
    return 0;
}

int params(object *obj, int *n_pos, int *n_dir, int *n_size, int *n_flags, int *n_obj) {
    /* report how many of each type of parameter is needed */
    *n_pos = 0;
    *n_dir = 0;
    *n_size = 0;
    *n_flags = 0;
    *n_obj = 0;

    return 0;
}

int get_bounds(object *obj)
{
    double radius = 1.0;
    obj->bounds.radius = radius + EPSILON;

    /* The bounding sphere's center is allocated in object_alloc(). */

    /* compute center and radius of (hyper)sphere that contains the object */
    /* For objects that are infinitely large, radius should be -1. */

    return 1;
}

int intersect(object *obj, vectNd *o, vectNd *v, vectNd *res, vectNd *normal, object **ptr)
{
    /* prepare an object if it is not prepared yet */
    if( !obj->prepared ) {
        prepare(obj);
    }

    /* compute intersection of vector parallel to v passing through o */
    /* Note: *v is a unit vector, so DO NOT call vectNd_unitize here! */
    /* set res to the intersection point and normal to the normal at that point */

    /* set *ptr to obj and return 1 on a hit, 0 otherwise */
    if( ptr != NULL )
        *ptr = obj;

    return 0;
}
