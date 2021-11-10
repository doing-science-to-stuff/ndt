/*
 * vectNd.h
 * ndt: n-dimensional tracer
 *
 * Copyright (c) 2019-2021 Bryan Franklin. All rights reserved.
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

#ifndef EPSILON
#define EPSILON (1e-4)
#endif /* EPSILON */
#ifndef EPSILON2
#define EPSILON2 ((EPSILON)*(EPSILON))
#endif /* EPSILON2 */

#if defined(__SSE__) && !defined(WITHOUT_SSE)
/* typecast v to __m128d* */
#define vectNd_SSE(x)  ((__m128d*)((x)->v))
#else
#warning "Not using SSE"
#endif /* __SSE__ */

#ifndef __SSE__
#error "SSE not available"
#endif /* 0 */

typedef struct vectNd_t
{
    double space[VECTND_DEF_SIZE];
    double *v;
    int n;
}
#if defined(__SSE__) && !defined(WITHOUT_SSE)
__attribute__((__aligned__(16)))
#endif /* __SSE__ */
vectNd;

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

/* see: https://en.wikipedia.org/wiki/Inline_function#Nonstandard_extensions */
#ifdef _MSC_VER
    #define forceinline __forceinline
#elif defined(__GNUC__)
    #define forceinline inline __attribute__((__always_inline__))
#elif defined(__CLANG__)
    #if __has_attribute(__always_inline__)
        #define forceinline inline __attribute__((__always_inline__))
    #else
        #define forceinline inline
    #endif
#else
    #define forceinline inline
#endif

static forceinline int vectNd_fill(vectNd *v, double val)
{
    int i=0;
    #if defined(__SSE__) && !defined(WITHOUT_SSE)
    int k=(v->n+1)/2;
    for(i=0; i<k; ++i)
        vectNd_SSE(v)[i] = _mm_set1_pd(val);
    #else
    for(i=0; i<v->n; ++i)
        v->v[i] = val;
    #endif /* __SSE__ */
    return VECTND_SUCCESS;
}

static forceinline int vectNd_get(vectNd *v, int pos, double *val) {
    if( pos<0 || pos >= v->n )
        return VECTND_FAIL;
    *val = v->v[pos];
    return VECTND_SUCCESS;
}

static forceinline int vectNd_set(vectNd *v, int pos, double val)
{
    if( pos<0 || pos >= v->n )
        return VECTND_FAIL;
    v->v[pos] = val;
    return VECTND_SUCCESS;
}

static forceinline int vectNd_setStr(vectNd *v, char *str)
{
    int pos=0;
    char *lstr = strdup(str);
    char *lasts=NULL;
    char *a = strtok_r(lstr,",",&lasts);
    while( a!=NULL ) {
        vectNd_set(v,pos++,atof(a));
        a = strtok_r(NULL,",",&lasts);
    }
    free(lstr); lstr=NULL;
    return VECTND_SUCCESS;
}

static forceinline int vectNd_alloc(vectNd *v, int dim)
{
    v->n = dim;
    if( dim > VECTND_DEF_SIZE ) {
        int alloc_dim = dim;
        #if defined(__SSE__) && !defined(WITHOUT_SSE)
        alloc_dim += alloc_dim&1;   /* make even if needed */
        #endif /* __SSE__ */
        #ifdef __SSE__
        void *ptr=NULL;
        if( posix_memalign(&ptr, 16, alloc_dim*sizeof(double)) ) {
            v->v = NULL;
            return VECTND_FAIL;
        }
        v->v = ptr;
        #else
        v->v = (double*)malloc(alloc_dim*sizeof(double));
        #endif /* 0 */
    } else {
        v->v = v->space;
    }
    #if defined(__SSE__) && !defined(WITHOUT_SSE)
    /* set padding field to zero if an odd number of dimensions */
    if( dim & 1 ) v->v[dim] = 0.0;
    #endif /* __SSE__ */
    return VECTND_SUCCESS;
}

static forceinline int vectNd_calloc(vectNd *v, int dim)
{
    vectNd_alloc(v,dim);
    vectNd_fill(v,0.0);

    return VECTND_SUCCESS;
}

static forceinline int vectNd_free(vectNd *v)
{
    if( v->n > VECTND_DEF_SIZE ) {
        free(v->v); v->v = NULL;
    }
    v->v = NULL;
    v->n = -1;
    return VECTND_SUCCESS;
}

static forceinline int vectNd_reset(vectNd *v)
{
    memset(v->v,'\0',v->n*sizeof(*v->v));
    return VECTND_SUCCESS;
}

static forceinline void vectNd_min(vectNd *v, double *res) {
    int dim = v->n;
    int i;
    double *vv = v->v;
    double min = vv[0];
    for(i=1; i<dim; ++i) {
        double vvi = vv[i];
        min = (vvi<min)?(vvi):(min);
    }
    *res = min;
}

static forceinline void vectNd_max(vectNd *v, double *res) {
    int dim = v->n;
    int i;
    double *vv = v->v;
    double max = vv[0];
    for(i=1; i<dim; ++i) {
        double vvi = vv[i];
        max = (vvi>max)?(vvi):(max);
    }
    *res = max;
}

static forceinline void vectNd_mul(vectNd *v1, vectNd *v2, vectNd *res) {
    int dim = v1->n;
    int i;
    double *v1v;
    double *v2v;
    double *r;

    v1v = v1->v;
    v2v = v2->v;
    r = res->v;
    for(i=0; i<dim; ++i) {
        r[i] = v1v[i] * v2v[i];
    }
}

static forceinline void vectNd_dot(vectNd *v1, vectNd *v2, double *res)
{
    #if defined(__SSE__) && !defined(WITHOUT_SSE)
    int k=(v1->n+1)>>1;
    int i;
    __m128d prod;
    __m128d sums;
    sums = _mm_mul_pd(vectNd_SSE(v1)[0],vectNd_SSE(v2)[0]);
    for(i=1; i<k; ++i) {
        prod = _mm_mul_pd(vectNd_SSE(v1)[i],vectNd_SSE(v2)[i]);
        sums = _mm_add_pd(sums,prod);
    }
    *res = sums[0]+sums[1];
    #else /* __SSE__ */
    int dim = v1->n;
    double sum = 0.0;
    int i;
    double *v1v;
    double *v2v;

    v1v = v1->v;
    v2v = v2->v;
    for(i=0; i<dim; ++i) {
        sum += v1v[i] * v2v[i];
    }
    *res = sum;
    #endif /* __SSE__ */
}

static forceinline void vectNd_add(vectNd *v1, vectNd *v2, vectNd *res)
{
    #if defined(__SSE__) && !defined(WITHOUT_SSE)
    int k=(v1->n+1)>>1;
    int i;
    vectNd_SSE(res)[0] = _mm_add_pd(vectNd_SSE(v1)[0],vectNd_SSE(v2)[0]);
    for(i=1; i<k; ++i)
        vectNd_SSE(res)[i] = _mm_add_pd(vectNd_SSE(v1)[i],vectNd_SSE(v2)[i]);
    #else /* __SSE__ */
    double *v1v;
    double *v2v;
    double *resv;
    int n;
    int i;

    v1v = v1->v;
    v2v = v2->v;
    resv = res->v;
    n = v1->n;
    for(i=0; i<n; ++i)
        resv[i] = v1v[i] + v2v[i];
    #endif /* __SSE__ */
}

static forceinline void vectNd_sub(vectNd *v1, vectNd *v2, vectNd *res)
{
    #if defined(__SSE__) && !defined(WITHOUT_SSE)
    int k=(v1->n+1)>>1;
    int i;
    vectNd_SSE(res)[0] = _mm_sub_pd(vectNd_SSE(v1)[0],vectNd_SSE(v2)[0]);
    for(i=1; i<k; ++i)
        vectNd_SSE(res)[i] = _mm_sub_pd(vectNd_SSE(v1)[i],vectNd_SSE(v2)[i]);
    #else /* __SSE__ */
    double *v1v;
    double *v2v;
    double *resv;
    int n;
    int i;
    n = v1->n;

    v1v = v1->v;
    v2v = v2->v;
    resv = res->v;
    for(i=0; i<n; ++i)
        resv[i] = v1v[i] - v2v[i];
    #endif /* __SSE__ */
}

static forceinline void vectNd_scale(vectNd *v, double s, vectNd *res)
{
    #if defined(__SSE__) && !defined(WITHOUT_SSE)
    int k=(v->n+1)>>1;
    int i;
    __m128d scale = _mm_set1_pd(s);
    vectNd_SSE(res)[0] = _mm_mul_pd(vectNd_SSE(v)[0],scale);
    for(i=1; i<k; ++i)
        vectNd_SSE(res)[i] = _mm_mul_pd(vectNd_SSE(v)[i],scale);
    #else /* __SSE__ */
    double *vv;
    double *resv;
    int n;
    int i;

    vv = v->v;
    resv = res->v;
    n = v->n;
    for(i=0; i<n; ++i)
        resv[i] = vv[i]*s;
    #endif /* __SSE__ */
}

static forceinline void vectNd_l2norm(vectNd *v, double *res)
{
    double sum;
    vectNd_dot(v,v,&sum);
    *res = sqrt(sum);
}
#define vectNd_length vectNd_l2norm

static forceinline void vectNd_unitize(vectNd *v)
{
    double len;
    vectNd_l2norm(v,&len);
    if( len > EPSILON || len < -EPSILON )
        vectNd_scale(v,1.0/len,v);
}

static forceinline void vectNd_dist(vectNd *v1, vectNd *v2, double *res)
{
    vectNd diff;
    vectNd_alloc(&diff,v1->n);
    vectNd_sub(v1,v2,&diff);
    vectNd_l2norm(&diff,res);
    vectNd_free(&diff);
}

static forceinline void vectNd_copy(vectNd *dst, vectNd *src)
{
    memcpy(dst->v,src->v,src->n*sizeof(*dst->v));
}

/* project a vector onto a vector known to be unit length */
static forceinline void vectNd_proj_unit(vectNd *v, vectNd *onto, vectNd *res)
{
    double ab;

    vectNd_dot(v,onto,&ab);
    vectNd_scale(onto,ab,res);
}

/* project a vector onto a vector of unknown length */
static forceinline void vectNd_proj(vectNd *v, vectNd *onto, vectNd *res)
{
    /* see: http://en.wikipedia.org/wiki/Vector_projection#Vector_projection_2 */
    double ab, bb;

    vectNd_dot(onto,onto,&bb);
    vectNd_dot(v,onto,&ab);
    vectNd_scale(onto,ab/bb,res);
}

#endif /* VECTND_H */
