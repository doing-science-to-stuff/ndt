/*
 * matrix.c
 * ndt: n-dimensional tracer
 *
 * Copyright (c) 2019 Bryan Franklin. All rights reserved.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "matrix.h"

#define POS(m,r,c) (((r)*((m).cols))+(c))
#define VAL(m,r,c) (m).values[POS((m),(r),(c))]

int matrix_init(matrix_t *mat,int rows,int cols)
{
    mat->size = rows*cols;
    mat->values = calloc((rows+1)*(cols+1),sizeof(double));
    mat->rows = rows;
    mat->cols = cols;

    return 0;
}

int matrix_free(matrix_t *mat)
{
    if( mat->values != NULL ) {
        free(mat->values); mat->values = NULL;
    }
    memset(mat,'\0',sizeof(matrix_t));

    return 0;
}

int matrix_set_value(matrix_t *mat, int r, int c, double val)
{
    VAL(*mat,r,c) = val;
    return 0;
}

double matrix_get_value(matrix_t *mat, int r, int c)
{
    return VAL(*mat,r,c);
}

int matrix_randomize(matrix_t *mat, double min, double max) {
    int i=0, j=0;

    for(i=0; i<mat->rows; ++i) {
        for(j=0; j<mat->cols; ++j) {
            matrix_set_value(mat,i,j,drand48()*(max-min)+min);
        }
    }

    return 0;
}

int matrix_print(matrix_t *mat,char *label)
{
    int i=0, j=0;

    printf("%s: %i by %i matrix:\n", label, mat->rows, mat->cols);
    for(j=0; j<mat->rows; ++j)
    {
        for(i=0; i<mat->cols; ++i)
        {
            printf("%f\t", VAL(*mat,j,i));
        }
        printf("\n");
    }
    printf("\n");

    return 0;
}

int matrix_identity(matrix_t *mat)
{
    int i=0;

    if( mat->rows != mat->cols )
        return 1;
    memset(mat->values,'\0',(mat->rows+1)*(mat->cols+1)*sizeof(double));
    for(i=0; i<mat->rows; ++i)
    {
        VAL(*mat,i,i) = 1;
    }

    return 0;
}

int matrix_print_dim(matrix_t *a, char *label)
{
    printf("%s: %i by %i matrix:\n", label, a->rows, a->cols);
    return 0;
}

int matrix_mult(matrix_t *c, matrix_t *a, matrix_t *b)
{
    int i, j, k;
    double sum;

    matrix_init(c,a->rows,b->cols);
    for(i=0; i<c->rows; ++i)
    {
        for(j=0; j<c->cols; ++j)
        {
            sum = 0;
            for(k=0; k<a->cols; ++k)
            {
                sum += matrix_get_value(a,i,k) * matrix_get_value(b,k,j);
            }
            matrix_set_value(c,i,j,sum);
        }
    }

    return 0;
}

int matrix_transpose(matrix_t *at, matrix_t *a)
{
    int i,j;

    matrix_init(at,a->cols,a->rows);
    for(i=0; i<a->rows; ++i)
    {
        for(j=0; j<a->cols; ++j)
            matrix_set_value(at,j,i,matrix_get_value(a,i,j));
    }

    return 0;
}

int matrix_normalize_columns(matrix_t *m)
{
    int col, row;

    for(col=0; col<m->cols; ++col) {
        double val, min, max;
        min = max = val = matrix_get_value(m,0,col);
        for(row=1; row<m->rows; ++row) {
            val = matrix_get_value(m,row,col);
            if( val < min )
                min = val;
            if( val > max )
                max = val;
        }
        for(row=0; row<m->rows; ++row) {
            val = matrix_get_value(m,row,col);
            val = (val - min) / (max - min);
            matrix_set_value(m, row, col, val);
        }
    }
    
    return 0;
}

int matrix_copy(matrix_t *dst, matrix_t *src)
{
    matrix_init(dst,src->rows,src->cols);
    memcpy(dst->values,src->values,sizeof(double)*src->rows*src->cols);

    return 0;
}

int matrix_gauss_elim(matrix_t *x, matrix_t *A, matrix_t *b)
{
    int i,k,l;
    int pi, pj;
    int *cpos;

    /* setup swap tracking arrays */
    cpos = (int*)calloc(A->cols,sizeof(int));
    for(i=0; i<A->cols; ++i)
        cpos[i] = i;

    /* perform elimination with pivoting */
    for(i=0; i<A->rows; ++i) {
        /* find pivot */
        double max = fabs(matrix_get_value(A,i,i));
        pi = pj = i;
        for(k=i; k<A->rows; ++k) {
            /* complete pivoting */
            for(l=i; l<A->cols; ++l) {
                if( fabs(matrix_get_value(A,k,l)) > max ) {
                    max = fabs(matrix_get_value(A,k,l));
                    pi = k;
                    pj = l;
                }
            }   
        }

        /* perform row swap (i and pi) */
        if( i != pi ) {
            double tmp_b;
            for(k=0; k<A->cols; ++k) {
                double tmp = matrix_get_value(A,i,k);
                matrix_set_value(A,i,k,matrix_get_value(A,pi,k));
                matrix_set_value(A,pi,k,tmp);
            }
            tmp_b = matrix_get_value(b,i,0);
            matrix_set_value(b,i,0,matrix_get_value(b,pi,0));
            matrix_set_value(b,pi,0,tmp_b);
        }

        /* perform column swap (i and pj) */
        if( i != pj ) {
            int tmp_pos = -1;
            for(k=0; k<A->rows; ++k) {
                double tmp = matrix_get_value(A,k,i);
                matrix_set_value(A,k,i,matrix_get_value(A,k,pj));
                matrix_set_value(A,k,pj,tmp);
            }

            /* update swap lists */
            tmp_pos = cpos[i];
            cpos[i] = cpos[pj];
            cpos[pj] = tmp_pos;
        }

        /* do elimination */
        for(k=i+1; k<A->rows; ++k) {
            double scale = matrix_get_value(A,k,i) / matrix_get_value(A,i,i);
            double newVal = -1;
            double maxVal = 0;
            matrix_set_value(A,k,i,0);
            for(l=i+1; l<A->cols; ++l) {
                newVal = matrix_get_value(A,k,l) -
                    scale * matrix_get_value(A,i,l);
                matrix_set_value(A,k,l,newVal);
                if( fabs(newVal) > maxVal )
                    maxVal = newVal;
            }
            if( fabs(maxVal) > 1e-5 ) {
                newVal = matrix_get_value(b,k,0) -
                    scale * matrix_get_value(b,i,0);
                matrix_set_value(b,k,0,newVal/maxVal);
                for(l=i+1; l<A->cols; ++l) {
                    newVal = matrix_get_value(A,k,l);
                    matrix_set_value(A,k,l,newVal/maxVal);
                }
            }
        }
    }

    /* perform back substitution */
    for(i=A->rows-1; i>=0; --i) {
        double xi = matrix_get_value(b,i,0);
        int j=0;
        for(j=i+1; j<A->cols; ++j) {
            xi -= matrix_get_value(A,i,j)*matrix_get_value(x,0,cpos[j]);
        }
        xi /= matrix_get_value(A,i,i);
        if( isnan(xi) || isinf(xi) ) {
            free(cpos); cpos=NULL;
            return 0;
        }
        matrix_set_value(x,0,cpos[i],xi);
    }
    free(cpos); cpos=NULL;

    return 1;
}

int matrix_LU_decompose(matrix_t *lu, matrix_t *a, int *rpivots, int *cpivots)
{
    int i,j,k;

    /* copy a into lu */
    matrix_copy(lu,a);
    
    for(i=0; i<lu->rows; ++i)
    {
        double Aii;

        if( rpivots!=NULL )
        {
            /* do pivoting */
            if( cpivots!=NULL )
            {
                /* full */
                /* find largest value in remainder of A, and swap it into
                 * position i,i */
            }
            else
            {
                /* partial */
                /* find largest value in column i, and swap it into row i */
            }
        }

        Aii = matrix_get_value(lu,i,i);
        if( Aii == 0)
            return -1;

        for(k=i+1; k<lu->rows; ++k)
        {
            double S = matrix_get_value(lu,k,i) / Aii;
            for(j=i+1; j<lu->cols; ++j)
            {
                double Akj = matrix_get_value(lu,k,j);
                double Aij = matrix_get_value(lu,i,j);
                Akj -= S*Aij;
                matrix_set_value(lu,k,j,Akj);
            }
            matrix_set_value(lu,k,i,S);
        }
    }
    
    return 0;
}

int matrix_get_L(matrix_t *L, matrix_t *a)
{
    int i=0, j=0;

    matrix_init(L,a->rows,a->cols);
    for(i=0; i<L->rows; ++i)
    {
        for(j=0; j<i; ++j)
        {
            matrix_set_value(L,i,j,matrix_get_value(a,i,j));
        }
        matrix_set_value(L,i,i,1);
    }
    
    return 0;
}

int matrix_get_U(matrix_t *U, matrix_t *a)
{
    int i=0, j=0;

    matrix_init(U,a->rows,a->cols);
    for(i=0; i<U->rows; ++i)
    {
        for(j=i; j<U->cols; ++j)
        {
            matrix_set_value(U,i,j,matrix_get_value(a,i,j));
        }
    }
    
    return 0;
}

int matrix_LU_solve(matrix_t *x, matrix_t *lu, matrix_t *b)
{
    int i,k;
    matrix_t y;

    /* forward substitution */
    matrix_init(&y,b->rows,1);
    matrix_set_value(&y,0,0,matrix_get_value(b,0,0));
    for(i=1; i<y.rows; ++i)
    {
        matrix_set_value(&y,i,0,matrix_get_value(b,i,0));
        for(k=0; k<i; ++k)
        {
            matrix_set_value(&y,i,0,matrix_get_value(&y,i,0) - matrix_get_value(lu,i,k)*matrix_get_value(&y,k,0));
        }
    }

    /* backward substitution */
    matrix_init(x,y.rows,1);
    for(i=y.rows-1; i>=0; --i)
    {
        double S = matrix_get_value(&y,i,0);
        for(k=i+1; k<lu->cols; ++k)
        {
            S = S - matrix_get_value(lu,i,k)*matrix_get_value(x,k,0);
        }
        matrix_set_value(x,i,0,S/matrix_get_value(lu,i,i));
    }
    matrix_free(&y);

    return 0;
}

int matrix_solve(matrix_t *x, matrix_t *A, matrix_t *b)
{
    matrix_t LU;

    /* LU decompose A */
    matrix_LU_decompose(&LU,A,NULL,NULL);

    /* solve using LU decomposed A */
    matrix_LU_solve(x,&LU,b);

    matrix_free(&LU); /* allocated in matrix_LU_decompose */

    return 0;
}

/* see:
 * https://math.dartmouth.edu/archive/m23s06/public_html/handouts/row_reduction_examples.pdf (example 2)
 * Should give x=<-4,5,-2>
 */
int matrix_test_solve()
{
    matrix_t A;
    matrix_t b;
    matrix_t x;

    matrix_init(&A,3,3);
    matrix_init(&b,3,1);
    matrix_init(&x,1,3);

    matrix_set_value(&A,0,0,0);
    matrix_set_value(&A,0,1,2);
    matrix_set_value(&A,0,2,1);
    matrix_set_value(&A,1,0,1);
    matrix_set_value(&A,1,1,-2);
    matrix_set_value(&A,1,2,-3);
    matrix_set_value(&A,2,0,-1);
    matrix_set_value(&A,2,1,1);
    matrix_set_value(&A,2,2,2);

    matrix_set_value(&b,0,0,-8);
    matrix_set_value(&b,1,0,0);
    matrix_set_value(&b,2,0,3);

    matrix_gauss_elim(&x,&A,&b);

    printf("Begin %s\n", __FUNCTION__);
    matrix_print(&A,"A");
    matrix_print(&b,"b");
    matrix_print(&x,"x");
    printf("End %s\n", __FUNCTION__);

    matrix_free(&A);
    matrix_free(&b);
    matrix_free(&x);

    return 0;
}

/* see:
 * http://www.math.sjsu.edu/~foster/m143m/pivoting_examples_2.pdf
 */
int matrix_test_solve2()
{
    matrix_t A;
    matrix_t b;
    matrix_t x;

    matrix_init(&A,3,3);
    matrix_init(&b,3,1);
    matrix_init(&x,1,3);

    matrix_set_value(&A,0,0,0);
    matrix_set_value(&A,0,1,2);
    matrix_set_value(&A,0,2,-3);
    matrix_set_value(&A,1,0,2);
    matrix_set_value(&A,1,1,2);
    matrix_set_value(&A,1,2,1);
    matrix_set_value(&A,2,0,2);
    matrix_set_value(&A,2,1,4);
    matrix_set_value(&A,2,2,4);

    matrix_set_value(&b,0,0,0);
    matrix_set_value(&b,1,0,0);
    matrix_set_value(&b,2,0,0);

    matrix_gauss_elim(&x,&A,&b);

    printf("Begin %s\n", __FUNCTION__);
    matrix_print(&A,"A");
    matrix_print(&b,"b");
    matrix_print(&x,"x");
    printf("End %s\n", __FUNCTION__);

    matrix_free(&A);
    matrix_free(&b);
    matrix_free(&x);

    return 0;
}

int matrix_test_solve3()
{
    matrix_t A;
    matrix_t b;
    matrix_t x;

    matrix_init(&A,4,4);
    matrix_init(&b,4,1);
    matrix_init(&x,1,4);

    matrix_set_value(&A,0,0,1);
    matrix_set_value(&A,0,1,-2);
    matrix_set_value(&A,0,2,3);
    matrix_set_value(&A,0,3,1);

    matrix_set_value(&A,1,0,-2);
    matrix_set_value(&A,1,1,1);
    matrix_set_value(&A,1,2,-2);
    matrix_set_value(&A,1,3,-1);

    matrix_set_value(&A,2,0,3);
    matrix_set_value(&A,2,1,-2);
    matrix_set_value(&A,2,2,1);
    matrix_set_value(&A,2,3,5);

    matrix_set_value(&A,3,0,1);
    matrix_set_value(&A,3,1,-1);
    matrix_set_value(&A,3,2,5);
    matrix_set_value(&A,3,3,3);

    matrix_set_value(&b,0,0,3);
    matrix_set_value(&b,1,0,-4);
    matrix_set_value(&b,2,0,7);
    matrix_set_value(&b,3,0,8);

    matrix_gauss_elim(&x,&A,&b);

    printf("Begin %s\n", __FUNCTION__);
    matrix_print(&A,"A");
    matrix_print(&b,"b");
    matrix_print(&x,"x");
    printf("End %s\n", __FUNCTION__);

    matrix_free(&A);
    matrix_free(&b);
    matrix_free(&x);

    return 0;
}

int matrix_invert(matrix_t *ainv, matrix_t *a)
{
    matrix_t lu, b, x;
    int i,j;

    if( a->rows != a->cols )
    {
        fprintf(stderr,"Can't invert a non-square matrix. (%ix%i)\n", a->rows, a->cols);
        return -1;
    }

    /* LU decompose A */
    matrix_LU_decompose(&lu,a,NULL,NULL);

    /* use forward/backward substitution to solve for columns of A^{-1} */
    matrix_init(ainv,a->cols,a->rows);
    matrix_init(&b,a->rows,1);
    for(i=0; i<ainv->cols; ++i)
    {
        /* fill b */
        for(j=0; j<ainv->rows; ++j)
            matrix_set_value(&b,j,0,(j==i)?1:0);

        /* solve for x */
        matrix_solve(&x,a,&b);

        /* copy solution into Ainv */
        for(j=0; j<ainv->rows; ++j)
            matrix_set_value(ainv,j,i,matrix_get_value(&x,j,0));

        matrix_free(&x);    /* was allocated in matrix_LU_solve */
    }
    matrix_free(&b);    /* was allocated above */
    matrix_free(&lu);   /* was allocated in matrix_LU_decompose */

    return 0;
}

double matrix_trace(matrix_t *mat)
{
    int i=0;
    double ret = 0;
    
    if( mat->rows!=mat->cols )
    {
        fprintf(stderr,"%s: can not trace a non-square matrix.\n", __FUNCTION__);
        return -1.0;
    }

    ret = 0.0;
    for(i=0; i<mat->rows; ++i)
    {
        ret += matrix_get_value(mat,i,i);
    }

    return ret;
}

double matrix_det(matrix_t *mat)
{
    matrix_t lu;
    double ret;
    int i;

    matrix_LU_decompose(&lu,mat,NULL,NULL);

    ret = 1.0;
    for(i=0; i<mat->rows; ++i)
    {
        ret *= matrix_get_value(&lu,i,i);
    }

    matrix_free(&lu);

    return ret;
}
