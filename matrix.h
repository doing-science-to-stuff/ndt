/*
 * matrix.h
 * ndt: n-dimensional tracer
 *
 * Copyright (c) 2019 Bryan Franklin. All rights reserved.
 */
#ifndef MATRIX_H
#define MATRIX_H
#include <sys/time.h>

typedef struct matrix
{
    double *values;
    int rows, cols;
    int size;
} matrix_t;

int matrix_init(matrix_t*,int,int);
int matrix_free(matrix_t*);
int matrix_set_value(matrix_t*,int,int,double);
double matrix_get_value(matrix_t*,int,int);
int matrix_randomize(matrix_t *mat, double min, double max);
int matrix_print(matrix_t*,char*);
int matrix_identity(matrix_t *mat);
int matrix_mult(matrix_t *c, matrix_t *a, matrix_t *b);
int matrix_transpose(matrix_t *at, matrix_t *a);
int matrix_normalize_columns(matrix_t *m);
int matrix_copy(matrix_t *dst, matrix_t *src);
int matrix_LU_decompose(matrix_t *lu, matrix_t *a, int *rpivots, int *cpivots);
int matrix_get_L(matrix_t *L, matrix_t *a);
int matrix_get_U(matrix_t *U, matrix_t *a);
int matrix_LU_solve(matrix_t *x, matrix_t *lu, matrix_t *b);
int matrix_solve(matrix_t *x, matrix_t *A, matrix_t *b);
int matrix_gauss_elim(matrix_t *x, matrix_t *A, matrix_t *b);
int matrix_test_solve();
int matrix_test_solve2();
int matrix_test_solve3();
int matrix_invert(matrix_t *ainv, matrix_t *a);
double matrix_trace(matrix_t *mat);
double matrix_det(matrix_t *mat);

#endif /* MATRIX_H */
