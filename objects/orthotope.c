/*
 * orthotope.c
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
    vectNd *basis;
    double *lengths;
    double *BdP;
    double *BdB;
} prepped_t;

static int prepare(object *sub) {
    pthread_mutex_lock(&lock);

    /* fill in any ray invarient parameters */
    if( !sub->prepared ) {
        prepped_t *prepped = calloc(1,sizeof(prepped_t));

        int dim = sub->flag[0];
        prepped->basis = calloc(dim,sizeof(vectNd));
        prepped->lengths = calloc(dim,sizeof(double));
        prepped->BdP = calloc(dim,sizeof(double));
        prepped->BdB = calloc(dim,sizeof(double));
        for(int i=0; i<dim; i++) {
            /* get unitized basis vectors */
            vectNd_alloc(&prepped->basis[i],sub->dir[0].n);
            vectNd_copy(&prepped->basis[i],&sub->dir[i]);
            vectNd_unitize(&prepped->basis[i]);

            /* pre-compute lengths and dot products */
            vectNd_l2norm(&sub->dir[i], &prepped->lengths[i]);
            vectNd_dot(&prepped->basis[i], &prepped->basis[i], &prepped->BdB[i]);
            vectNd_dot(&sub->pos[0], &prepped->basis[i], &prepped->BdP[i]);
        }

        sub->prepped = prepped;
        sub->prepared = 1;
    }

    pthread_mutex_unlock(&lock);

    return 1;
}

int cleanup(object *sub) {
    if( sub->prepared == 0 )
        return -1;

    prepped_t *prepped = sub->prepped;
    int dim = sub->flag[0];
    for(int i=0; i<dim; i++) {
        vectNd_free(&prepped->basis[i]);
    }
    free(prepped->basis); prepped->basis = NULL;
    free(prepped->lengths); prepped->lengths = NULL;
    free(prepped->BdP); prepped->BdP = NULL;
    free(prepped->BdB); prepped->BdB = NULL;
    return 0;
}

int type_name(char *name, int size) {
    strncpy(name,"orthotope",size);
    return 0;
}

int params(object *obj, int *n_pos, int *n_dir, int *n_size, int *n_flags, int *n_obj) {
    /* report how many of each type of parameter is needed */
    *n_pos = 1;
    if( obj->n_flag > 0 )
        *n_dir = obj->flag[0];
    else
        *n_dir = 1;
    *n_size = 0;
    *n_flags = 1;
    *n_obj = 0;

    return 0;
}

int get_bounds(object *obj) {
    int dim = obj->dimensions;

    /* sum all axis vectors */
    vectNd sum;
    vectNd_calloc(&sum,dim);
    int i=0;
    for(i=0; i<obj->flag[0]; ++i) {
        vectNd_add(&sum,&obj->dir[i],&sum);
    }

    /* divide sum by 2 and add to bottom point to get vector to centroid */
    vectNd_scale(&sum,0.5,&sum);
    vectNd_add(&obj->pos[0],&sum,&obj->bounds.center);

    /* pos[0] is a corner and center is the midpoint of a diagonal */
    /* Note: this may only work when all basis vectors are the same length */
    vectNd_l2norm(&sum,&obj->bounds.radius);
    obj->bounds.radius += EPSILON;

    vectNd_free(&sum);

    return 1;
}

static int within_orthotope(object *sub, vectNd *point) {
    int dim;
    int i=0;
    dim  = point->n;

    /* check length of projection onto each axis against axis length */
    vectNd Bc;
    vectNd_alloc(&Bc,dim);
    vectNd_sub(point,&sub->pos[0],&Bc);
    double scale;
    double AdA;
    prepped_t* prepped = (prepped_t*)sub->prepped;
    vectNd *basis = prepped->basis;
    double *lengths = prepped->lengths;
    int n = sub->flag[0];
    double *BdBs = prepped->BdB;
    for(i=0; i<n; ++i) {
        /* get scaling of basis[i] needed to project (point - pos[0]) onto basis[i] */
        vectNd_dot(&Bc,&basis[i],&scale);
        scale = scale / BdBs[i];

        if( scale < -EPSILON || scale > lengths[i]+EPSILON ) {
            vectNd_free(&Bc);
            return 0;
        }
    }
    vectNd_free(&Bc);

    return 1;   /* didn't violate any constraints */
}

int intersect(object *sub, vectNd *o, vectNd *v, vectNd *res, vectNd *normal, object **ptr)
{
    int ret = 0;
    int i=0;

    if( !sub->prepared ) {
        prepare(sub);
    }

    prepped_t *prepped = (prepped_t*)sub->prepped;
    int dim = sub->dimensions;

    /* sum over all basis vectors */
    vectNd *basis = prepped->basis;
    vectNd P;
    vectNd_alloc(&P,dim);
    vectNd sum_A;
    vectNd_calloc(&sum_A,dim);
    double VdA, AdA;
    vectNd sA;
    vectNd_alloc(&sA,dim);
    vectNd_reset(&sum_A);
    for(i=0; i<sub->flag[0]; ++i) {
        vectNd_dot(v,&basis[i],&VdA);
        AdA = prepped->BdB[i];
        vectNd_scale(&basis[i],VdA/AdA,&sA);
        vectNd_add(&sum_A,&sA,&sum_A);
    }   /* sum_A is now \sum Y_i */
    vectNd_sub(&sum_A,v,&P);

    vectNd Q;
    vectNd_alloc(&Q,dim);
    double OdA, BdA;
    vectNd_reset(&sum_A);
    for(i=0; i<sub->flag[0]; ++i) {
        vectNd_dot(o,&basis[i],&OdA);
        BdA = prepped->BdP[i];
        AdA = prepped->BdB[i];
        vectNd_scale(&basis[i],(OdA-BdA)/AdA,&sA);
        vectNd_add(&sum_A,&sA,&sum_A);
    }   /* sum_A is now \sum X_i */
    vectNd_sub(&sub->pos[0],o,&Q);
    vectNd_add(&Q,&sum_A,&Q);
    vectNd_free(&sum_A);

    /* solve quadratic */
    double qa, qb, qc;
    vectNd_dot(&P,&P,&qa);
    vectNd_dot(&P,&Q,&qb);
    qb *= 2;    /* FOILed again! */
    vectNd_dot(&Q,&Q,&qc);
    vectNd_free(&P);
    vectNd_free(&Q);
    qc -= EPSILON;

    double t1, t2;
    double det, detRoot;
    /* solve for t */
    det = qb*qb - 4*qa*qc;
    if( det >= 0.0 && fabs(qa)>EPSILON ) {
        detRoot = sqrt(det);
        t1 = (-qb + detRoot) / (2*qa);
        t2 = (-qb - detRoot) / (2*qa);

        /* pick which (if any) point to return */
        if( t2>EPSILON ) {
            vectNd_scale(v,t2,&sA);
            vectNd_add(o,&sA,res);

            /* do end test */
            if( within_orthotope(sub, res) )
                ret = 1;
        }

        if( ret==0 && t1>EPSILON ) {
            vectNd_scale(v,t1,&sA);
            vectNd_add(o,&sA,res);

            /* do end test */
            if( within_orthotope(sub, res) )
                ret = 1;
        }
    }

    if( ret == 0 ) {
        double t=-1.0;
        /* find value of t where o+v*t is closest to plane */
        if( fabs(qa) < EPSILON ) {
            /* equation is essentially qb*t + qc = 0 */
            if( fabs(qb) < EPSILON )
                t = -qc / qb;
            else
                t = -1.0;
        }
        else
        {
            /* find minumum for qa*t^2 + qb*t + qc */
            /* find d/dt = 2*qa*t + qb = 0 */
            t = -qb / (2*qa);
        }
        if( t < EPSILON ) {
            /* hit is behind the viewer */
            vectNd_free(&sA);
            return 0;
        }

        double dist = qa*t*t + qb*t + qc;
        if( fabs(dist) > EPSILON ) {
            /* closest point is too far from surface */
            vectNd_free(&sA);
            return 0;
        }

        /* find intersection point */
        vectNd_scale(v,t,&sA);
        vectNd_add(o,&sA,res);

        /* do end test */
        if( within_orthotope(sub, res) )
            ret = 1;
    }
    vectNd_free(&sA);

    /* find normal */
    if( ret != 0 ) {
        vectNd nC;
        vectNd_alloc(&nC,dim);

        /* get vector from bottom point to intersection point */
        vectNd_sub(res,&sub->pos[0],&nC);

        /* get sum of nC projected onto each of the basis */
        vectNd nB;
        vectNd_calloc(&nB,dim);
        vectNd nCpAi;
        vectNd_calloc(&nCpAi,dim);
        for( i=0; i<sub->flag[0]; ++i) {
            vectNd_proj(&nC,&basis[i],&nCpAi);
            vectNd_add(&nB,&nCpAi,&nB);
        }
        vectNd_free(&nCpAi);

        /* get vector from nearest axis point to intersection */
        vectNd_sub(&nC,&nB,normal);
        vectNd_free(&nB);
        vectNd_free(&nC);

        if( ptr != NULL )
            *ptr = sub;
    }

    return ret;
}
