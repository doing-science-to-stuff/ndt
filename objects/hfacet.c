/*
 * hfacet.c
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
static int ones_set = 0;
static vectNd ones;

typedef struct prepared_data {
    /* data that is ray invariant and can be pre-computed in prepare function */
    vectNd edge[3];
    vectNd unit_edge[3];
    vectNd edge_perp;
    double length[3];    /* length of each 'edge' */
} prepped_t;

int cleanup(object *face) {
    if( face->prepared == 0 )
        return -1;

    prepped_t *prepped = face->prepped;

    for(int i=0; i<3; ++i) {
        vectNd_free(&prepped->edge[i]);
        vectNd_free(&prepped->unit_edge[i]);
    }
    vectNd_free(&prepped->edge_perp);

    return 0;
}

static int prepare(object *face) {
    pthread_mutex_lock(&lock);

    vectNd *vertex = face->pos;

    int dim = face->dimensions;
    int i;

    if( ones_set==0 ) {
        vectNd_alloc(&ones,dim);
        vectNd_fill(&ones,1.0);
        ones_set = 1;
    }

    /* fill in any ray invarient parameters */
    if( !face->prepared ) {
        prepped_t *prepped = calloc(1,sizeof(prepped_t));
        face->prepped = prepped;

        for(i=0; i<3; i++) {
            int j = (i+1)%3;
            vectNd_alloc(&prepped->edge[i],dim);
            vectNd_sub(&vertex[j],&vertex[i],&prepped->edge[i]);
            vectNd_l2norm(&prepped->edge[i],&prepped->length[i]);

            /* make unitized copies of edges */
            vectNd_alloc(&prepped->unit_edge[i],dim);
            vectNd_copy(&prepped->unit_edge[i],&prepped->edge[i]);
            vectNd_unitize(&prepped->unit_edge[i]);
        }
        
        /* reverse edge 2 for use in intersection code */
        vectNd_scale(&prepped->edge[2],-1.0,&prepped->edge[2]);
        vectNd_scale(&prepped->unit_edge[2],-1.0,&prepped->unit_edge[2]);

        /* find vector that is perpendicular to edge 0 */
        vectNd e2e0;
        vectNd_alloc(&e2e0,dim);
        vectNd_alloc(&prepped->edge_perp,dim);
        vectNd_proj(&prepped->edge[2],&prepped->edge[0],&e2e0);
        vectNd_sub(&prepped->edge[2],&e2e0,&prepped->edge_perp);
        vectNd_free(&e2e0);
        vectNd_unitize(&prepped->edge_perp);

        face->prepared = 1;
    }

    pthread_mutex_unlock(&lock);

    return 1;
}

int type_name(char *name, int size) {
    strncpy(name,"hfacet",size);
    return 0;
}

int params(object *obj, int *n_pos, int *n_dir, int *n_size, int *n_flags, int *n_obj) {
    *n_pos = 3;
    *n_dir = 3; /* 3 are needed when flag[0]==1, only 1 otherwise. */
    *n_size = 0;
    *n_flags = 1;
    *n_obj = 0;

    return 0;
}

int get_bounds(object *obj) {
    int dim = obj->dimensions;

    /* center is average of end points */
    vectNd_alloc(&obj->bounds.center,dim);

    /* average all vertices */
    vectNd sum;
    vectNd_calloc(&sum,dim);
    int i=0;
    vectNd *vertex = obj->pos;
    for(i=0; i<3; ++i) {
        vectNd_add(&sum,&vertex[i],&sum);
    }
    vectNd_scale(&sum,1.0/3.0,&obj->bounds.center);
    vectNd_free(&sum);

    double max_length = -1.0;
    for(i=0; i<3; ++i) {
        double len = 0;
        vectNd_dist(&obj->bounds.center,&vertex[i],&len);
        if( len > max_length )
            max_length = len;
    }
    obj->bounds.radius = max_length + EPSILON;

    obj->prepared = 0;

    return 1;
}

static int hfacet_point_in_plane(object *obj, vectNd *point, vectNd *on) {
    vectNd *vertex = obj->pos;
    vectNd *unit_edge = ((prepped_t*)obj->prepped)->unit_edge; 
    vectNd *edge_perp = &((prepped_t*)obj->prepped)->edge_perp;

    vectNd D;
    vectNd U, V;
    int dim = point->n;
    vectNd_alloc(&D, dim);
    vectNd_alloc(&U, dim);
    vectNd_alloc(&V, dim);

    vectNd_sub(point,&vertex[0],&D);
    vectNd_proj_unit(&D,&unit_edge[0],&U);
    vectNd_proj_unit(&D,edge_perp,&V);

    vectNd_add(&U,&V,on);
    vectNd_add(on,&vertex[0],on);

    vectNd_free(&D);
    vectNd_free(&U);
    vectNd_free(&V);
    
    return 0;
}

/* see: http://en.wikipedia.org/wiki/Barycentric_coordinate_system */
static inline int get_barycentric(object *face, vectNd *point, double *coords) {
    vectNd *A;
    vectNd *B;
    vectNd C;
    double xp, yp;
    double x1=0, y1=0;
    double x2, y2;
    double x3, y3;

    prepped_t *prepped = ((prepped_t*)face->prepped);
    vectNd *vertex = face->pos;

    vectNd_alloc(&C,point->n);

    /* convert point to 2D coordinates within face's plane */
    A = &prepped->unit_edge[0];
    B = &prepped->edge_perp;
    vectNd_sub(point,&vertex[0],&C);

    vectNd_dot(A,&C,&xp);
    vectNd_dot(B,&C,&yp);

    /* this needs non-unitized edges */
    vectNd_dot(A,&prepped->edge[0],&x2);
    vectNd_dot(B,&prepped->edge[0],&y2);

    /* this needs non-unitized edges */
    vectNd_dot(A,&prepped->edge[2],&x3);
    vectNd_dot(B,&prepped->edge[2],&y3);

    vectNd_free(&C);

    double l1, l2, l3;
    l1 = ((y2-y3)*(xp-x3) + (x3-x2)*(yp-y3)) /
         ((y2-y3)*(x1-x3) + (x3-x2)*(y1-y3));
    l2 = ((y3-y1)*(xp-x3) + (x1-x3)*(yp-y3)) /
         ((y2-y3)*(x1-x3) + (x3-x2)*(y1-y3));
    l3 = 1 - l1 - l2;

    coords[0] = l1;
    coords[1] = l2;
    coords[2] = l3;

    return 0;
}

static inline int inside_facet(object *face, vectNd *point, double *bary) {
    double lambda[3];
    int i=0;

    if( bary==NULL )
        bary = lambda;

    /* use barycentric coordinates to determine boundraries.
     * Slow but effective */
    get_barycentric(face,point,bary);

    for(i=0; i<3; ++i) {
        if( bary[i] < -EPSILON || bary[i] > 1+EPSILON )
            return 0;
    }

    return 1;   /* didn't violate any constraints */
}

int intersect(object *face, vectNd *o, vectNd *v, vectNd *res, vectNd *normal, object **ptr)
{
    int ret = 0;

    if( !face->prepared ) {
        prepare(face);
    }

    prepped_t *prepped = ((prepped_t*)face->prepped);
    vectNd *vertex = face->pos;

    int dim = face->dimensions;

    /* additional setup */
    double Qv, Rv;

    vectNd R;
    vectNd_alloc(&R,dim);
    vectNd vE0;
    vectNd_alloc(&vE0,dim);
    vectNd vE2;
    vectNd_alloc(&vE2,dim);
    vectNd Q;
    vectNd_alloc(&Q,dim);
    vectNd oP0;
    vectNd_alloc(&oP0,dim);

    /* compute dependant terms */
    vectNd_proj_unit(v,&prepped->unit_edge[0],&vE0);
    vectNd_proj_unit(v,&prepped->edge_perp,&vE2);
    vectNd_add(&vE0,&vE2,&R);
    vectNd_sub(&R,v,&R);
    vectNd_dot(&R,&ones,&Rv);

    /* give up if divisor is too small (i.e. place is parallel to v) */
    if( fabs(Rv) < EPSILON ) {
        vectNd_free(&R);
        vectNd_free(&Q);
        vectNd_free(&oP0);
        vectNd_free(&vE0);
        vectNd_free(&vE2);
        return 0;
    }

    /* compute constant terms */
    vectNd_sub(o,&vertex[0],&oP0);
    vectNd_proj_unit(&oP0,&prepped->unit_edge[0],&vE0);
    vectNd_proj_unit(&oP0,&prepped->edge_perp,&vE2);
    vectNd_add(&vE0,&vE2,&Q);
    vectNd_sub(&Q,&oP0,&Q);
    vectNd_dot(&Q,&ones,&Qv);

    vectNd_free(&R);
    vectNd_free(&Q);
    vectNd_free(&oP0);
    vectNd_free(&vE0);
    vectNd_free(&vE2);

    /* solve for t */
    double t = -1;
    t = -Qv / Rv;

    /* pick which (if any) point to return */
    double lambda[3]; /* barycentric coordiantes */
    if( t>EPSILON ) {
        vectNd_scale(v,t,res);
        vectNd_add(o,res,res);

        /* do edges test */
        if( inside_facet(face, res, lambda) ) {
            ret = 1;
        }
    }

    /* find normal */
    if( ret != 0 ) {
        int use_normals = face->flag[0];
        if( use_normals ) {
            /* use weighted average of normal vectors */
            int i=0;
            vectNd *normals = face->dir;

            vectNd sN;
            vectNd_reset(normal);
            vectNd_alloc(&sN,dim);
            for(i=0; i<3; ++i) {
                vectNd_scale(&normals[i],lambda[i],&sN);
                vectNd_add(normal,&sN,normal);
            }
            vectNd_free(&sN);
        } else {
            vectNd P;
            vectNd_alloc(&P,dim);
            hfacet_point_in_plane(face,o,&P);

            /* normal is the direction of shortest distance from plane to
             * observer point  */
            vectNd_sub(o,&P,normal);
            vectNd_unitize(normal);

            vectNd_free(&P);
        }

        if( ptr != NULL )
            *ptr = face;
    }

    return ret;
}
