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

static int factorial(int n) {
    int ret = 1;
    for(int i=1; i<=n; ++i) {
        ret *= i;
    }
    return ret;
}

static int choose(int n, int m) {
    return factorial(n) / (factorial(m) * factorial(n-m));
}

/* see: https://en.wikipedia.org/wiki/Hypercube#Elements */
static int num_n_faces(int n, int m) {
    /* for an n-cube, how many m-dimensional components does it contain */
    return (1<<(n-m)) * choose(n, m);
}

static int add_faces(object *cube, int m) {
    int n = cube->dimensions;
    int num_faces = num_n_faces(n, m);
    vectNd dir;
    vectNd_calloc(&dir, n);
    vectNd pos;
    vectNd_calloc(&pos, n);

    if( m > 2 )
        add_faces(cube, m-1);

    printf("Adding %i-dimensional faces.\n", m);

    int *dirs_count, *pos_count;
    dirs_count = calloc(m, sizeof(int));
    pos_count = calloc(n-m, sizeof(int));
    for(int i=0; i<m; ++i)
        dirs_count[i] = m-i-1;
    int offset_id = 0;
    int real_offset_id = 0;
    for(int f=0; f<num_faces; ++f) {
        #if 0
        printf("\ndirs_count: ");
        for(int i=0; i<m; ++i) {
            printf(" %i", dirs_count[i]);
        }
        printf("\npos_count: ");
        for(int i=0; i<n-m; ++i) {
            printf(" %i", pos_count[i]);
        }
        printf("\n");
        printf("real_offset_id = %i\n", real_offset_id);
        #endif /* 0 */

        vectNd_reset(&pos);
        vectNd_reset(&dir);
        offset_id = real_offset_id;
        for(int i=0; i<n; ++i) {
            /* determine which dimensions 'face' exists in */
            int is_dir = 0;
            for(int j=0; is_dir==0 && j<m; ++j) {
                if( i == dirs_count[j] )
                    is_dir = 1;
            }
            if( is_dir ) {
                vectNd_set(&pos, i, cube->pos[0].v[i] - 0.5 * cube->size[i]);
                continue;
            }

            int value = offset_id % 2;
            offset_id >>= 1;

            /* determine location of 'face' */
            vectNd_set(&pos, i, cube->pos[0].v[i] + cube->size[i] * (value-0.5));
        }

        object *obj = NULL;
        if( m > 1 ) {
            /* add a orthotope for face */
            obj = object_alloc(cube->dimensions, "orthotope", "");
            object_add_flag(obj, m);
            if( m == 3 || n == 3 ) {
                obj->red = 0.0;
                obj->green = 0.0;
                obj->blue = 0.8;
            } else {
                obj->red = 0.8;
                obj->green = 0.8;
                obj->blue = 0.8;
            }

            for(int i=0; i<m; ++i) {
                int j = dirs_count[i];
                vectNd_set(&pos, j, cube->pos[0].v[j] - 0.5 * cube->size[j]);
                vectNd_reset(&dir);
                vectNd_set(&dir, j, cube->size[j]);
                object_add_dir(obj, &dir);
                #if 0
                vectNd_print(&dir,"dir");
                #endif /* 0 */
            }
            object_add_pos(obj, &pos);
            #if 0
            vectNd_print(&pos,"pos");
            #endif /* 0 */
        } else {
            fprintf(stderr, "%i-dimensional face shouldn't be requested.\n", m);
            exit(1);
        }
        snprintf(obj->name, sizeof(obj->name), "%id face %i of %s", m, f, cube->name);
        object_add_obj(cube, obj);

        /* update counters */
        ++real_offset_id;
        int i=0;
        while( i<(n-m) && pos_count[i] == 1 ) {
            pos_count[i] = 0;
            ++i;
        }
        if( i < n-m ) {
            pos_count[i] += 1;
        } else {
            /* pos counter reached end, so update dirs_counter */
            int j=0;
            while( j < m && dirs_count[j] == n-j-1 ) {
                if( j < m-1 ) {
                    dirs_count[j] = dirs_count[j+1]+1;
                } else {
                    dirs_count[j] = 0;
                }
                ++j;
            }
            if( j < m ) {
                dirs_count[j] += 1;
                --j;
                while( j >= 0 ) {
                    dirs_count[j] = dirs_count[j+1]+1;
                    --j;
                }
            }
        }
    }
    free(dirs_count); dirs_count=NULL;
    free(pos_count); pos_count=NULL;
    vectNd_free(&pos);
    vectNd_free(&dir);

    return 0;
}


static int prepare(object *hcube)
{
    pthread_mutex_lock(&lock);

    /* fill in any ray invarient parameters */
    if( !hcube->prepared ) {
        add_faces(hcube, hcube->dimensions-1);

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

    /* center of hcube is center of bounding sphere */
    vectNd_copy(&obj->bounds.center, &obj->pos[0]);

    double sum = 0;
    for(int i=0; i<obj->n_size; ++i) {
        sum += pow(obj->size[i]/2.0, 2.0);
    }
    obj->bounds.radius = sqrt(sum) + EPSILON;

    return 1;
}

int intersect(object *hcube, vectNd *o, vectNd *v, vectNd *res, vectNd *normal, object **ptr)
{
    if( !hcube->prepared ) {
        prepare(hcube);
    }

    int ret = trace(o, v, hcube->obj, hcube->n_obj, res, normal, ptr);

    if( ret && ptr != NULL ) {
        /* set object to hcube itself for material looks */
        *ptr = hcube;
    }

    return ret;
}
