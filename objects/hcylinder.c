/*
 * hcylinder.c
 * ndt: n-dimensional tracer
 *
 * Copyright (c) 2019 Bryan Franklin. All rights reserved.
 */
#include <stdio.h>
#include <math.h>
#include <pthread.h>
#include <stdlib.h>
#include "object.h"

static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;

typedef struct prepared_data {
    /* data that is ray invariant and can be pre-computed in prepare function */
    vectNd *axes;
    double *lengths;
    double *AdA;
    double *BdA;
} prepped_t;

static int prepare(object *cyl) {
    pthread_mutex_lock(&lock);

    /* fill in any ray invarient parameters */
    if( !cyl->prepared ) {
        int dim;
        vectNd *bottom = &cyl->pos[0];

        prepped_t *prepped = calloc(1,sizeof(prepped_t));

        dim = bottom->n;
        prepped->axes = calloc(dim-2,sizeof(vectNd));
        prepped->lengths = calloc(dim-2,sizeof(double));
        prepped->AdA = calloc(dim-2,sizeof(double));
        prepped->BdA = calloc(dim-2,sizeof(double));
        for(int i=0; i<dim-2; i++) {
            vectNd_alloc(&prepped->axes[i],dim);
            vectNd_sub(&cyl->pos[i+1],&cyl->pos[0],&prepped->axes[i]);
            vectNd_unitize(&prepped->axes[i]);
            vectNd_dist(&cyl->pos[i+1],&cyl->pos[0],&prepped->lengths[i]);
            vectNd_dot(&prepped->axes[i],&prepped->axes[i],&prepped->AdA[i]);
            vectNd_dot(&cyl->pos[0],&prepped->axes[i],&prepped->BdA[i]);
        }

        cyl->prepped = prepped;
        cyl->prepared = 1;
    }

    pthread_mutex_unlock(&lock);

    return 1;
}

int cleanup(object *cyl) {
    if( cyl->prepared == 0 )
        return -1;

    prepped_t *prepped = cyl->prepped;
    int dim = cyl->dimensions;
    for(int i=0; i<dim-2; i++) {
        vectNd_free(&prepped->axes[i]);
    }
    free(prepped->axes); prepped->axes = NULL;
    free(prepped->lengths); prepped->lengths = NULL;
    free(prepped->AdA); prepped->AdA = NULL;
    free(prepped->BdA); prepped->BdA = NULL;
    return 0;
}

int type_name(char *name, int size) {
    strncpy(name,"hcylinder",size);
    return 0;
}

int params(object *obj, int *n_pos, int *n_dir, int *n_size, int *n_flags, int *n_obj) {
    /* report how many of each type of parameter is needed */
    *n_pos = obj->dimensions - 1;
    *n_dir = 0;
    *n_size = 1;
    *n_flags = 1;
    *n_obj = 0;

    return 0;
}

int get_bounds(object *obj) {
    int dim = obj->dimensions;

    double radius = obj->size[0];
    if( radius<=0 )
        fprintf(stderr,"Warning: hyper-cylinder with non-positive radius.\n");

    /* center is average of end points */

    /* sum all axis vectors */
    double max_length = 0;
    vectNd sums;
    vectNd diff;
    vectNd_calloc(&diff,dim);
    vectNd_calloc(&sums,dim);
    double sum_sq = 0.0;
    for(int i=0; i<dim-2; ++i) {
        double length;
        vectNd_sub(&obj->pos[i+1],&obj->pos[0],&diff);
        vectNd_l2norm(&diff,&length);
        sum_sq += (length/2.0)*(length/2.0);
        if( length>max_length )
            max_length = length;
        vectNd_add(&sums,&diff,&sums);
    }
    vectNd_free(&diff);

    /* divide sum by 2 and add to bottom point to get vector to centroid */
    vectNd_scale(&sums,0.5,&sums);
    vectNd_add(&obj->pos[0],&sums,&obj->bounds.center);
    vectNd_free(&sums);

    if( obj->flag[0] != 0 ) {
        /* half distance from centroid to 'end' point + radius */
        double b = obj->size[0];
        /* This is where the weird gaps were coming from,
         * The radius wasn't being taken into consideration. */
        obj->bounds.radius = sqrt(sum_sq+b*b) + EPSILON;
    } else {
        /* inifinite cylinder */
        obj->bounds.radius = -1;
    }

    return 1;
}

static int between_ends(object *cyl, vectNd *point) {
    int dim;
    dim  = point->n;

    /* check length of projection onto each axis against axis length */
    vectNd Bc;
    vectNd_alloc(&Bc,dim);
    vectNd_sub(point,&cyl->pos[0],&Bc);
    double scale;
    prepped_t* prepped = (prepped_t*)cyl->prepped;
    vectNd *axes = prepped->axes;
    double *AdAs = prepped->AdA;
    for(int i=0; i<dim-2; ++i) {
        vectNd_dot(&Bc,&axes[i],&scale);
        scale = scale / AdAs[i];

        if( scale < -EPSILON || scale > prepped->lengths[i]+EPSILON ) {
            vectNd_free(&Bc);
            return 0;
        }
    }
    vectNd_free(&Bc);
    
    return 1;   /* didn't violate any constraints */
}

int intersect(object *cyl, vectNd *o, vectNd *v, vectNd *res, vectNd *normal, object **ptr)
{
    if( !cyl->prepared ) {
        prepare(cyl);
    }

    int ret = 0;
    prepped_t *prepped = (prepped_t*)cyl->prepped;
    int dim = cyl->dimensions;

    /* allocate all intermediate vectors */
    vectNd *axes = prepped->axes;
    vectNd *pos0 = &cyl->pos[0];
    double radius = cyl->size[0];
    double VdA, AdA;
    double OdA, BdA;
    double qa, qb, qc;
    double det, detRoot;
    double t1, t2;
    vectNd P;
    vectNd Q;
    vectNd sA;
    vectNd sum_A;
    vectNd_alloc(&P,dim);
    vectNd_alloc(&Q,dim);
    vectNd_alloc(&sA,dim);
    vectNd_calloc(&sum_A,dim);

    /* sum over all basis vectors */
    for(int i=0; i<dim-2; ++i) {
        AdA = prepped->AdA[i];
        vectNd_dot(v,&axes[i],&VdA);
        vectNd_scale(&axes[i],VdA/AdA,&sA);
        vectNd_add(&sum_A,&sA,&sum_A);
    }
    vectNd_sub(&sum_A,v,&P);

    vectNd_reset(&sum_A);
    for(int i=0; i<dim-2; ++i) {
        BdA = prepped->BdA[i];
        AdA = prepped->AdA[i];
        vectNd_dot(o,&axes[i],&OdA);
        vectNd_scale(&axes[i],(OdA-BdA)/AdA,&sA);
        vectNd_add(&sum_A,&sA,&sum_A);
    }
    vectNd_sub(pos0,o,&Q);
    vectNd_add(&Q,&sum_A,&Q);

    /* solve quadratic */
    vectNd_dot(&P,&P,&qa);
    vectNd_dot(&P,&Q,&qb);
    qb *= 2;    /* FOILed again! */
    vectNd_dot(&Q,&Q,&qc);
    qc -= radius*radius;

    /* solve for t */
    det = qb*qb - 4*qa*qc;
    if( det < 0.0 ) {
        vectNd_free(&Q);
        vectNd_free(&P);
        vectNd_free(&sum_A);
        vectNd_free(&sA);
        return 0;
    }
    detRoot = sqrt(det);
    t1 = (-qb + detRoot) / (2*qa);
    t2 = (-qb - detRoot) / (2*qa);

    /* pick which (if any) point to return */
    if( t2>EPSILON ) {
        vectNd_scale(v,t2,&sA);
        vectNd_add(o,&sA,res);

        /* do end test */
        if( between_ends(cyl, res) )
            ret = 1;
    }

    if( ret==0 && t1>EPSILON ) {
        vectNd_scale(v,t1,&sA);
        vectNd_add(o,&sA,res);

        /* do end test */
        if( between_ends(cyl, res) )
            ret = 1;
    }

    /* find normal */
    if( ret != 0 ) {
        /* get vector from bottom point to intersection point */
        vectNd_sub(res,pos0,&P);

        /* get sum of P projected onto each of the axes */
        vectNd_reset(&Q);
        for(int i=0; i<dim-2; ++i) {
            vectNd_proj(&P,&axes[i],&sA);
            vectNd_add(&Q,&sA,&Q);
        }

        /* get vector from nearest axis point to intersection */
        vectNd_sub(&P,&Q,normal);

        if( ptr != NULL )
            *ptr = cyl;
    }

    vectNd_free(&Q);
    vectNd_free(&P);
    vectNd_free(&sum_A);
    vectNd_free(&sA);

    return ret;
}
