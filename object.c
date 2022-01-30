/*
 * object.c
 * ndt: n-dimensional tracer
 *
 * Copyright (c) 2019-2021 Bryan Franklin. All rights reserved.
 */
#include <dirent.h>
#include <dlfcn.h>
#include <limits.h>
#include <pthread.h>
#include <stdio.h>
#include <unistd.h>
#ifdef WITH_VALGRIND
#include <valgrind/valgrind.h>
#endif /* WITH_VALGRIND */
#include "bounding.h"
#include "object.h"

static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;

static struct object_registry registry = {NULL};

static int default_color(object *obj, vectNd *at, double *red, double *green, double *blue) {
    *red = obj->red;
    *green = obj->green;
    *blue = obj->blue;
    if( at==NULL )
        return 0;

    return 0;
}

static int default_reflect(object *obj, vectNd *at, double *red_r, double *green_r, double *blue_r) {
    *red_r = obj->red_r;
    *green_r = obj->green_r;
    *blue_r = obj->blue_r;
    if( at==NULL )
        return 0;

    return 0;
}

static int default_trans(object *obj, vectNd *at, int *transparent) {
    *transparent = obj->transparent;
    if( at==NULL )
        return 0;

    return 0;
}

int register_object(char *filename) {
    /* record function pointers */
    void *dl_handle = NULL;
    /* If you're looking to fix the memory leak reported here, see:
     * https://bugs.kde.org/show_bug.cgi?id=358980
     */
    dl_handle = dlopen(filename,RTLD_NOW);
    if( !dl_handle ) {
        fprintf(stderr, "%s\n", dlerror());
        return -1;
    }

    /* add record to registry */
    int error = 0;
    struct object_reg_entry *entry;
    entry = calloc(1,sizeof(*entry));
    entry->obj.dl_handle = dl_handle;
    entry->obj.type_name = (int (*)(char *, int))dlsym(dl_handle, "type_name");
    entry->obj.params = (int (*)(struct gen_object *, int *, int *, int *, int *, int *))dlsym(dl_handle, "params");
    entry->obj.cleanup = (int (*)(struct gen_object *))dlsym(dl_handle, "cleanup");
    entry->obj.bounding_points = (int (*)(struct gen_object *, bounds_list *))dlsym(dl_handle, "bounding_points");
    entry->obj.intersect = (int (*)(struct gen_object *, vectNd *, vectNd *, vectNd *, vectNd *, struct gen_object **))dlsym(dl_handle, "intersect");
    entry->obj.get_color = (int (*)(struct gen_object *, vectNd *, double *, double *, double *))dlsym(dl_handle, "get_color");
    if( entry->obj.get_color == NULL )
        entry->obj.get_color = default_color;
    entry->obj.get_reflect = (int (*)(struct gen_object *, vectNd *, double *, double *, double *))dlsym(dl_handle, "get_reflect");
    if( entry->obj.get_reflect == NULL )
        entry->obj.get_reflect = default_reflect;
    entry->obj.get_trans = (int (*)(struct gen_object *, vectNd *, int *))dlsym(dl_handle, "get_trans");
    if( entry->obj.get_trans == NULL )
        entry->obj.get_trans = default_trans;
    entry->obj.refract_ray = (int (*)(struct gen_object *, vectNd *, double *))dlsym(dl_handle, "refract_ray");

    /* check for required functions */
    if( entry->obj.type_name == NULL ) {
        fprintf(stderr, "%s missing type_name function.\n", filename);
        error = 1;
    }
    if( entry->obj.params == NULL ) {
        fprintf(stderr, "%s missing params function.\n", filename);
        error = 2;
    }
    if( entry->obj.bounding_points == NULL ) {
        fprintf(stderr, "%s missing bounding_points function.\n", filename);
        error = 3;
    }
    if( entry->obj.intersect == NULL ) {
        fprintf(stderr, "%s missing intersect function.\n", filename);
        error = 4;
    }

    if( error ) {
        #ifdef WITH_VALGRIND
        if( !RUNNING_ON_VALGRIND )
        #endif /* WITH_VALGRIND */
            dlclose(dl_handle);
        free(entry); entry = NULL;
        return -1;
    }

    entry->obj.type_name(entry->type, sizeof(entry->type));
    entry->next = registry.objs;

    registry.objs = entry;

    char *slash = NULL;
    if( (slash=strrchr(filename, '/')) ) {
        filename = slash+1;
    }
    printf("\tloaded object from '%s'.\n", filename);

    return 0;
}

int register_objects(char *dirname) {
    if( dirname == NULL ) {
        fprintf(stderr,"%s: dirname is NULL\n", __FUNCTION__);
        return -1;
    }

    /* open directory */
    printf("%s: opening '%s' directory\n", __FUNCTION__, dirname);
    DIR *dirp = opendir(dirname);
    if( dirp == NULL ) {
        perror("opendir");
        return -1;
    }

    /* read filenames */
    struct dirent *dp;
    while( (dp = readdir(dirp)) != NULL ) {
        /* check extension */
        int namlen = -1;
        namlen = strlen(dp->d_name);
        if( namlen > 3 && strncasecmp(dp->d_name+namlen-3, ".so", 3)==0 ) {
            /* avoid library search path */
            char filename[PATH_MAX];
            snprintf(filename,sizeof(filename), "%s/%s", dirname, dp->d_name);

            /* load .so file found */
            register_object(filename);
        }
    }

    closedir(dirp); dirp = NULL;

    return 0;
}

int registered_types(char ***list, int *num) {
    /* count registered types */
    int i=0;
    struct object_reg_entry *curr;
    curr = registry.objs;
    while( curr ) {
        i++;
        curr = curr->next;
    }
    *num = i;

    /* allocate *list */
    *list = (char**)calloc(i+1,sizeof(*list));
    if( *list == NULL ) {
        *num = -1;
        return -1;
    }

    /* fill list */
    curr = registry.objs;
    i = 0;
    while( curr ) {
        (*list)[i++] = strdup(curr->type);
        curr = curr->next;
    }

    return *num;
}

int registered_types_free(char **list) {
    if( list == NULL ) {
        return 0;
    }

    /* free list */
    for(int i=0; list[i]!=NULL; ++i) {
        free(list[i]); list[i] = NULL;
    }
    free(list); list=NULL;

    return 0;
}

int unregister_objects() {
   /* free registration structures */
   struct object_reg_entry *curr = registry.objs;
   while( curr ) {
       struct object_reg_entry *next = curr->next;
       printf("unregistering '%s'.\n", curr->type);
       #ifdef WITH_VALGRIND
       /* disable dlclose in valgrind:
        * http://valgrind.org/docs/manual/faq.html#faq.unhelpful
        */
       if( !RUNNING_ON_VALGRIND ) {
       #endif /* WITH_VALGRIND */
           dlclose(curr->obj.dl_handle); curr->obj.dl_handle = NULL;
       #ifdef WITH_VALGRIND
       }
       #endif /* WITH_VALGRIND */
       free(curr);
       curr = next;
   }

    return 0;
}

object *object_alloc(int dimensions, char *type, char *name) {
    
    /* find type in registry */
    struct object_reg_entry *curr = registry.objs;
    while( curr && strcasecmp(curr->type, type) ) {
        curr = curr->next;
    }
    if( curr == NULL ) {
        fprintf(stderr, "Unknown object type '%s'.\n", type);
        exit(1);
    }

    /* allocate object */
    object *obj = NULL;
    obj = calloc(1, sizeof(object));

    /* record number of dimensions */
    obj->dimensions = dimensions;

    /* fill in function pointers */
    obj->type_name = curr->obj.type_name;
    obj->params = curr->obj.params;
    obj->cleanup = curr->obj.cleanup;
    obj->bounding_points = curr->obj.bounding_points;
    obj->intersect = curr->obj.intersect;
    obj->get_color = curr->obj.get_color;
    obj->get_reflect = curr->obj.get_reflect;
    obj->get_trans = curr->obj.get_trans;
    obj->refract_ray = curr->obj.refract_ray;

    /* create bounding sphere center */
    vectNd_calloc(&obj->bounds.center, dimensions);
    obj->bounds.radius = 0;

    /* set a default name */
    /* Note: unique names by default migth be helpful. */
    if( name==NULL )
        name = "unnamed";
    
    /* store name in object */
    strncpy(obj->name, name, sizeof(obj->name));
    obj->name[sizeof(obj->name)-1] = '\0';

    return obj;
}

int object_free(object *obj) {

    if( obj->cleanup ) {
        obj->cleanup(obj);
    }

    vectNd_free(&obj->bounds.center);
    obj->bounds.radius = 0;

    if( obj->pos != NULL ) {
        for(int i=0; i<obj->n_pos; ++i) {
            vectNd_free(&obj->pos[i]);
        }
        free(obj->pos); obj->pos = NULL;
    }

    if( obj->dir != NULL ) {
        for(int i=0; i<obj->n_dir; ++i) {
            vectNd_free(&obj->dir[i]);
        }
        free(obj->dir); obj->dir = NULL;
    }

    if( obj->size != NULL ) {
        free(obj->size); obj->size = NULL;
    }

    if( obj->flag != NULL ) {
        free(obj->flag); obj->flag = NULL;
    }

    if( obj->obj !=NULL ) {
        for(int i=0; i<obj->n_obj; ++i) {
            object_free(obj->obj[i]); obj->obj[i] = NULL;
        }
        free(obj->obj); obj->obj = NULL;
    }

    if( obj->prepped != NULL ) {
        free(obj->prepped); obj->prepped = NULL;
    }
    obj->prepared = 0;

    free(obj);

    return 0;
}

int object_cleanup_all(object *obj) {
    /* perform a post-order cleanup on all objects rooted at obj */
    for(int i=0; i<obj->n_obj; ++i) {
        object_cleanup_all(obj->obj[i]);
    }

    if( obj->cleanup && obj->prepared ) {
        obj->cleanup(obj);
    }
    obj->prepared = 0;
    vectNd_reset(&obj->bounds.center);
    obj->bounds.radius = 0;

    return 0;
}

int object_validate(object *obj) {
    char type[256];
    obj->type_name(type, sizeof(type));

    /* check each function pointer */
    if( obj->type_name == NULL ) {
        fprintf(stderr, "type_name not set on object %p.\n", (void*)obj);
        return -1;
    }

    if( obj->params == NULL ) {
        fprintf(stderr, "params not set on %s object %p.\n", type, (void*)obj);
        return -1;
    }
    if( obj->bounding_points == NULL ) {
        fprintf(stderr, "bounding_points not set on %s object %p.\n", type, (void*)obj);
        return -1;
    }
    if( obj->intersect == NULL ) {
        fprintf(stderr, "intersect not set on %s object %p.\n", type, (void*)obj);
        return -1;
    }
    if( obj->get_color == NULL ) {
        fprintf(stderr, "get_color not set on %s object %p.\n", type, (void*)obj);
        return -1;
    }
    if( obj->get_reflect == NULL ) {
        fprintf(stderr, "get_reflect not set on %s object %p.\n", type, (void*)obj);
        return -1;
    }
    if( obj->get_trans == NULL ) {
        fprintf(stderr, "get_trans not set on %s object %p.\n", type, (void*)obj);
        return -1;
    }

    /* verify parameter counts */
    int n_pos;
    int n_dir;
    int n_size;
    int n_flag;
    int n_obj;
    char obj_name[OBJ_NAME_MAX_LEN+8] = "";
    if( obj->name[0]!='\0' )
        snprintf(obj_name, sizeof(obj_name), "'%s' ", obj->name);
    obj->params(obj, &n_pos, &n_dir, &n_size, &n_flag, &n_obj);
    if( n_pos > obj->n_pos ) {
        fprintf(stderr, "insufficient positions set for %s object %s%p (%i set, %i required).\n", type, obj_name, (void*)obj, obj->n_pos, n_pos);
        exit(1);
    }
    if( n_dir > obj->n_dir ) {
        fprintf(stderr, "insufficient directions set for %s object %s%p (%i set, %i required).\n", type, obj_name, (void*)obj, obj->n_dir, n_dir);
        exit(1);
    }
    if( n_size > obj->n_size ) {
        fprintf(stderr, "insufficient sizes set for %s object %s%p (%i set, %i required).\n", type, obj_name, (void*)obj, obj->n_size, n_size);
        exit(1);
    }
    if( n_flag > obj->n_flag ) {
        fprintf(stderr, "insufficient flags set for %s object %s%p (%i set, %i required).\n", type, obj_name, (void*)obj, obj->n_flag, n_flag);
        exit(1);
    }
    if( n_obj > obj->n_obj ) {
        fprintf(stderr, "insufficient objects set for %s object %s%p (%i set, %i required).\n", type, obj_name, (void*)obj, obj->n_obj, n_obj);
        exit(1);
    }

    /* validate sub-objects */
    for(int i=0; i<obj->n_obj; ++i) {
        object_validate(obj->obj[i]);
    }

    return 0;
}

static int object_add_space(void **list, int *n, int *cap, size_t size) {

    if( *n >= *cap ) {
        void *tmp = NULL;
        int new_cap = *cap * 2 + 1;
        tmp = realloc(*list, new_cap*size);
        if( tmp == NULL ) {
            perror("realloc");
            return -1;
        }
        *list = tmp;
        *cap = new_cap;
    }

    return 0;
}

static int append_vector(vectNd **list, int *n, int *cap, vectNd *vec) {

    /* allocate new list and copy over existing vectors, if needed */
    if( *n >= *cap ) {
        int new_cap = *cap * 2 + 1;
        vectNd *tmp = NULL;
        if( posix_memalign((void**)&tmp, 16, new_cap*sizeof(vectNd)) ) {
            perror("posix_memalign");
            return -1;
        }
        *cap = new_cap;

        for(int i=0; i<*n; ++i) {
            vectNd_alloc(&tmp[i], (*list)[i].n);
            vectNd_copy(&tmp[i], &(*list)[i]);
            vectNd_free(&(*list)[i]);
        }
        free(*list); *list = NULL;
        *list = tmp;
    }

    /* add new vector */
    vectNd_alloc(&(*list)[*n], vec->n);
    vectNd_copy(&(*list)[*n], vec);
    *n += 1;

    return 0;
}

int object_add_pos(object *obj, vectNd *new_pos) {
    return append_vector(&obj->pos, &obj->n_pos, &obj->cap_pos, new_pos);
}

int object_add_posStr(object *obj, char *str) {
    vectNd vec;
    vectNd_calloc(&vec, obj->dimensions);
    vectNd_setStr(&vec, str);

    int ret = append_vector(&obj->pos, &obj->n_pos, &obj->cap_pos, &vec);
    vectNd_free(&vec);

    return ret;
}

int object_add_dir(object *obj, vectNd *new_dir) {
    return append_vector(&obj->dir, &obj->n_dir, &obj->cap_dir, new_dir);
}

int object_add_dirStr(object *obj, char *str) {
    vectNd vec;
    vectNd_calloc(&vec, obj->dimensions);
    vectNd_setStr(&vec, str);

    int ret = append_vector(&obj->dir, &obj->n_dir, &obj->cap_dir, &vec);
    vectNd_free(&vec);

    return ret;
}

int object_add_size(object *obj, double new_size) {
    if( object_add_space((void*)&obj->size, &obj->n_size, &obj->cap_size, sizeof(new_size)) < 0 ) {
        return -1;
    }
    obj->size[obj->n_size] = new_size;
    obj->n_size++;

    return 0;
}

int object_add_flag(object *obj, int new_flag) {
    if( object_add_space((void*)&obj->flag, &obj->n_flag, &obj->cap_flag, sizeof(new_flag)) < 0 ) {
        return -1;
    }
    obj->flag[obj->n_flag] = new_flag;
    obj->n_flag++;

    return 0;
}

int object_add_obj(object *obj, object *new_obj) {
    if( object_add_space((void*)&obj->obj, &obj->n_obj, &obj->cap_obj, sizeof(new_obj)) < 0 ) {
        return -1;
    }
    obj->obj[obj->n_obj] = new_obj;
    obj->n_obj++;
    obj->bounds.radius = 0.0;

    return 0;
}


int object_move(object *obj, vectNd *offset) {
    /* make sure object is complete enough to be manipulated */
    object_validate(obj);

    /* move all positions */
    for(int i=0; i<obj->n_pos; ++i) {
        vectNd_add(&obj->pos[i], offset, &obj->pos[i]);
    }
    vectNd_add(&obj->bounds.center, offset, &obj->bounds.center);

    /* move all sub-objects */
    for(int i=0; i<obj->n_obj; ++i) {
        object_move(obj->obj[i], offset);
    }

    return 0;
}

int object_rotate(object *obj, vectNd *center, int v1, int v2, double angle) {
    /* make sure object is complete enough to be manipulated */
    object_validate(obj);

    /* rotate all positions */
    for(int i=0; i<obj->n_pos; ++i) {
        vectNd_rotate(&obj->pos[i], center, v1, v2, angle, &obj->pos[i]);
    }
    vectNd_rotate(&obj->bounds.center, center, v1, v2, angle, &obj->bounds.center);

    /* rotate all directions */
    for(int i=0; i<obj->n_dir; ++i) {
        vectNd_rotate(&obj->dir[i], NULL, v1, v2, angle, &obj->dir[i]);
    }

    /* rotate all sub-objects */
    for(int i=0; i<obj->n_obj; ++i) {
        object_rotate(obj->obj[i], center, v1, v2, angle);
    }

    return 0;
}

int object_rotate2(object *obj, vectNd *center, vectNd *v1, vectNd *v2, double angle) {
    /* make sure object is complete enough to be manipulated */
    object_validate(obj);

    /* rotate all positions */
    for(int i=0; i<obj->n_pos; ++i) {
        vectNd_rotate2(&obj->pos[i], center, v1, v2, angle, &obj->pos[i]);
    }
    vectNd_rotate2(&obj->bounds.center, center, v1, v2, angle, &obj->bounds.center);

    /* rotate all directions */
    for(int i=0; i<obj->n_dir; ++i) {
        vectNd_rotate2(&obj->dir[i], NULL, v1, v2, angle, &obj->dir[i]);
    }

    /* rotate all sub-objects */
    for(int i=0; i<obj->n_obj; ++i) {
        object_rotate2(obj->obj[i], center, v1, v2, angle);
    }

    return 0;
}

int object_get_bounds(object *obj) {
    bounds_list points;
    bounds_list_init(&points);
    obj->bounding_points(obj, &points);

    if( points.head == NULL ) {
        obj->bounds.radius = -1.0;
        return 0;
    }

    #if 0
    bounds_list_centroid(&points, &obj->bounds.center);
    bounds_list_radius(&points, &obj->bounds.center, &obj->bounds.radius);
    #else
    bounds_list_optimal(&points, &obj->bounds.center, &obj->bounds.radius);
    #endif /* 0 */
    if( obj->bounds.radius > 0.0 )
        obj->bounds.radius += EPSILON;
    bounds_list_free(&points);

    return 0;
}

static inline int vect_object_intersect(object *obj, vectNd *o, vectNd *v, vectNd *res, vectNd *normal, object **obj_ptr, double min_dist) {
    int ret = 0;

    /* make sure bounding sphere is set */
    if( obj->bounds.radius == 0 ) {
        pthread_mutex_lock(&lock);
        /* recheck, with lock */
        if( obj->bounds.radius == 0 )
            object_get_bounds(obj);
        pthread_mutex_unlock(&lock);
    }

    /* check bounding sphere first */
    if( obj->bounds.radius > 0 ) {
        ret = vect_bounding_sphere_intersect(&obj->bounds, o, v, min_dist);
        if( ret <= 0 ) {
            *obj_ptr = NULL;
            return 0;
        }
    } 

    /* check for actual intersection, if it's possible */
    ret = obj->intersect(obj, o, v, res, normal, obj_ptr);

    return ret;
}

#ifndef WITHOUT_KDTREE
int object_kdlist_add(kd_item_list_t *list, object *obj, int obj_id) {

    /* recurse into clusters */
    char typename[OBJ_TYPE_MAX_LEN] = "";
    obj->type_name(typename,sizeof(typename));
    if( !strncmp(typename, "cluster", sizeof(typename)) ) {
        for(int i=0; i<obj->n_obj; ++i) {
            object_kdlist_add(list, obj->obj[i], i);
        }
        return 1;
    }

    /* add non-clusters */
    kd_item_t *item = calloc(1, sizeof(kd_item_t));
    int dimensions = obj->dimensions;
    kd_item_init(item, dimensions);
    vectNd radiuses, with_radius;
    vectNd_alloc(&radiuses, dimensions);
    vectNd_alloc(&with_radius, dimensions);

    bounds_list points;
    bounds_list_init(&points);

    obj->bounding_points(obj, &points);
    bounds_node *curr = points.head;
    while( curr!=NULL ) {
        /* account for the radius of cluster */
        vectNd_fill(&radiuses, fabs(curr->bounds.radius));

        vectNd_copy(&with_radius, &curr->bounds.center);
        vectNd_add(&curr->bounds.center, &radiuses, &with_radius);
        aabb_add_point(&item->bb, &with_radius);

        vectNd_copy(&with_radius, &curr->bounds.center);
        vectNd_sub(&curr->bounds.center, &radiuses, &with_radius);
        aabb_add_point(&item->bb, &with_radius);

        curr = curr->next;
    }
    bounds_list_free(&points);
    vectNd_free(&with_radius);
    vectNd_free(&radiuses);

    item->id = obj_id;
    item->obj_ptr = obj;

    kd_item_list_add(list, item);
    return 1;
}

int trace_kd(vectNd *pos, vectNd *look, kd_tree_t *kd, vectNd *hit, vectNd *hit_normal, object **ptr, double dist_limit) {

    /* get a unit vector inthe direction of v */
    vectNd unit_look;
    vectNd_alloc(&unit_look,look->n);
    vectNd_copy(&unit_look,look);
    vectNd_unitize(&unit_look);

    /* traverse kd-tree to get list of hitable objects */
    int ret = kd_tree_intersect(kd, pos, &unit_look, hit, hit_normal, (void**)ptr, dist_limit);

    vectNd_free(&unit_look);

    return ret;
}
#endif /* !WITHOUT_KDTREE */

int trace(vectNd *pos, vectNd *unit_look, object **objs, int *ids, int n, char *obj_mask, vectNd *hit, vectNd *hit_normal, object **ptr, double *t_ptr, double dist_limit) {
    double min_dist = -1;
    vectNd res;
    vectNd normal;
    int dim = unit_look->n;

    vectNd_alloc(&res,dim);
    vectNd_alloc(&normal,dim);

    /* for each object */
    if( ptr!=NULL )
        *ptr = NULL;
    for(int i=0; i<n; ++i) {

        /* skip objects that have already been checked */
        if( obj_mask && ids ) {
            int id = ids[i];
            if( obj_mask[id] != 0 ) {
                continue;
            }
            obj_mask[id] = 1;
        }

        int ret = VECTND_FAIL;
        double dist = -1;
        object *tmp_ptr = NULL;

        ret = vect_object_intersect(objs[i], pos, unit_look, &res, &normal, &tmp_ptr, min_dist);
        if( ret > 0 ) {
            vectNd_dist(pos,&res,&dist);
            if( dist > EPSILON && (dist+EPSILON < min_dist || min_dist < 0) ) {
                min_dist = dist;
                vectNd_copy(hit,&res);
                vectNd_copy(hit_normal,&normal);
                if( ptr!=NULL )
                    *ptr = tmp_ptr;
            }

            if( dist_limit == 0.0 || dist < dist_limit ) {
                break;
            }
        }
    }

    if( t_ptr != NULL && min_dist > EPSILON ) {
        *t_ptr = min_dist;
    }

    vectNd_free(&normal);
    vectNd_free(&res);

    if( min_dist < 0 )
        return 0;

    return 1;
}
