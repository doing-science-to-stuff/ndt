/*
 * nelder-mead.c
 * ndt: n-dimensional tracer
 *
 * Copyright (c) 2018-2019 Bryan Franklin. All rights reserved.
 */

#include <float.h>
#include <stdio.h>
#include "vectNd.h"

/* ---------------------------- */

typedef struct NMSample {
    vectNd parameters;
    double value;
} NMSample;

void nmSampleCopy(NMSample *dst, NMSample *src) {
    vectNd_copy(&dst->parameters, &src->parameters);
    dst->value = src->value;
}

typedef struct NMSimplex {
    NMSample *points;
    int count;
} NMSimplex;

void nmSimplexInit(NMSimplex *simplex, int dimensions) {
    memset(simplex, '\0', sizeof(*simplex));
    simplex->points = calloc(dimensions+1, sizeof(NMSample));
}

void nmSimplexFree(NMSimplex *simplex) {
    for(int i=0; i<simplex->count; ++i) {
        vectNd_free(&simplex->points[i].parameters);
    }
    free(simplex->points); simplex->points=NULL;
    simplex->count = 0;
}

void nmSimplexPrint(NMSimplex *simplex) {
    printf("simplex:\n");
    for(int i=0; i<simplex->count; ++i) {
        printf("\tvalue=%g; ", simplex->points[i].value);
        vectNd_print(&simplex->points[i].parameters, "parameters");
    }
}

void nmSimplexAdd(NMSimplex *simplex, NMSample *sample) {
    vectNd_calloc(&simplex->points[simplex->count].parameters, sample->parameters.n);
    nmSampleCopy(&simplex->points[simplex->count], sample);
    simplex->count += 1;
}

void nmSimplexSort(NMSimplex *simplex) {
    if( simplex->count <= 1 )
        return;

    int done = 0;
    NMSample *points = simplex->points;
    NMSample tmp;
    vectNd_calloc(&tmp.parameters, points[0].parameters.n);
    while( !done ) {
        done = 1;
        for(int i=simplex->count-1; i>0; --i) {
            if( points[i-1].value > points[i].value ) {
                /* swap points i and i-1 */
                nmSampleCopy(&tmp, &points[i-1]);
                nmSampleCopy(&points[i-1], &points[i]);
                nmSampleCopy(&points[i], &tmp);

                done = 0;
            }
        }
    }
    vectNd_free(&tmp.parameters);
}

/* ---------------------------- */

enum NMState {
    initial, reflect, expand, contract_out, contract_in, shrink, shrink2
};

/* see: http://www.scholarpedia.org/article/Nelder-Mead_algorithm */
typedef struct NelderMead {
    /* maintain search state */
    int dimensions;
    int iterations;
    NMSimplex simplex;
    vectNd seed;
    enum NMState state;

    /* sample points */
    NMSample x_r;
    NMSample x_e;
    NMSample x_c;
    vectNd s_shrink;

    /* hyper-parameters */
    double alpha;
    double beta;
    double gamma;
    double delta;
} NelderMead;

void nm_init(void **nm_ptr, int dimensions) {
    NelderMead *nm = calloc(1, sizeof(NelderMead));
    *nm_ptr = nm;
    memset(nm, '\0', sizeof(*nm));

    nm->dimensions = dimensions;
    nm->iterations = 0;
    nm->state = initial;

    nm->alpha = 1;      /* \alpha > 0 */
    nm->beta = 0.5;     /* 0 < \beta < 1 */
    nm->gamma = 2;      /* \gamma > 1 */
    nm->delta = 0.5;    /* 0 < \delta < 1 */

    vectNd_calloc(&nm->seed, dimensions);
    vectNd_calloc(&nm->x_r.parameters, dimensions);
    vectNd_calloc(&nm->x_e.parameters, dimensions);
    vectNd_calloc(&nm->x_c.parameters, dimensions);
    vectNd_calloc(&nm->s_shrink, dimensions);

    nmSimplexInit(&nm->simplex, dimensions);
}

void nm_free(void *nm_ptr) {
    NelderMead *nm = nm_ptr;

    vectNd_free(&nm->seed);
    vectNd_free(&nm->x_r.parameters);
    vectNd_free(&nm->x_e.parameters);
    vectNd_free(&nm->x_c.parameters);
    vectNd_free(&nm->s_shrink);

    nmSimplexFree(&nm->simplex);

    free(nm); nm=NULL;
}

void nm_set_seed(void *nm_ptr, vectNd *seed) {
    NelderMead *nm = nm_ptr;
    if( nm->state != initial ) { return; }
    vectNd_copy(&nm->seed, seed);
}

void nm_best_point(void *nm_ptr, vectNd *result) {
    NelderMead *nm = nm_ptr;

    int best = 0;
    double min = nm->simplex.points[best].value;
    for(int i=0; i<nm->simplex.count; ++i) {
        if( nm->simplex.points[i].value < min ) {
            min = nm->simplex.points[i].value;
            best = i;
        }
    }

    if( best < nm->simplex.count )
        vectNd_copy(result, &nm->simplex.points[best].parameters);
}

void nm_add_result(void *nm_ptr, vectNd *parameters, double value) {
    NelderMead *nm = nm_ptr;

    nm->iterations += 1;

    NMSample newSample;
    vectNd_calloc(&newSample.parameters, parameters->n);
    vectNd_copy(&newSample.parameters, parameters);
    newSample.value = value;

    /* shrink just replaces points, but needs the associated values */
    if( nm->state == shrink2 ) {
        nmSampleCopy(&nm->simplex.points[nm->simplex.count-2], &newSample);
        nm->state = reflect;
        vectNd_free(&newSample.parameters);
        return;
    } else if( nm->state == shrink ) {
        nmSampleCopy(&nm->simplex.points[nm->simplex.count-1], &newSample);
        nm->state = shrink2;
        vectNd_free(&newSample.parameters);
        return;
    }

    /* check for initialization state */
    if( nm->simplex.count <= nm->dimensions ) {
        nmSimplexAdd(&nm->simplex, &newSample);
        if( nm->simplex.count >= nm->dimensions+1 )
            nm->state = reflect;
        return;
    }

    /* sort simple */
    if( nm->state != shrink && nm->state != shrink2 )
        nmSimplexSort(&nm->simplex);

    /* get h, s, l */
    NMSample h; /* simplex[simplex.count-1] */
    NMSample s; /* simplex[simplex.count-2] */
    NMSample l; /* simplex[0] */
    NMSample r; /* newSample */
    vectNd_calloc(&h.parameters, nm->dimensions);
    vectNd_calloc(&s.parameters, nm->dimensions);
    vectNd_calloc(&l.parameters, nm->dimensions);
    vectNd_calloc(&r.parameters, nm->dimensions);
    nmSampleCopy(&h, &nm->simplex.points[nm->simplex.count-1]);
    nmSampleCopy(&s, &nm->simplex.points[nm->simplex.count-2]);
    nmSampleCopy(&l, &nm->simplex.points[0]);
    nmSampleCopy(&r, &newSample);
    vectNd_free(&newSample.parameters);

    /* actual parameters can be discarded,
     * as only the output values are needed here */
    vectNd_free(&h.parameters);
    vectNd_free(&s.parameters);
    vectNd_free(&l.parameters);

    #if 0
    printf("f(h) = %g\n", h.value);
    printf("f(s) = %g\n", s.value);
    printf("f(l) = %g\n", l.value);
    printf("f(r) = %g\n", r.value);
    #endif /* 0 */

    /* deal with recently computed point based on state */
    if( nm->state == reflect ) {
        nmSampleCopy(&nm->x_r, &r);

        if( l.value <= nm->x_r.value && nm->x_r.value < s.value ) {
            /* accept x_r and terminate iteration */
            nmSampleCopy(&nm->simplex.points[nm->simplex.count-1], &r);
            vectNd_free(&r.parameters);
            return;
        }
    }

    if( nm->state == expand ) {
        nmSampleCopy(&nm->x_e, &r);

        if( nm->x_e.value < nm->x_r.value ) {
            /* accept x_e and terminate iteration */
            nmSampleCopy(&nm->simplex.points[nm->simplex.count-1], &nm->x_e);
            nm->state = reflect;
            vectNd_free(&r.parameters);
            return;
        }

        /* accept x_r and terminate iteration */
        nmSampleCopy(&nm->simplex.points[nm->simplex.count-1], &nm->x_e);
        nm->state = reflect;
        vectNd_free(&r.parameters);
        return;
    }

    if( nm->state == contract_out ) {
        nmSampleCopy(&nm->x_c, &r);

        if( nm->x_c.value < nm->x_r.value ) {
            /* accept x_c and terminate iteration */
            nmSampleCopy(&nm->simplex.points[nm->simplex.count-1], &nm->x_c);
            nm->state = reflect;
            vectNd_free(&r.parameters);
            return;
        }
    }

    if( nm->state == contract_in ) {
        nmSampleCopy(&nm->x_c, &r);

        if( nm->x_c.value < h.value ) {
            /* accept x_c and terminate iteration */
            nmSampleCopy(&nm->simplex.points[nm->simplex.count-1], &nm->x_c);
            nm->state = reflect;
            vectNd_free(&r.parameters);
            return;
        }
    }
    vectNd_free(&r.parameters);

    /* determine next state if new point not accepted */
    if( r.value < l.value ) {
        /* cause x_e to be computed next */
        nm->state = expand;
        return;
    } else if( r.value >= s.value ) {
        /* cause x_c to be computed next */
        if( s.value <= r.value && r.value < h.value )
            nm->state = contract_out;
        else
            nm->state = contract_in;
        return;
    }

    nm->state = shrink;
    return;
}

void nm_next_point(void *nm_ptr, vectNd *vector) {
    NelderMead *nm = nm_ptr;

    if( nm->state == initial && nm->simplex.count < nm->dimensions+1 ) {
        /* add new initial point */
        if( nm->simplex.count > 0 ) {
            int pos = nm->simplex.count-1;
            vectNd_copy(vector, &nm->seed);
            vector->v[pos] += nm->simplex.count;
        } else if( nm->seed.n == vector->n ) {
            vectNd_copy(vector, &nm->seed);
        } else {
            vectNd_copy(&nm->seed, vector);
        }
        return;
    }

    /* check to see that we have correct number of points */
    if( nm->simplex.count != nm->dimensions+1 ) {
        vectNd_copy(vector, &nm->seed);
        return;
    }

    /* sort simplex */
    if( nm->state != shrink && nm->state != shrink2 )
        nmSimplexSort(&nm->simplex);

    /* get h, s, l */
    NMSample h; /* simplex[simplex.count-1] */
    NMSample s; /* simplex[simplex.count-2] */
    NMSample l; /* simplex[0] */
    vectNd_calloc(&h.parameters, nm->dimensions);
    vectNd_calloc(&s.parameters, nm->dimensions);
    vectNd_calloc(&l.parameters, nm->dimensions);
    nmSampleCopy(&h, &nm->simplex.points[nm->simplex.count-1]);
    nmSampleCopy(&s, &nm->simplex.points[nm->simplex.count-2]);
    nmSampleCopy(&l, &nm->simplex.points[0]);

    /* compute centroid */
    vectNd c, sumVect;
    vectNd_calloc(&c, nm->dimensions);
    vectNd_calloc(&sumVect, nm->dimensions);
    for(int i=0; i<nm->simplex.count-1; ++i) {
        vectNd_add(&sumVect, &nm->simplex.points[i].parameters, &sumVect);
    }
    vectNd_scale(&sumVect, 1.0/(nm->simplex.count-1), &c);
    vectNd_free(&sumVect);

    /* add appropriate new point(s) */
    vectNd scaled, tmp;
    vectNd_calloc(&scaled, nm->dimensions);
    vectNd_calloc(&tmp, nm->dimensions);
    switch( nm->state ) {
        case initial:
            /* initial is handled above */
            printf("This should never happen!\n");
            break;
        case reflect:
            vectNd_sub(&c, &h.parameters, &tmp);
            vectNd_scale(&tmp, nm->alpha, &scaled);
            vectNd_add(&c, &scaled, vector);    /* x_r */
            break;
        case expand:
            vectNd_sub(&nm->x_r.parameters, &c, &tmp);
            vectNd_scale(&tmp, nm->gamma, &scaled);
            vectNd_add(&c, &scaled, vector);    /* x_e */
            break;
        case contract_out:
            vectNd_sub(&nm->x_r.parameters, &c, &tmp);
            vectNd_scale(&tmp, nm->beta, &scaled);
            vectNd_add(&c, &scaled, vector);    /* x_c */
            break;
        case contract_in:
            vectNd_sub(&h.parameters, &c, &tmp);
            vectNd_scale(&tmp, nm->beta, &scaled);
            vectNd_add(&c, &scaled, vector);    /* x_c */
            break;
        case shrink:
            /* store one shrink point for state shrink2 */
            vectNd_add(&nm->x_r.parameters, &s.parameters, &tmp);
            vectNd_scale(&tmp, 0.5, &nm->s_shrink); /* new s */

            /* return other shrink point */
            vectNd_add(&nm->x_r.parameters, &h.parameters, &tmp);
            vectNd_scale(&tmp, 0.5, vector);        /* new h */
            break;
        case shrink2:
            /* return second shrink point */
            vectNd_copy(vector, &nm->s_shrink);
            vectNd_reset(&nm->s_shrink);
            break;
    }
    vectNd_free(&tmp);
    vectNd_free(&scaled);
    vectNd_free(&c);
    vectNd_free(&h.parameters);
    vectNd_free(&s.parameters);
    vectNd_free(&l.parameters);

    return;
}

int nm_done(void *nm_ptr, double threshold, int iterations) {
    NelderMead *nm = nm_ptr;
    if( nm->state == initial )
        return 0;

    /* check maximum iterations */
    if( nm->iterations > iterations ) {
        return 1;
    }

    /* check distance between best and worst parameters */
    double dist;
    if( nm->state != shrink && nm->state != shrink2 )
        nmSimplexSort(&nm->simplex);
    vectNd_dist(&nm->simplex.points[0].parameters,
                &nm->simplex.points[nm->simplex.count-1].parameters, &dist);
    #if 0
    printf("iteration: %i; dist: %g\n", nm->iterations, dist);
    #endif /* 0 */
    if( dist < threshold ) {
        return 1;
    }

    return 0;
}
