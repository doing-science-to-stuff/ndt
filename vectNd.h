/*
 * vectNd.h
 * ndt: n-dimensional tracer
 *
 * Copyright (c) 2019 Bryan Franklin. All rights reserved.
 */
#ifndef VECTND_H
#define VECTND_H
#include <math.h>
#if defined(__SSE__)
#include <xmmintrin.h>
#endif /* __SSE__ */
#include <string.h>

#define VECTND_SUCCESS 1
#define VECTND_FAIL 0

/* this needs to be an even number when using SSE */
#define VECTND_DEF_SIZE 4

#define rad2deg(x) ((x)*180.0/M_PI)
#define deg2rad(x) ((x)*M_PI/180.0)

#if defined(__SSE__) && !defined(WITHOUT_SSE)
/* typecast v to __m128d* */
#define vectNd_SSE(x)  ((__m128d*)((x)->v))
#else
#warning "Not using SSE"
#endif /* __SSE__ */

typedef struct vectNd_t
{
    double space[VECTND_DEF_SIZE];
    double *v;
    int n;
} __attribute__((__aligned__(16))) vectNd;

int vectNd_fill(vectNd *v, double val);
int vectNd_get(vectNd *v, int pos, double *val);
int vectNd_set(vectNd *v, int pos, double val);
int vectNd_setStr(vectNd *v, char *str);

#ifdef WITHOUT_INLINE
int vectNd_alloc(vectNd *v, int dim);
int vectNd_calloc(vectNd *v, int dim);
int vectNd_free(vectNd *v);
int vectNd_reset(vectNd *v);

void vectNd_dot(vectNd *v1, vectNd *v2, double *res);
void vectNd_sub(vectNd *v1, vectNd *v2, vectNd *res);
void vectNd_add(vectNd *v1, vectNd *v2, vectNd *res);
void vectNd_scale(vectNd *v, double s, vectNd *res);
void vectNd_dist(vectNd *v1, vectNd *v2, double *res);
void vectNd_l2norm(vectNd *v, double *res);
void vectNd_unitize(vectNd *v);
void vectNd_copy(vectNd *dst, vectNd *src);
void vectNd_proj(vectNd *v, vectNd *onto, vectNd *res);
void vectNd_proj_unit(vectNd *v, vectNd *onto, vectNd *res);
#endif /* WITHOUT_INLINE */

int vectNd_cross(vectNd *v1, vectNd *res);
int vectNd_orthogonalize(vectNd *in1, vectNd *in2, vectNd *out1, vectNd *out2);
int vectNd_angle(vectNd *v1, vectNd *v2, double *angle);
int vectNd_angle3(vectNd *p1, vectNd *p2, vectNd *p3, double *angle);
int vectNd_reflect(vectNd *v, vectNd *normal, vectNd *res, double mag);
int vectNd_refract(vectNd *v, vectNd *normal, vectNd *res, double index);
int vectNd_interpolate(vectNd *s, vectNd *e, double x, vectNd *r);

int vectNd_rotate(vectNd *v, vectNd *center, int i, int j, double angle, vectNd *res);
int vectNd_rotate2(vectNd *v, vectNd *center, vectNd *v1, vectNd *v2, double angle, vectNd *res);

int vectNd_print(vectNd *v, char *name);

#ifndef WITHOUT_INLINE
#define __STATIC_INLINE__ static inline
#include "vectNd_inline.c"
#else
#define __STATIC_INLINE__
#endif /* WITHOUT_INLINE */

#endif /* VECTND_H */
