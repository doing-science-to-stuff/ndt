/*
 * nelder-mead.h
 * ndt: n-dimensional tracer
 *
 * Copyright (c) 2018-2019 Bryan Franklin. All rights reserved.
 */
#ifndef NELDER_NEAD_H
#define NELDER_NEAD_H

void nm_init(void **nm, int dimensions);
void nm_free(void *nm);
void nm_set_seed(void *nm, vectNd *seed);
void nm_best_point(void *nm, vectNd *result);
void nm_add_result(void *nm, vectNd *parameters, double value);
void nm_next_point(void *nm, vectNd *vector);
int nm_done(void *nm, double threshold, int iterations);

#endif /* NELDER_NEAD_H */
