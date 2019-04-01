/*
 * object.h
 * ndt: n-dimensional tracer
 *
 * Copyright (c) 2019 Bryan Franklin. All rights reserved.
 */
#ifndef OBJECTS_OBJECT_H
#define OBJECTS_OBJECT_H
#include "../vectNd.h"
#include "../object.h"

int type_name(char *name, int size);
int params(object *obj, int *n_pos, int *n_dir, int *n_size, int *n_flags, int *n_obj);
int get_bounds(object *obj);
int intersect(object *obj, vectNd *o, vectNd *v, vectNd *res, vectNd *normal, object **ptr);
int get_color(object *obj, vectNd *at, double *red, double *green, double *blue);
int get_reflect(object *obj, vectNd *at, double *red_r, double *green_r, double *blue_r);
int get_trans(object *obj, vectNd *at, int *transparent);
int refract_ray(object *obj, vectNd *at, double *index);

#endif /* OBJECTS_OBJECT_H */
