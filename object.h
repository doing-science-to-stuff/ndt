/*
 * object.h
 * ndt: n-dimensional tracer
 *
 * Copyright (c) 2019-2021 Bryan Franklin. All rights reserved.
 */
#ifndef OBJECT_H
#define OBJECT_H
#include "vectNd.h"
#include "bounding.h"
#ifndef WITHOUT_KDTREE
#include "kd-tree.h"
#endif /* !WITHOUT_KDTREE */

#define EPSILON (1e-4)
#ifndef EPSILON2
#define EPSILON2 ((EPSILON)*(EPSILON))
#endif /* EPSILON2 */

#define OBJ_TYPE_MAX_LEN 64
#define OBJ_NAME_MAX_LEN 32

typedef struct gen_object {
    unsigned int transparent:1;
    unsigned int prepared:1;
    int dimensions;
    double red, green, blue;
    double red_r, green_r, blue_r;
    double refract_index;

    char name[OBJ_NAME_MAX_LEN];

    /* array of positions */
    vectNd *pos;
    int n_pos;
    int cap_pos;

    /* array of directions */
    vectNd *dir;
    int n_dir;
    int cap_dir;

    /* array of scalars */
    double *size;
    int n_size;
    int cap_size;

    /* array of integers */
    int *flag;
    int n_flag;
    int cap_flag;

    /* array of other objects */
    struct gen_object **obj;
    int n_obj;
    int cap_obj;

    /* bounds data */
    bounding_sphere bounds;

    /* opaque pointer to data computed in prepare method */
    void *prepped;

    void *dl_handle;
    int (*type_name)(char *name, int size);
    int (*params)(struct gen_object *obj, int *n_pos, int *n_dir, int *n_size, int *n_flags, int *n_obj);
    int (*cleanup)(struct gen_object *obj);
    int (*bounding_points)(struct gen_object * obj, bounds_list *list);
    int (*intersect)(struct gen_object * obj, vectNd *o, vectNd *v, vectNd *res, vectNd *normal, struct gen_object **obj_ptr);
    int (*get_color)(struct gen_object *obj, vectNd *at, double *red, double *green, double *blue);
    int (*get_reflect)(struct gen_object *obj, vectNd *at, double *red_r, double *green_r, double *blue_r);
    int (*get_trans)(struct gen_object *obj, vectNd *at, int *transparent);
    int (*refract_ray)(struct gen_object *obj, vectNd *at, double *index);
} object;

struct object_reg_entry {
    char type[OBJ_TYPE_MAX_LEN];
    object obj;
    struct object_reg_entry *next;
};

struct object_registry {
   struct object_reg_entry *objs;
};

/* initial loading of object types */
int register_objects(char *dirname);
int register_object(char *filename);
int registered_types(char ***list, int *num);
int registered_types_free(char **list);
int unregister_objects();

/* instanciation of objects */
object *object_alloc(int dimensions, char *type, char *name);
int object_free(object *obj);
int object_cleanup_all(object *obj);
int object_validate(object *obj);

/* add things to object */
int object_add_pos(object *obj, vectNd *new_pos);
int object_add_posStr(object *obj, char *str);
int object_add_dir(object *obj, vectNd *new_dir);
int object_add_dirStr(object *obj, char *str);
int object_add_size(object *obj, double new_size);
int object_add_flag(object *obj, int new_flag);
int object_add_obj(object *obj, object *new_obj);

/* positioning objects */
int object_move(object * obj, vectNd *offset);
int object_rotate(object * obj, vectNd *center, int v1, int v2, double angle);
int object_rotate2(object * obj, vectNd *center, vectNd *v1, vectNd *v2, double angle);
int object_get_bounds(object *obj);

/* tracing rays to objects */
#ifndef WITHOUT_KDTREE
int object_kdlist_add(kd_item_list_t *list, object *obj, int obj_id);
int trace_kd(vectNd *pos, vectNd *unit_look, kd_tree_t *kd, vectNd *hit, vectNd *hit_normal, object **ptr, double dist_limit);
#endif /* !WITHOUT_KDTREE */
int trace(vectNd *pos, vectNd *unit_look, object **objs, int *ids, int n, char *obj_mask, vectNd *hit, vectNd *hit_normal, object **ptr, double *t_ptr, double dist_limit);

#endif /* OBJECT_H */
