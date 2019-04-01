/*
 * kmeans.h
 * ndt: n-dimensional tracer
 *
 * Copyright (c) 2019 Bryan Franklin. All rights reserved.
 */
#include "vectNd.h"

typedef struct kmean_vector {
    vectNd vect;
    int which;
} kmean_vector_t;

typedef struct kmean_vector_list {
    struct kmean_vector *data;
    int num;
} kmean_vector_list_t;

int kmeans_find(kmean_vector_list_t *data, kmean_vector_list_t *cents);
int kmeans_new_list(kmean_vector_list_t *list, int num, int width);
int kmeans_free_list(kmean_vector_list_t *list);
int kmeans_print_vect(kmean_vector_t *vect);
int kmeans_print_list(kmean_vector_list_t *list);
