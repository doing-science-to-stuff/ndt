/*
 * facet.c
 * ndt: n-dimensional tracer
 *
 * Copyright (c) 2019 Bryan Franklin. All rights reserved.
 */
#include <stdio.h>
#include <pthread.h>
#include "../matrix.h"
#include "../vectNd.h"
#include "object.h"

static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;

typedef struct prepared_data {
    /* data that is ray invariant and can be pre-computed in prepare function */
    vectNd edge[3];
    vectNd unit_edge[3];
    double length[3];    /* length of each 'edge' */
    double v0_angle;    /* interior angle at vertex 2 */
    double angle[3];    /* angle at each vertex */
    vectNd basis[2];    /* othogonal basis vectors for plane */
} prepped_t;

int cleanup(object *face) {
    if( face->prepared == 0 )
        return -1;

    prepped_t *prepped = face->prepped;

    for(int i=0; i<3; ++i) {
        vectNd_free(&prepped->edge[i]);
        vectNd_free(&prepped->unit_edge[i]);
    }

    vectNd_free(&prepped->basis[0]);
    vectNd_free(&prepped->basis[1]);

    return 0;
}

static int facet_prepare(object *face) {
    pthread_mutex_lock(&lock);

    if( !face->prepared ) {
        int p = 3;
        int d = face->dimensions;
        int i = 0;

        prepped_t *prepped = calloc(1,sizeof(prepped_t));

        /* compute edge vectors, lengths, and normalized versions */
        for(i=0; i<p; ++i) {
            int j = (i+1)%p;
            int k = (i+2)%p;
            vectNd_alloc(&prepped->edge[i],d);
            vectNd_alloc(&prepped->unit_edge[i],d);

            vectNd_sub(&face->pos[j],&face->pos[i],&prepped->edge[i]);
            vectNd_copy(&prepped->unit_edge[i], &prepped->edge[i]);
            vectNd_unitize(&prepped->unit_edge[i]);

            vectNd_angle3(&face->pos[k],&face->pos[i],&face->pos[j],&prepped->angle[i]);
        }

        /* compute basis vectors for plane */
        vectNd_calloc(&prepped->basis[0],d);
        vectNd_calloc(&prepped->basis[1],d);
        vectNd_orthogonalize(&prepped->edge[0],&prepped->edge[1],
                &prepped->basis[0],&prepped->basis[1]);

        /* get angle at vertex 0 */
        vectNd_angle3(&face->pos[2],&face->pos[0],&face->pos[1],
                &prepped->v0_angle);

        face->prepped = prepped;
        face->prepared = 1;
    }

    pthread_mutex_unlock(&lock);

    return 1;
}

int type_name(char *name, int size) {
    strncpy(name,"facet",size);
    return 0;
}

int params(object *obj, int *n_pos, int *n_dir, int *n_size, int *n_flags, int *n_obj) {
    /* report how many of each type of parameter is needed */
    *n_pos = 3;
    *n_dir = 3;
    *n_size = 0;
    *n_flags = 1;
    *n_obj = 0;

    return 0;
}

int bounding_points(object *obj, bounds_list *list) {
    for(int i=0; i<obj->n_pos; ++i) {
        bounds_list_add(list, &obj->pos[i], 0.0);
    }

	return 1;
}

int facet_set_vertices(object *face, vectNd *points)
{
    int i=0;

    for(i=0; i<3; ++i) {
        object_add_pos(face, &points[i]);
    }

	/* set invariants */
	facet_prepare(face);

    return 0;
}

int facet_set_normal(object *face, vectNd *normals)
{
    face->n_dir = 0;
    object_add_dir(face, &normals[0]);
    face->flag[0] = 0; /* use_normals */

    return 0;
}

int facet_set_normals(object *face, vectNd *normals)
{
    int i=0;

    face->n_dir = 0;
    for(i=0; i<3; ++i) {
        object_add_dir(face, &normals[i]);
    }
    face->flag[0] = 0; /* use_normals */

    return 0;
}


static int inside_edges(object *face, vectNd *res)
{
    prepped_t *prepped = face->prepped;
    double *angles = prepped->angle;
    vectNd *pos = face->pos;
    double angle;
    for(int i=0; i<3; ++i) {
        int j=(i+1)%3;
        
        vectNd_angle3(res,&pos[i],&pos[j],&angle);
        if( angle > angles[i] )
            return 0;
    }

    return 1;
}

int intersect(object *face, vectNd *o, vectNd *v, vectNd *res, vectNd *normal, object **ptr)
{
    if( !face->prepared ) {
        facet_prepare(face);
    }

    int ret = 0;
    int dim = o->n;
    prepped_t *prepped = face->prepped;

    /* sum over all basis vectors */
    double VdA, AdA;
    double OdA, BdA;
    vectNd *pos1 = &face->pos[1];
    vectNd P;
    vectNd sA;
    vectNd sum_A;
    vectNd Q;
    vectNd_alloc(&Q,dim);
    vectNd_alloc(&P,dim);
    vectNd_alloc(&sA,dim);
    vectNd_calloc(&sum_A,dim);
    for(int i=0; i<2; ++i) {
        vectNd_dot(v,&prepped->basis[i],&VdA);
        vectNd_dot(&prepped->basis[i],&prepped->basis[i],&AdA);
        vectNd_scale(&prepped->basis[i],VdA/AdA,&sA);
        vectNd_add(&sum_A,&sA,&sum_A);
    }
    vectNd_sub(&sum_A,v,&P);

    vectNd_reset(&sum_A);
    for(int i=0; i<2; ++i) {
        vectNd_dot(o,&prepped->basis[i],&OdA);
        vectNd_dot(pos1,&prepped->basis[i],&BdA);
        vectNd_dot(&prepped->basis[i],&prepped->basis[i],&AdA);
        vectNd_scale(&prepped->basis[i],(OdA-BdA)/AdA,&sA);
        vectNd_add(&sum_A,&sA,&sum_A);
    }
    vectNd_sub(pos1,o,&Q);
    vectNd_add(&Q,&sum_A,&Q);

    /* solve quadratic */
    double qa, qb, qc;
    vectNd_dot(&P,&P,&qa);
    vectNd_dot(&P,&Q,&qb);
    qb *= 2;    /* FOILed again! */
    vectNd_dot(&Q,&Q,&qc);

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
        vectNd_free(&sum_A);
        vectNd_free(&P);
        vectNd_free(&Q);
        vectNd_free(&sA);
        return 0;
    }

    double dist = qa*t*t + qb*t + qc;
    if( fabs(dist) > EPSILON ) {
        /* closest point is too far from surface */
        vectNd_free(&sum_A);
        vectNd_free(&P);
        vectNd_free(&Q);
        vectNd_free(&sA);
        return 0;
    }

    /* find intersection point */
    vectNd_scale(v,t,&sA);
    vectNd_add(o,&sA,res);

    /* do end test */
    if( inside_edges(face, res) )
        ret = 1;

    /* fill in the normal at the intersection point */
    vectNd_copy(normal,&face->dir[0]);

    vectNd_free(&sum_A);
    vectNd_free(&P);
    vectNd_free(&Q);
    vectNd_free(&sA);

    if( ret && ptr ) {
        *ptr = face;
    }

    return ret;
}
