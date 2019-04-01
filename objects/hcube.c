/*
 * hcube.c
 * ndt: n-dimensional tracer
 *
 * Copyright (c) 2019 Bryan Franklin. All rights reserved.
 */
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <math.h>
#include "object.h"

static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;

static int prepare(object *hcube)
{
    pthread_mutex_lock(&lock);

    /* fill in any ray invarient parameters */
    if( !hcube->prepared ) {

        object *face;
        vectNd zero;
        vectNd_calloc(&zero,hcube->dimensions);
        for(int i=0; i<hcube->n_size; i++) {
            /* positive face */
            face = object_alloc(hcube->dimensions, "hplane", "face a");
            object_add_pos(face, &zero);
            object_add_dir(face, &zero);
            vectNd_set(&face->pos[0],i,hcube->size[i]/2);
            vectNd_set(&face->dir[0],i,1);
            object_move(face, &hcube->pos[0]);
            object_add_obj(hcube, face);

            /* negative face */
            face = object_alloc(hcube->dimensions, "hplane", "face b");
            object_add_pos(face, &zero);
            object_add_dir(face, &zero);
            vectNd_set(&face->pos[0],i,-hcube->size[i]/2);
            vectNd_set(&face->dir[0],i,-1);
            object_move(face, &hcube->pos[0]);
            object_add_obj(hcube, face);
        }
        vectNd_free(&zero);

        /* mark object as prepared */
        hcube->prepared = 1;
    }

    pthread_mutex_unlock(&lock);

    return 0;
}

int type_name(char *name, int size) {
    strncpy(name,"hcube",size);
    return 0;
}

int params(object *obj, int *n_pos, int *n_dir, int *n_size, int *n_flags, int *n_obj) {
    /* report how many of each type of parameter is needed */
    *n_pos = 1;
    *n_dir = 0;
    *n_size = obj->dimensions;
    *n_flags = 0;
    *n_obj = 0;

    return 0;
}

int get_bounds(object *obj) {

    if( !obj->prepared ) {
        prepare(obj);
    }

    /* get centroid of all plane defining points */
    for(int i=0; i<obj->n_obj; i++) {
        vectNd_add(&obj->bounds.center,&obj->obj[i]->pos[0],
                    &obj->bounds.center);
    }
    vectNd_scale(&obj->bounds.center,1.0/obj->n_obj,&obj->bounds.center);

    /* get radius */
    double sum=0;
    for(int i=0; i<obj->n_obj; i++) {
        double dist;
        vectNd_dist(&obj->bounds.center, &obj->obj[i]->pos[0], &dist);
        sum += dist*dist;
    }
    /* each dimension will be added twice */
    sum /= 2;
    obj->bounds.radius = sqrt(sum) + EPSILON;

    return 1;
}

int intersect(object *hcube, vectNd *o, vectNd *v, vectNd *res, vectNd *normal, object **ptr)
{
    double best_dist = -1;
    vectNd lres;
    vectNd lnorm;
    int dim = -1;
    vectNd toInt;
    vectNd toPoint;;
    double dProd1 = -1;
    double dProd2 = -1;
    int ret = 0;

    if( !hcube->prepared ) {
        prepare(hcube);
    }

    dim = o->n;
    vectNd_alloc(&lres,dim);
    vectNd_alloc(&lnorm,dim);
    vectNd_alloc(&toInt,dim);
    vectNd_alloc(&toPoint,dim);

    /* for each face */
    for(int i=0; i<hcube->n_obj; ++i) {
        int lret = 0;
        /* check for intersection with each face */
        if( (lret = hcube->obj[i]->intersect(hcube->obj[i],o,v,&lres,&lnorm,ptr))==0 )
            continue;

        double ldist = -1;
        vectNd_dist(&lres,o,&ldist);
        if( ldist < best_dist || best_dist < 0 ) {
            /* check to see if intersection is on correct side of all other
             * faces */
            int valid = 1;
            for(int j=0; j<hcube->n_obj && valid!=0; ++j) {
                if( i==j )
                    continue;

                /* verify that lres is on correct side of face */   
                vectNd_sub(&lres,
                    &hcube->obj[j]->pos[0],&toInt);
                vectNd_sub(&hcube->obj[i]->pos[0],
                    &hcube->obj[j]->pos[0],&toPoint);
                vectNd_dot(&hcube->obj[j]->dir[0],&toInt,&dProd1);
                vectNd_dot(&hcube->obj[j]->dir[0],&toPoint,&dProd2);
                if( (dProd1*dProd2) < 0 )
                    valid = 0;
            }

            if( valid!=0 ) {
                vectNd_copy(res,&lres);
                vectNd_copy(normal,&lnorm);
                best_dist = ldist;
                ret = 1;
            }
        }
    }
    vectNd_free(&toInt);
    vectNd_free(&toPoint);
    vectNd_free(&lres);
    vectNd_free(&lnorm);

    if( ret && ptr != NULL ) {
        *ptr = hcube;
    }

    return ret;
}
