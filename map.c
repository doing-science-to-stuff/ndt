/*
 * map.c
 * ndt: n-dimensional tracer
 *
 * Copyright (c) 2019 Bryan Franklin. All rights reserved.
 */
#include <stdio.h>
#include <pthread.h>
#include "image.h"
#include "matrix.h"
#include "map.h"

static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;

int map_init(map_t *map)
{
    pthread_mutex_lock(&lock);
    memset(map,'\0',sizeof(*map));
    pthread_mutex_unlock(&lock);

    return 0;
}

int map_load_image(map_t *map, char *fname, int format)
{
    int ret=0;

    pthread_mutex_lock(&lock);
    image_init(&map->image);
    if( (ret=image_load(&map->image,fname,format))!=0 ) {
        printf("loading %s returned %i\n", fname, ret);
        exit(1);
    }
    map->image.edge_style = IMG_EDGE_LOOP;
    pthread_mutex_unlock(&lock);

    return ret;
}

int map_vect(map_t *map, vectNd *in, vectNd *out)
{
    vectNd relative;
    int dim;

    /* subtract the map origin from v */
    dim = map->orig.n;
    vectNd_alloc(&relative,dim);
    vectNd_sub(in,&map->orig,&relative);

    /* setup system of equations to be solved */
    matrix_t x, A, b;
    matrix_init(&A,dim,map->d);
    matrix_init(&x,map->d,1);
    matrix_init(&b,dim,1);
    for(int r=0; r<dim; ++r) {
        for(int c=0; c<map->d; ++c) {
            matrix_set_value(&A,r,c,map->base[c].v[r]);
        }
        matrix_set_value(&b,r,0,relative.v[r]);
    }
    matrix_gauss_elim(&x,&A,&b);
    vectNd_free(&relative);

    for(int i=0; i<map->d && i<in->n; ++i) {
        vectNd_set(out,i,matrix_get_value(&x,i,0));
    }
    matrix_free(&A);
    matrix_free(&x);
    matrix_free(&b);

    return 0;
}

static int map_spherical(map_t *map, vectNd *v, double *x, double *y) {

    vectNd mapped;
    vectNd_alloc(&mapped,v->n);
    vectNd_copy(&mapped,v);

    *x = (atan2(mapped.v[0], mapped.v[1])+M_PI) / (2*M_PI);
    double l2 = mapped.v[0]*mapped.v[0] + mapped.v[1]*mapped.v[1];
    *y = (atan2(mapped.v[2], sqrt(l2)) + M_PI/2)/M_PI;

    vectNd_free(&mapped);

    return 0;
}

static int map_cylinderical(map_t *map, vectNd *v, double *x, double *y) {
    vectNd mapped;
    vectNd_alloc(&mapped,v->n);
    vectNd_copy(&mapped,v);

    *x = (atan2(mapped.v[0], mapped.v[1])+M_PI) / (2*M_PI);
    *y = mapped.v[2];

    vectNd_free(&mapped);

    return 0;
}

static int map_linear(map_t *map, vectNd *v, double *xp, double *yp) {

    double x, y;
    x = v->v[0];
    y = v->v[1];

    x = x - floor(x);
    y = y - floor(y);

    *xp = x;
    *yp = y;

    return 0;
}

int map_image(map_t *map, vectNd *v, pixel_t *p)
{
    /* map position into basis space */
    vectNd mapped;
    int dim;
    dim = map->orig.n;
    vectNd_alloc(&mapped,dim);
    map_vect(map,v,&mapped);

    /* translate into requested spaces */
    double mx, my;
    switch( map->mode ) {
        case MAP_SPHERICAL:
            map_spherical(map,&mapped,&mx,&my);
            break;
        case MAP_CYLINDRICAL:
            map_cylinderical(map,&mapped,&mx,&my);
            break;
        case MAP_LINEAR:
            map_linear(map,&mapped,&mx,&my);
            break;
        case MAP_RANDOM:
            mx = drand48();
            my = drand48();
            break;
        default:
            printf("Unknown mapping mode %i\n", map->mode);
            mx = drand48();
            my = drand48();
            break;
    }
    vectNd_free(&mapped);

    /* map mx and my into pixel coordinates */
    double x,y;
    x = mx * map->image.width;
    y = (1-my) * map->image.height;
    int ix, iy;
    ix = (int)x;
    iy = (int)y;

    /* use resulting coordinates to get colour from image */
    if( map->smoothing==BILINEAR ) {
        dbl_pixel_t s1, s2, s3, s4;

        dbl_image_get_pixel(&map->image,ix,iy,&s1);
        dbl_image_get_pixel(&map->image,ix+1,iy,&s2);
        dbl_image_get_pixel(&map->image,ix,iy+1,&s3);
        dbl_image_get_pixel(&map->image,ix+1,iy+1,&s4);

        /* compute bilinear interpolation of s1,s2,s3, and s4 */
        dbl_pixel_t dp;
        bilinear_pixel(ix,iy,ix+1,iy+1, &s1, &s2, &s3, &s4, x, y, &dp);
        pixel_d2c(*p,dp);
    } else if( map->smoothing==UNSMOOTHED ) {
        image_get_pixel(&map->image,x,y,p);
    } else {
        printf("Unknown smoothing mode (%i).\n", map->smoothing);
        return 1;
    }

    return 0;
}
