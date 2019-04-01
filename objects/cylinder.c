/*
 * cylinder.c
 * ndt: n-dimensional tracer
 *
 * Copyright (c) 2019 Bryan Franklin. All rights reserved.
 */
#include <stdio.h>
#include <math.h>
#include <pthread.h>
#include "object.h"

static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;

typedef struct prepared_data {
    /* data that is ray invariant and can be pre-computed in prepare function */
    vectNd axis;
    double length;
} prepped_t;

static int prepare(object *cyl) {
    pthread_mutex_lock(&lock);

    /* fill in any ray invarient parameters */
    if( !cyl->prepared ) {
        prepped_t *prepped = calloc(1,sizeof(prepped_t));
        vectNd_alloc(&prepped->axis,cyl->dimensions);
        vectNd_sub(&cyl->pos[1],&cyl->pos[0],&prepped->axis);
        vectNd_unitize(&prepped->axis);
        vectNd_dist(&cyl->pos[1],&cyl->pos[0],&prepped->length);
        cyl->prepped = prepped;
        cyl->prepared = 1;
    }

    pthread_mutex_unlock(&lock);

    return 1;
}

int type_name(char *name, int size) {
    strncpy(name,"cylinder",size);
    return 0;
}

int params(object *obj, int *n_pos, int *n_dir, int *n_size, int *n_flags, int *n_obj) {
    /* report how many of each type of parameter is needed */
    *n_pos = 2;
    *n_dir = 0;
    *n_size = 1;
    *n_flags = 1;
    *n_obj = 0;

    return 0;
}

int get_bounds(object *obj) {
    /* center is average of end points */
    vectNd_alloc(&obj->bounds.center,obj->dimensions);
    vectNd_add(&obj->pos[1],&obj->pos[0],&obj->bounds.center);
    vectNd_scale(&obj->bounds.center,0.5,&obj->bounds.center);

    if( obj->flag[0] != 0 ) {
        /* radius is half length + radius (just to be sure) */
        vectNd_dist(&obj->pos[1],&obj->pos[0],&obj->bounds.radius);
        obj->bounds.radius /= 2;
        obj->bounds.radius += obj->size[0]+EPSILON;
    } else {
        /* inifinite cylinder */
        obj->bounds.radius = -1;
    }

    return 1;
}

static int between_ends(object *cyl, vectNd *point) {
    vectNd Bc;
    vectNd_alloc(&Bc,point->n);
    vectNd_sub(point,&cyl->pos[0],&Bc);
    double scale;
    prepped_t *prepped = cyl->prepped;
    vectNd_dot(&Bc,&prepped->axis,&scale);
    vectNd_free(&Bc);

    if( scale > 0 && scale < prepped->length )
        return 1;
    
    return 0;
}

int intersect(object *cyl, vectNd *o, vectNd *v, vectNd *res, vectNd *normal, object **ptr)
{
    int ret = 0;
    int dim = 0;
    vectNd Be;
    vectNd A;

    if( !cyl->prepared ) {
        prepare(cyl);
    }

    /* additional initial vectors needed */
    dim = o->n;
    vectNd_alloc(&Be,dim);
    vectNd_copy(&Be,&cyl->pos[0]);
    vectNd_alloc(&A,dim);
    vectNd_copy(&A,&((prepped_t*)cyl->prepped)->axis);

    /* lots of initial dot products */
    double VdA;
    double AdA;
    double OdA;
    double BdA;
    vectNd_dot(v,&A,&VdA);
    vectNd_dot(&A,&A,&AdA);
    vectNd_dot(o,&A,&OdA);
    vectNd_dot(&Be,&A,&BdA);
    double Vaaa;
    double BOaa;
    Vaaa = VdA/AdA;
    BOaa = (BdA-OdA)/AdA;

    /* more vector math */
    vectNd Y;
    vectNd sA;
    vectNd_alloc(&Y,dim);
    vectNd_alloc(&sA,dim);
    vectNd_scale(&A,Vaaa,&sA);
    vectNd_sub(v,&sA,&Y);

    vectNd X;
    vectNd_alloc(&X,dim);
    vectNd tmp;
    vectNd_alloc(&tmp,dim);
    vectNd_sub(o,&Be,&tmp);
    vectNd_scale(&A,BOaa,&sA);
    vectNd_add(&tmp,&sA,&X);

    /* solve quadratic */
    double qa, qb, qc;
    double det, detRoot;
    double t1, t2;
    vectNd_dot(&Y,&Y,&qa);
    vectNd_dot(&Y,&X,&qb);
    qb *= 2;    /* FOILed again! */
    vectNd_dot(&X,&X,&qc);
    qc -= cyl->size[0]*cyl->size[0];

    /* solve for t */
    det = qb*qb - 4*qa*qc;
    if( det <= 0 )
        return 0;
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
        vectNd nC;
        vectNd_alloc(&nC,dim);
        vectNd_sub(res,&cyl->pos[0],&nC);
        double nCdA;
        vectNd_dot(&A,&nC,&nCdA);
        vectNd nB;
        vectNd_alloc(&nB,dim);
        vectNd_scale(&A,nCdA/AdA,&nB);
        vectNd_sub(&nC,&nB,normal);
    }

    if( ret )
        *ptr = cyl;

    return ret;
}
