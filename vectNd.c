/*
 * vectNd.c
 * ndt: n-dimensional tracer
 *
 * Copyright (c) 2019-2020 Bryan Franklin. All rights reserved.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "object.h"
#include "vectNd.h"
#include "matrix.h"

int vectNd_fill(vectNd *v, double val)
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

int vectNd_get(vectNd *v, int pos, double *val) {
    if( pos<0 || pos >= v->n )
        return VECTND_FAIL;
    *val = v->v[pos];
    return VECTND_SUCCESS;
}

int vectNd_set(vectNd *v, int pos, double val)
{
    if( pos<0 || pos >= v->n )
        return VECTND_FAIL;
    v->v[pos] = val;
    return VECTND_SUCCESS;
}

int vectNd_setStr(vectNd *v, char *str)
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

/* this requires dimmensions-1 vectors */
int vectNd_cross(vectNd *vects, vectNd *res)
{
    int dim = -1;
    int i = 0;

    dim = vects[0].n;
    for(i=1; i<dim-1; ++i) {
        if( dim != vects[i].n )
            return VECTND_FAIL;
    }
    
    /* no idea what happens here */
    
    return VECTND_SUCCESS;
}

int vectNd_orthogonalize(vectNd *in1, vectNd *in2, vectNd *out1, vectNd *out2)
{
    /* Note int vectors and out vectors can be the same,
     * as long as in1 coresponds to out1, and in2 coresponds to out2. */

    /* get component of in1 in in2 direction */
    vectNd temp;
    vectNd_calloc(&temp,in1->n);
    vectNd_proj(in1,in2,&temp);
    /* remove that component from in1 */
    if( out1 !=NULL )
        vectNd_sub(in1,&temp,out1);
    if( out2 != NULL )
        vectNd_copy(out2,in2);
    vectNd_free(&temp);

    /* unitize results */
    if( out1 !=NULL )
        vectNd_unitize(out1);
    if( out2 != NULL )
        vectNd_unitize(out2);

    return VECTND_SUCCESS;
}

#ifdef WITHOUT_INLINE
#include "vectNd_inline.h"
#endif /* WITHOUT_INLINE */

int vectNd_angle(vectNd *v1, vectNd *v2, double *angle)
{
    double len1, len2;
    double dotProd=0.0;
    double div;

    vectNd_dot(v1,v2,&dotProd);
    vectNd_l2norm(v1,&len1);
    vectNd_l2norm(v2,&len2);

    div = len1*len2;
    if( fabs(div) > EPSILON )
        *angle = acos(dotProd / div);
    else
        *angle = -1;

    return VECTND_SUCCESS;
}

int vectNd_angle3(vectNd *p1, vectNd *p2, vectNd *p3, double *angle)
{
    vectNd v1, v2;

    vectNd_alloc(&v1,p1->n);
    vectNd_alloc(&v2,p1->n);

    vectNd_sub(p1,p2,&v1);
    vectNd_sub(p3,p2,&v2);

    vectNd_angle(&v1,&v2,angle);

    vectNd_free(&v1);
    vectNd_free(&v2);

    return VECTND_SUCCESS;
}

int vectNd_reflect(vectNd *u, vectNd *n, vectNd *res, double mag)
{
    /* see: http://www.unc.edu/~marzuola/Math547_S13/Math547_S13_Projects/P_Smith_Section001_RayTracing.pdf */
    vectNd nnu; /* norm * (norm . look) */
    double nu; /* norm . look */
    double nn; /* norm . norm (missing from source material) */

    vectNd_dot(n, u, &nu);  /* n . u */
    vectNd_dot(n, n, &nn);  /* n . n */
    vectNd_alloc(&nnu,u->n);
    vectNd_scale(n, (1+mag)*nu/nn, &nnu); /* 2*(n.nu) * n */
    vectNd_sub(u, &nnu, res); /* u - 2*(n.nu) * n */

    vectNd_free(&nnu);

    return VECTND_SUCCESS;
}

int vectNd_refract(vectNd *u, vectNd *n, vectNd *res, double index)
{
    /* see: http://en.wikipedia.org/wiki/Snell's_law */
    int dim = u->n;

    /* get angle of incidence */
    vectNd rev_u;
    vectNd rev_n;
    vectNd_alloc(&rev_u,dim);
    vectNd_alloc(&rev_n,dim);
    vectNd_scale(u,-1,&rev_u);
    vectNd_scale(n,-1,&rev_n);
    double un_dot;
    vectNd_dot(&rev_u,n,&un_dot);

    /* compute refraction angle */
    double theta_in;
    if( un_dot < 0 ) {
        /* invert index if we're on the other side of normal */
        index = 1/index;
        vectNd_angle(&rev_u,&rev_n,&theta_in);
    } else {
        vectNd_angle(&rev_u,n,&theta_in);
    }

    double theta_out;
    double sin_out = sin(theta_in) / index;
    if( sin_out <= 1.0 ) {
        theta_out = asin( sin_out );
    } else {
        /* theta_in excedes critical angle */
        theta_out = M_PI - theta_in;
    }

    /* get vector perpendicular to normal */
    vectNd_unitize(&rev_n);
    vectNd_unitize(n);
    vectNd un;
    vectNd np;
    vectNd_alloc(&un,dim);
    vectNd_alloc(&np,dim);
    vectNd_proj_unit(u,&rev_n,&un);
    vectNd_sub(u,&un,&np);
    vectNd_unitize(&np);

    /* get refraction vector */
    double rn;
    double rp;
    rn = cos(theta_out);
    rp = sin(theta_out);
    vectNd ref_n;
    vectNd ref_p;
    vectNd_alloc(&ref_n,dim);
    vectNd_alloc(&ref_p,dim);
    if( un_dot < 0 )
        vectNd_scale(n,rn,&ref_n);
    else
        vectNd_scale(&rev_n,rn,&ref_n);
    vectNd_scale(&np,rp,&ref_p);

    vectNd_add(&ref_n,&ref_p,res);

    return VECTND_SUCCESS;
}

int vectNd_interpolate(vectNd *s, vectNd *e, double t, vectNd *r)
{
    vectNd offset;
    vectNd_alloc(&offset,s->n);
    vectNd_sub(e,s,&offset);
    vectNd_scale(&offset,t,&offset);
    vectNd_add(s,&offset,r);
    vectNd_free(&offset);

    return VECTND_SUCCESS;
}

int vectNd_rotate(vectNd *v, vectNd *center, int i, int j, double angle, vectNd *res)
{
    int k=-1;
    if( i==j )
        return VECTND_FAIL;

    if( angle==0.0 )
        return VECTND_SUCCESS;

    int dim = v->n;
    if( i >= dim || j >= dim ) {
        fprintf(stderr, "%s: attempt to rotate %i dimensional vector in %i,%i plane.\n", __FUNCTION__, dim, i, j);
        return VECTND_FAIL;
    }

    /* if no result vector is given, write result to input vector */
    if( res == NULL )
        res = v;

    /* rotate points v angle degrees around the plane defined by
     * dimensions i and j  centered at c */
    vectNd tmp;
    vectNd_alloc(&tmp,v->n);

    /* shift v so c would be at the origin */
    if( center )
        vectNd_sub(v,center,&tmp);
    else
        vectNd_copy(&tmp,v);

    /* create rotation matrix */
    matrix_t rot;
    matrix_init(&rot,v->n,v->n);
    matrix_identity(&rot);
    matrix_set_value(&rot, i, i, cos(angle) );
    matrix_set_value(&rot, i, j, -sin(angle) );
    matrix_set_value(&rot, j, i, sin(angle) );
    matrix_set_value(&rot, j, j, cos(angle) );
    //matrix_print(&rot,"rotation matrix");

    /* apply matrix */
    matrix_t a;
    matrix_t b;
    matrix_init(&a,v->n,1);
    for(k=0; k<v->n; ++k)
        matrix_set_value(&a,k,0,tmp.v[k]);
    matrix_mult(&b,&rot,&a);

    /* store results */
    for(k=0; k<v->n; ++k) {
        tmp.v[k] = matrix_get_value(&b,0,k);
        if( fabs(tmp.v[k]) < EPSILON )
            tmp.v[k] = 0;
    }

    matrix_free(&rot);
    matrix_free(&a);
    matrix_free(&b);

    /* shift back to original position */
    if( center )
        vectNd_add(&tmp,center,res);
    else
        vectNd_copy(res,&tmp);
    vectNd_free(&tmp);

    return VECTND_SUCCESS;
}

int vectNd_rotate2(vectNd *v, vectNd *center, vectNd *v1, vectNd *v2, double angle, vectNd *res)
{
    vectNd basisX, basisY;
    vectNd localPos;
    vectNd projX, projY;
    vectNd rotX, rotY;

    /* if no result vector is given, write result to input vector */
    if( res == NULL )
        res = v;

    /* convert v1&v2 into orthogonal unitized basis vectors for the plane */
    vectNd_calloc(&basisX,v->n);
    vectNd_calloc(&basisY,v->n);
    vectNd_orthogonalize(v1,v2,&basisX,&basisY);

    /* adjust for center location */
    vectNd_calloc(&localPos,v->n);
    if( center )
        vectNd_sub(v,center,&localPos);
    else
        vectNd_copy(&localPos, v);

    /* project vector onto each basis vector */
    vectNd_calloc(&projX,v->n);
    vectNd_calloc(&projY,v->n);
    vectNd_proj(&localPos,&basisX,&projX);
    vectNd_proj(&localPos,&basisY,&projY);

    /* perform rotation */
    double virtX, virtY;
    vectNd_dot(&projX,&basisX,&virtX);
    vectNd_dot(&projY,&basisY,&virtY);
    vectNd_calloc(&rotX,v->n);
    vectNd_calloc(&rotY,v->n);
    vectNd_scale(&basisX, virtX*cos(angle) - virtY*sin(angle), &rotX);
    vectNd_scale(&basisY, virtY*cos(angle) + virtX*sin(angle) , &rotY);

    /* convert rotated coefficients to final point location */
    vectNd_sub(v,&projX,res);
    vectNd_sub(res,&projY,res);
    vectNd_add(res,&rotX,res);
    vectNd_add(res,&rotY,res);

    vectNd_free(&basisX);
    vectNd_free(&basisY);
    vectNd_free(&localPos);
    vectNd_free(&projX);
    vectNd_free(&projY);
    vectNd_free(&rotX);
    vectNd_free(&rotY);

    return VECTND_SUCCESS;
}

int vectNd_print(vectNd *v, char *name)
{
    if( name!=NULL )
        printf("%s: ",name);
    printf("<");
    int i=0;
    for(i=0; i<v->n; ++i)
        printf("%g%s",v->v[i], (i<(v->n-1))?", ":"");
    printf(">\n");

    return VECTND_SUCCESS;
}
