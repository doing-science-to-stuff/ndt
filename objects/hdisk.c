/*
 * hdisk.c
 * ndt: n-dimensional tracer
 *
 * Copyright (c) 2019-2021 Bryan Franklin. All rights reserved.
 */
#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>
#include "object.h"
#include "../matrix.h"

static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;

static int prepare(object *obj) {
    pthread_mutex_lock(&lock);

    /* fill in any ray invariant parameters */
    if( !obj->prepared ) {

        /* create a hyper-plane for hdisk to use internally */
        object *hplane = object_alloc(obj->dimensions, "hplane", "hdisk's hplane");
        object_add_pos(hplane, &obj->pos[0]);
        object_add_dir(hplane, &obj->dir[0]);
        object_add_obj(obj, hplane);

        /* mark object as prepared */
        obj->prepared = 1;
    }

    pthread_mutex_unlock(&lock);

    return 1;
}

int type_name(char *name, int size) {
    strncpy(name,"hdisk",size);
    return 0;
}

int params(object *obj, int *n_pos, int *n_dir, int *n_size, int *n_flags, int *n_obj) {
    if( obj==NULL )
        return -1;

    /* report how many of each type of parameter is needed */
    *n_pos = 1;
    *n_dir = 1;
    *n_size = 1;
    *n_flags = 0;
    *n_obj = 0;

    return 0;
}

int bounding_points(object *obj, bounds_list *list) {
    bounds_list_add(list, &obj->pos[0], obj->size[0]);

    return 1;
}

int intersect(object *obj, vectNd *o, vectNd *v, vectNd *res, vectNd *normal, object **ptr)
{
    int ret = 1;

    /* prepare an object if it is not prepared yet */
    if( !obj->prepared ) {
        prepare(obj);
    }

    /* get intersection with disk's plane */
    ret = obj->obj[0]->intersect(obj->obj[0], o, v, res, normal, NULL);
    if( ret==0 )
        return ret;

    /* check to see if res is inside the disk */
    double dist = -1;
    vectNd_dist(res, &obj->pos[0], &dist);
    if( dist > obj->size[0] || dist < 0 )
        ret = 0;

    if( ret )
        *ptr = obj;

    return ret;
}
