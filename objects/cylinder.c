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
    double AdA;
    double BdA;
} prepped_t;

static int prepare(object *cyl) {
    pthread_mutex_lock(&lock);

    /* fill in any ray invariant parameters */
    if( !cyl->prepared ) {
        prepped_t *prepped = calloc(1,sizeof(prepped_t));
        vectNd_alloc(&prepped->axis,cyl->dimensions);
        vectNd_sub(&cyl->pos[1],&cyl->pos[0],&prepped->axis);
        vectNd_unitize(&prepped->axis);
        vectNd_dist(&cyl->pos[1],&cyl->pos[0],&prepped->length);
        vectNd_dot(&prepped->axis,&prepped->axis,&prepped->AdA);
        vectNd_dot(&cyl->pos[0],&prepped->axis,&prepped->BdA);
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
    vectNd_free(&prepped->axis);

    return 0;
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

int bounding_points(object *obj, bounds_list *list) {
    if( obj->flag[0] != 0 ) {
        /* add both ends with radius */
        bounds_list_add(list, &obj->pos[0], obj->size[0]);
        bounds_list_add(list, &obj->pos[1], obj->size[0]);
    } else {
        /* leave list empty for infinite cylinder */
    }

    return 1;
}

static int between_ends(object *cyl, vectNd *point) {
    double scale;
    prepped_t *prepped = cyl->prepped;
    vectNd Bc;
    vectNd_alloc(&Bc,point->n);
    vectNd_sub(point,&cyl->pos[0],&Bc);
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

    if( !cyl->prepared ) {
        prepare(cyl);
    }

    /* allocate additional intermediate vectors needed */
    prepped_t *prepped = (prepped_t*)cyl->prepped;
    dim = o->n;
    vectNd *Be = &cyl->pos[0];
    vectNd *A = &prepped->axis;
    double size0 = cyl->size[0];
    vectNd sA;
    vectNd X;
    vectNd Y;
    vectNd tmp;
    vectNd_alloc(&sA,dim);
    vectNd_alloc(&X,dim);
    vectNd_alloc(&Y,dim);
    vectNd_alloc(&tmp,dim);

    /* lots of initial dot products */
    double VdA;
    double AdA = prepped->AdA;
    double OdA;
    double BdA = prepped->BdA;
    double Vaaa;
    double BOaa;
    vectNd_dot(v,A,&VdA);
    vectNd_dot(o,A,&OdA);
    Vaaa = VdA/AdA;
    BOaa = (BdA-OdA)/AdA;

    /* more vector math */
    vectNd_scale(A,Vaaa,&sA);
    vectNd_sub(v,&sA,&Y);

    vectNd_sub(o,Be,&tmp);
    vectNd_scale(A,BOaa,&sA);
    vectNd_add(&tmp,&sA,&X);

    /* solve quadratic */
    double qa, qb, qc;
    double det, detRoot;
    double t1, t2;
    vectNd_dot(&Y,&Y,&qa);
    vectNd_dot(&Y,&X,&qb);
    qb *= 2;    /* FOILed again! */
    vectNd_dot(&X,&X,&qc);
    qc -= size0 * size0;

    /* free intermediate results that are no longer needed */

    /* solve for t */
    det = qb*qb - 4*qa*qc;
    if( det <= 0 ) {
        vectNd_free(&tmp);
        vectNd_free(&Y);
        vectNd_free(&X);
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
        double nCdA;
        vectNd_sub(res,Be,&X);
        vectNd_dot(A,&X,&nCdA);
        vectNd_scale(A,nCdA/AdA,&Y);
        vectNd_sub(&X,&Y,normal);
    }

    vectNd_free(&tmp);
    vectNd_free(&Y);
    vectNd_free(&X);
    vectNd_free(&sA);

    if( ret )
        *ptr = cyl;

    return ret;
}
