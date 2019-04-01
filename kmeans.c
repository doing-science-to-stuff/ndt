/*
 * kmeans.c
 * ndt: n-dimensional tracer
 *
 * Copyright (c) 2019 Bryan Franklin. All rights reserved.
 */
#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include "vectNd.h"
#include "kmeans.h"

static double kmeans_vect_dist(kmean_vector_t *vect1, kmean_vector_t *vect2)
{
    double ret = -1.0;

    vectNd_dist(&vect1->vect,&vect2->vect,&ret);

    return ret;
}

static int kmeans_assign(kmean_vector_t *vect, kmean_vector_list_t *cents)
{
    int which=-1, i=0;
    double dist=-1, min_dist=-1;

    /* loop through all centroids, find one closest to vect */
    min_dist = dist = kmeans_vect_dist(vect,&cents->data[0]);
    which = 0;
    for(i=1; i<cents->num; ++i)
    {
        dist = kmeans_vect_dist(vect,&cents->data[i]);
        if( dist < min_dist )
        {
            min_dist = dist;
            which = i;
        }
    }

    /* return centroid id for vect */
    return which;
}

static double kmeans_adjust_centers(kmean_vector_list_t *list, kmean_vector_list_t *cents)
{
    int i=0, which=0;
    int *nums=NULL;
    kmean_vector_list_t sums;

    /* allocate sums and counters */
    nums = calloc(cents->num,sizeof(int));
    kmeans_new_list(&sums,cents->num,cents->data[0].vect.n);

    /* reset sums */
    for(i=0; i<sums.num; ++i) {
        vectNd_reset(&sums.data[i].vect);
        nums[i] = 0;
    }

    /* loop through list */
    for(i=0; i<list->num; ++i)
    {
        which = list->data[i].which;
        nums[which]++;
        /* sum values from each vector by class */
        vectNd_add(&sums.data[which].vect, &list->data[i].vect,
                    &sums.data[which].vect);
    }

    /* divide each sum by number in class */
    double dist = 0.0;
    kmean_vector_t old_pos;
    vectNd_alloc(&old_pos.vect,cents->data[0].vect.n);
    for(which=0; which<cents->num; ++which)
    {
        if( nums[which] > 0 )
        {
            old_pos.vect.n = cents->data[which].vect.n;
            old_pos.which = cents->data[which].which;

            vectNd_copy(&old_pos.vect,&cents->data[which].vect);
            vectNd_scale(&sums.data[which].vect, 1.0/nums[which],
                            &cents->data[which].vect);
            dist += kmeans_vect_dist(&old_pos, &cents->data[which]);
        }
    }
    vectNd_free(&old_pos.vect);

    free(nums); nums=NULL;
    kmeans_free_list(&sums);

    return dist;
}

static double kmeans_update(kmean_vector_list_t *list, kmean_vector_list_t *cents)
{
    int i=0;
    int ret = 0;

    /* assign each data point in list to nearest centroid */
    for(i=0; i<list->num; ++i)
    {
        int new_which = kmeans_assign(&list->data[i],cents);
        if( list->data[i].which != new_which )
        {
            list->data[i].which = new_which;
            ret++;
        }
    }

    /* find new centers */
    ret = kmeans_adjust_centers(list,cents);

    return ret;
}

/* Use Lloyd's algorithm to find kmeans centers */
/* http://en.wikipedia.org/wiki/Lloyd%27s_algorithm */
int kmeans_find(kmean_vector_list_t *data, kmean_vector_list_t *cents)
{
    int iterations = 1;
    double update_dist = -1;
    while( (update_dist=kmeans_update(data,cents))>cents->num )
    {
        ++iterations;
    }

    return iterations;
}

int kmeans_new_list(kmean_vector_list_t *list, int num, int len)
{
    void *ptr=NULL;
    int i=0;

    if( list==NULL )
        return -1;
    list->num = 0;
    list->data = NULL;

    ptr = calloc(num,sizeof(kmean_vector_t));
    if( ptr==NULL )
        return -1;
    list->data = ptr;

    list->num=num;
    for(i=0; i<num; ++i)
        vectNd_calloc(&list->data[i].vect,len);

    return 0;
}

int kmeans_free_list(kmean_vector_list_t *list)
{
    int i=0;

    for(i=0; i<list->num; ++i)
    {
        vectNd_free(&list->data[i].vect);
    }
    free(list->data); list->data=NULL;

    return 0;
}

int kmeans_print_vect(kmean_vector_t *vect)
{
    int j=0; 

    printf("\t<");
    for(j=0; j<vect->vect.n; ++j)
    {
        printf("%g",vect->vect.v[j]);
        if( j != vect->vect.n-1 )
            printf(", ");
    }
    printf(">\n");

    return 0;
}

int kmeans_print_list(kmean_vector_list_t *list)
{
    int i=0, j=0;

    printf("Vector List:\n");
    printf("\tnum=%i\n", list->num);
    for(i=0; i<list->num; ++i)
    {
        printf("\t<");
        for(j=0; j<list->data[i].vect.n; ++j)
        {
            printf("%g",list->data[i].vect.v[j]);
            if( j != list->data[i].vect.n-1 )
                printf(", ");
        }
        printf(">\n");
    }

    return 0;
}
