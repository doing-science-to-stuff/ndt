/*
 * cluster.c
 * ndt: n-dimensional tracer
 *
 * Copyright (c) 2019 Bryan Franklin. All rights reserved.
 */
#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>
#include "object.h"
#include "../kmeans.h"

static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;

int type_name(char *name, int size) {
    strncpy(name,"cluster",size);
    return 0;
}

int params(object *obj, int *n_pos, int *n_dir, int *n_size, int *n_flags, int *n_obj) {
    /* report how many of each type of parameter is needed */
    *n_pos = 0;
    *n_dir = 0;
    *n_size = 0;
    *n_flags = 1;
    *n_obj = 0;

    return 0;
}

int bounding_points(object *obj, bounds_list *list)
{
    for(int i=0; i<obj->n_obj; i++) {
        bounds_list points;
        bounds_list_init(&points);
        object *sub = obj->obj[i];

        if( strcmp("outline", sub->name) == 0 )
            continue;

        sub->bounding_points(sub, &points);
        if( points.head == NULL ) {
            /* if an infinite object is encountered,
             * clear the lists and return. */
            bounds_list_free(&points);
            bounds_list_free(list);
            return 0;
        }

        /* combine new points with complete list */
        bounds_list_join(list, &points);

        bounds_list_free(&points);
    }

    return 1;
}

#if 0
static int check_bounds(object *obj, bounding_sphere *bounds) {
    for(int i=0; i<obj->n_obj; ++i) {
        double dist = -1.0;
        vectNd_dist(&bounds->center, &obj->obj[i]->bounds.center, &dist);
        if( dist + obj->obj[i]->bounds.radius > bounds->radius
            || check_bounds(obj->obj[i], bounds) ) {
            printf("bounds failure for '%s' in '%s', %g+%g > %g\n",
                    obj->obj[i]->name, obj->name,
                    dist, obj->obj[i]->bounds.radius,
                    bounds->radius);
            return 1;
        }
    }

    return 0;
}
#endif /* 0 */

static int cluster_do_clustering(object *clstr, int k)
{
    if( k<2 || clstr->n_obj < 2*k )
        return 0;

    /* setup kmeans */
    kmean_vector_list_t centers;
    kmeans_new_list(&centers, clstr->n_obj, clstr->dimensions);

    /* copy all bounding sphere centers */
    for(int i=0; i<clstr->n_obj; ++i)
        vectNd_copy(&centers.data[i].vect, &clstr->obj[i]->bounds.center);

    /* perform clustering */
    kmean_vector_list_t centroids;
    kmeans_new_list(&centroids,k,clstr->dimensions);
    for(int i=0; i<k; ++i) {
        vectNd_copy(&centroids.data[i].vect, &clstr->obj[i]->bounds.center);
        centroids.data[i].which = i;
    }
    kmeans_find(&centers,&centroids);

    /* split objects into sub-clusters */
    object **subs = calloc(k,sizeof(object*));
    for(int i=0; i<k; ++i) {
        subs[i] = object_alloc(clstr->dimensions, "cluster", "sub cluster");
        object_add_flag(subs[i], k);
        snprintf(subs[i]->name, sizeof(subs[i]->name), "%s%i", clstr->name, i);
    }
    for(int i=0; i<clstr->n_obj; ++i) {
        int which = centers.data[i].which;
        object_add_obj(subs[which], clstr->obj[i]);
    }

    /* verify that all clusters have objects */
    int did_split = 1;
    for(int i=0; did_split==1 && i<k; ++i) {
        if( subs[i]->n_obj == clstr->n_obj )
            did_split = 0;
    }

    if( did_split==1 ) {
        /* recurse on each sub-clusters */
        for(int i=0; i<k; ++i) {
            object_get_bounds(subs[i]);
            cluster_do_clustering(subs[i],subs[i]->flag[0]);
        }

        /* replace list with new sub-clusters */
        free(clstr->obj); clstr->obj=NULL;
        clstr->n_obj = clstr->cap_obj = 0;
        for(int i=0; i<k; ++i) {
            if( subs[i]!=NULL && subs[i]->n_obj > 0 ) {
                object_add_obj(clstr, subs[i]);
            }
        }
    } else {
        for(int i=0; i<k; ++i) {
            /* prevent freeing of sub-objects in, now useless, clusters */
            subs[i]->n_obj = 0;

            /* destroy unused cluster */
            object_free(subs[i]); subs[i] = NULL;
        }
    }
    free(subs); subs=NULL;

    #if 0
    printf("Adding a cluster outline sphere for %s.\n", clstr->name);
    /* For debugging, add a transparent sphere where the bounding sphere is. */
    object_get_bounds(clstr);
    object *outline = object_alloc(clstr->dimensions, "sphere", "outline");
    outline->red = outline->green = outline->blue = 0.1;
    outline->red_r = outline->green_r = outline->blue_r = 0.0;
    outline->refract_index = 1.0;
    outline->transparent = 1;
    object_add_pos(outline, &clstr->bounds.center);
    object_add_size(outline, clstr->bounds.radius-EPSILON);
    vectNd_print(&clstr->bounds.center, "\tcenter");
    printf("\toutline radius = %g\n", clstr->bounds.radius);
    object_add_obj(clstr, outline);
    #endif /* 0 */

    kmeans_free_list(&centers);
    kmeans_free_list(&centroids);

    object_get_bounds(clstr);

    #if 0
    if( check_bounds(clstr, &clstr->bounds) ) {
        fprintf(stderr,"bound check failed!");
        exit(1);
    }
    #endif /* 0 */

    return 1;
}

static int prepare(object *obj) {
    pthread_mutex_lock(&lock);

    /* fill in any ray invariant parameters */
    if( !obj->prepared ) {
        /* cluster objects */
        cluster_do_clustering(obj, obj->flag[0]);

        object_get_bounds(obj);

        /* mark object as prepared */
        obj->prepared = 1;
    }

    pthread_mutex_unlock(&lock);

    return 1;
}

int intersect(object *obj, vectNd *o, vectNd *v, vectNd *res, vectNd *normal, object **obj_ptr)
{
    /* prepare an object if it is not prepared yet */
    if( !obj->prepared ) {
        prepare(obj);
    }

    int ret = trace(o, v, obj->obj, obj->n_obj, res, normal, obj_ptr, -1.0);

    return ret;
}
