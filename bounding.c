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

    if( !sph->prepared ) {
        bounding_sphere_prepare(sph);
    }

    /* compute d */
    vectNd *center = &sph->center;

    vectNd oc;
    double oc_len2;
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

    double voc;
    vectNd_dot(v,&oc,&voc); /* v . (o-c) */
    vectNd_free(&oc);

    double r_sqr = sph->radius_sqr;
    double voc2 = voc*voc;
    double desc = voc2 - oc_len2 + r_sqr;

    /* desc_root = sqrt(desc); */
    /* d = -(voc +/- desc_root); // and d must be non-negative */
    /* reject if d < 0 */
    /* 0 > -(voc + desc_root)  or  0 > -(voc - desc_root) */
    /* 0 > -voc - desc_root  or  0 > -voc + desc_root */
    /* desc_root >= 0, so only right expression is interesting */
    /* 0 > -voc + desc_root */
    /* voc > desc_root */
    /* voc^2 > desc and voc > 0 */

    /* check to see if bounding sphere is behind us */
    if( desc < 0.0 || (voc > 0.0 && voc2 > desc) ) {
        return 0;
    }

    return 1;
}

/* Start bounds_list operations */

int bounds_list_init(bounds_list *list) {
    memset(list, '\0', sizeof(*list));
    return 0;
}

int bounds_list_add(bounds_list *list, vectNd *vect, double radius) {
    /* add a new node to the head of the list */
    bounds_node *new_node = calloc(1,sizeof(bounds_node));
    if( new_node == NULL ) {
        perror("calloc");
        exit(1);
    }
    new_node->next = list->head;
    list->head = new_node;
    if( list->tail == NULL )
        list->tail = new_node;

    /* copy vector and radius into new node */
    vectNd_calloc(&new_node->bounds.center, vect->n);
    vectNd_copy(&new_node->bounds.center, vect);
    new_node->bounds.radius = radius;

    return 0;
}

int bounds_list_join(bounds_list *list, bounds_list *other) {
    if( list->tail )
        list->tail->next = other->head;
    else
        list->head = other->head;
    list->tail = other->tail;
    other->head = NULL;
    other->tail = NULL;
    return 0;
}

int bounds_list_free(bounds_list *list) {
    /* start at head */
    bounds_node *curr = list->head;

    /* loop through all nodes */
    while(curr) {
        /* free each vector */
        vectNd_free(&curr->bounds.center);
        bounds_node *next = curr->next;
        free(curr); curr=NULL;
        curr = next;
    }
    list->head = NULL;
    list->tail = NULL;

    return 0;
}

int bounds_list_centroid(bounds_list *list, vectNd *centroid) {
    /* Nelder-Mead could be used to find a center that minimizes the radius. */

    bounds_node *curr = list->head;
    vectNd sum;
    vectNd_calloc(&sum, centroid->n);
    int count = 0;
    while(curr) {
        vectNd_add(&sum, &curr->bounds.center, &sum);
        ++count;
        curr = curr->next;
    }
    vectNd_scale(&sum, 1.0/count, centroid);

    return 0;
}

int bounds_list_radius(bounds_list *list, vectNd *centroid, double *radius) {
    bounds_node *curr = list->head;
    double max = -1.0;
    while(curr) {
        double dist = -1.0;
        vectNd_dist(centroid, &curr->bounds.center, &dist);
        if( curr->bounds.radius > 0.0 )
            dist += curr->bounds.radius;
        max = (dist>max)?dist:max;
        curr = curr->next;
    }
    *radius = max;

    return 0;
}
