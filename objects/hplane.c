/*
 * hplane.c
 * ndt: n-dimensional tracer
 *
 * Copyright (c) 2019 Bryan Franklin. All rights reserved.
 */
#include <math.h>
#include <stdio.h>
#include "object.h"

int type_name(char *name, int size) {
    strncpy(name,"hplane",size);
    return 0;
}

int params(object *obj, int *n_pos, int *n_dir, int *n_size, int *n_flags, int *n_obj) {
    /* report how many of each type of parameter is needed */
    *n_pos = 1;
    *n_dir = 1;
    *n_size = 0;
    *n_flags = 0;
    *n_obj = 0;

    return 0;
}

int get_bounds(object *obj)
{
    /* hyperplanes are infinite in extent */
    obj->bounds.radius = -1;

    return 1;
}

int intersect(object *obj, vectNd *o, vectNd *v, vectNd *res, vectNd *normal, object **ptr)
{
    /* see: * http://en.wikipedia.org/wiki/Line–plane_intersection#Algebraic_form */
    double d=-1;
    vectNd pl;  /* p_0 - l_0 */
    double pln=0; /* (p_0-l_0) . n */
    double ln=-1;  /* l . n */

    vectNd *point = &obj->pos[0];
    vectNd_copy(normal, &obj->dir[0]);

    /* compute d */
    vectNd_alloc(&pl,v->n);
    vectNd_sub(point,o,&pl);
    vectNd_dot(&pl,normal,&pln);
    vectNd_dot(v,normal,&ln);

    if( fabs(ln) > EPSILON )
        d = pln / ln;

    /* find intersection */
    if( d >= EPSILON ) {
        vectNd_copy(res,o);
        vectNd_scale(v,d,&pl);
        vectNd_add(res,&pl,res);
    }

    vectNd_free(&pl);

    if( d < EPSILON )
        return 0;

    if( ptr != NULL )
        *ptr = obj;
    return 1;
}
