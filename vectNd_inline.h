/*
 * vectNd_inline.h
 * ndt: n-dimensional tracer
 *
 * Copyright (c) 2019 Bryan Franklin. All rights reserved.
 */
#ifndef EPSILON
#define EPSILON (1e-4)
#endif /* EPSILON */

#ifndef __SSE__
#error "SSE not available"
#endif /* 0 */

__STATIC_INLINE__ int vectNd_alloc(vectNd *v, int dim)
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

__STATIC_INLINE__ int vectNd_calloc(vectNd *v, int dim)
{
    vectNd_alloc(v,dim);
    vectNd_fill(v,0.0);

    return VECTND_SUCCESS;
}

__STATIC_INLINE__ int vectNd_free(vectNd *v)
{
    if( v->n > VECTND_DEF_SIZE ) {
        free(v->v); v->v = NULL;
    }
    v->v = NULL;
    v->n = -1;
    return VECTND_SUCCESS;
}

__STATIC_INLINE__ int vectNd_reset(vectNd *v)
{
    memset(v->v,'\0',v->n*sizeof(*v->v));
    return VECTND_SUCCESS;
}

__STATIC_INLINE__ void vectNd_dot(vectNd *v1, vectNd *v2, double *res)
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

__STATIC_INLINE__ void vectNd_add(vectNd *v1, vectNd *v2, vectNd *res)
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

__STATIC_INLINE__ void vectNd_sub(vectNd *v1, vectNd *v2, vectNd *res)
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

__STATIC_INLINE__ void vectNd_scale(vectNd *v, double s, vectNd *res)
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

__STATIC_INLINE__ void vectNd_l2norm(vectNd *v, double *res)
{
    double sum;
    vectNd_dot(v,v,&sum);
    *res = sqrt(sum);
}

__STATIC_INLINE__ void vectNd_unitize(vectNd *v)
{
    double len;
    vectNd_l2norm(v,&len);
    if( len > EPSILON || len < -EPSILON )
        vectNd_scale(v,1.0/len,v);
}

__STATIC_INLINE__ void vectNd_dist(vectNd *v1, vectNd *v2, double *res)
{
    vectNd diff;
    vectNd_alloc(&diff,v1->n);
    vectNd_sub(v1,v2,&diff);
    vectNd_l2norm(&diff,res);
    vectNd_free(&diff);
}

__STATIC_INLINE__ void vectNd_copy(vectNd *dst, vectNd *src)
{
    memcpy(dst->v,src->v,src->n*sizeof(*dst->v));
}

/* project a vector onto a vector known to be unit length */
__STATIC_INLINE__ void vectNd_proj_unit(vectNd *v, vectNd *onto, vectNd *res)
{
    double ab;

    vectNd_dot(v,onto,&ab);
    vectNd_scale(onto,ab,res);
}

/* project a vector onto a vector of unknown length */
__STATIC_INLINE__ void vectNd_proj(vectNd *v, vectNd *onto, vectNd *res)
{
    /* see: http://en.wikipedia.org/wiki/Vector_projection#Vector_projection_2 */
    double ab, bb;

    vectNd_dot(onto,onto,&bb);
    vectNd_dot(v,onto,&ab);
    vectNd_scale(onto,ab/bb,res);
}
