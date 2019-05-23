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

typedef struct bounds_node_t {
    bounding_sphere bounds;
    struct bounds_node_t *next;
} bounds_node;

typedef struct bounds_list_t {
    struct bounds_node_t *head;
    struct bounds_node_t *tail;
} bounds_list;

int bounds_list_init(bounds_list *list);
int bounds_list_add(bounds_list *list, vectNd *vect, double radius);
int bounds_list_join(bounds_list *list, bounds_list *other);
int bounds_list_free(bounds_list *list);

int bounds_list_centroid(bounds_list *list, vectNd *centroid);
int bounds_list_radius(bounds_list *list, vectNd *centroid, double *radius);
int bounds_list_optimal(bounds_list *list, vectNd *centroid, double *radius);

#endif /* BOUNDING_SPHERE_H */
