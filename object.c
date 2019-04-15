/*
 * object.c
 * ndt: n-dimensional tracer
 *
 * Copyright (c) 2019 Bryan Franklin. All rights reserved.
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

    return 0;
}

static int default_reflect(object *obj, vectNd *at, double *red_r, double *green_r, double *blue_r) {
    *red_r = obj->red_r;
    *green_r = obj->green_r;
    *blue_r = obj->blue_r;

    return 0;
}

static int default_trans(object *obj, vectNd *at, int *transparent) {
    *transparent = obj->transparent;

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
    entry->obj.type_name = dlsym(dl_handle, "type_name");
    entry->obj.params = dlsym(dl_handle, "params");
    entry->obj.cleanup = dlsym(dl_handle, "cleanup");
    entry->obj.get_bounds = dlsym(dl_handle, "get_bounds");
    entry->obj.intersect = dlsym(dl_handle, "intersect");
    entry->obj.get_color = dlsym(dl_handle, "get_color");
    if( entry->obj.get_color == NULL )
        entry->obj.get_color = default_color;
    entry->obj.get_reflect = dlsym(dl_handle, "get_reflect");
    if( entry->obj.get_reflect == NULL )
        entry->obj.get_reflect = default_reflect;
    entry->obj.get_trans = dlsym(dl_handle, "get_trans");
    if( entry->obj.get_trans == NULL )
        entry->obj.get_trans = default_trans;
    entry->obj.refract_ray = dlsym(dl_handle, "refract_ray");

    /* check for required functions */
    if( entry->obj.type_name == NULL ) {
        fprintf(stderr, "%s missing type_name function.\n", filename);
        error = 1;
    }
    if( entry->obj.params == NULL ) {
        fprintf(stderr, "%s missing params function.\n", filename);
        error = 2;
    }
    if( entry->obj.get_bounds == NULL ) {
        fprintf(stderr, "%s missing get_bounds function.\n", filename);
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

    printf("\tloaded object from '%s'.\n", filename);

    return 0;
}

int register_objects(char *dirname) {
    if( dirname == NULL ) {
        fprintf(stderr,"%s: dirname is NULL\n", __FUNCTION__);
        return -1;
    }

    /* record current path and change into objects directory, to avoid path
     * separator nonsense. */
    char pwd[PATH_MAX];
    if( getcwd(pwd,sizeof(pwd)) == NULL ) {
        perror("getpwd");
        return -1;
    }
    if( chdir(dirname) < 0 ) {
        perror("chdir");
        return -1;
    }

    /* open directory */
    printf("%s: opening %s\n", __FUNCTION__, dirname);
    DIR *dirp = opendir(".");
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
            /* load every .so file found */
            register_object(dp->d_name);
        }
    }

    closedir(dirp); dirp = NULL;

    /* return to original directory */
    if( chdir(pwd) < 0 ) {
        perror("chdir");
        return -1;
    }

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
    obj->get_bounds = curr->obj.get_bounds;
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

int object_validate(object *obj) {
    char type[256];
    obj->type_name(type, sizeof(type));

    /* check each function pointer */
    if( obj->type_name == NULL ) {
        fprintf(stderr, "type_name not set on object %p.\n", obj);
        return -1;
    }

    if( obj->params == NULL ) {
        fprintf(stderr, "params not set on %s object %p.\n", type, obj);
        return -1;
    }
    if( obj->get_bounds == NULL ) {
        fprintf(stderr, "get_bounds not set on %s object %p.\n", type, obj);
        return -1;
    }
    if( obj->intersect == NULL ) {
        fprintf(stderr, "intersect not set on %s object %p.\n", type, obj);
        return -1;
    }
    if( obj->get_color == NULL ) {
        fprintf(stderr, "get_color not set on %s object %p.\n", type, obj);
        return -1;
    }
    if( obj->get_reflect == NULL ) {
        fprintf(stderr, "get_reflect not set on %s object %p.\n", type, obj);
        return -1;
    }
    if( obj->get_trans == NULL ) {
        fprintf(stderr, "get_trans not set on %s object %p.\n", type, obj);
        return -1;
    }

    /* verify parameter counts */
    int n_pos;
    int n_dir;
    int n_size;
    int n_flag;
    int n_obj;
    obj->params(obj, &n_pos, &n_dir, &n_size, &n_flag, &n_obj);
    if( n_pos > obj->n_pos ) {
        fprintf(stderr, "insufficient positions set for %s object %p (%i set, %i required).\n", type, obj, obj->n_pos, n_pos);
        exit(1);
    }
    if( n_dir > obj->n_dir ) {
        fprintf(stderr, "insufficient directions set for %s object %p (%i set, %i required).\n", type, obj, obj->n_dir, n_dir);
        exit(1);
    }
    if( n_size > obj->n_size ) {
        fprintf(stderr, "insufficient sizes set for %s object %p (%i set, %i required).\n", type, obj, obj->n_size, n_size);
        exit(1);
    }
    if( n_flag > obj->n_flag ) {
        fprintf(stderr, "insufficient flags set for %s object %p (%i set, %i required).\n", type, obj, obj->n_flag, n_flag);
        exit(1);
    }
    if( n_obj > obj->n_obj ) {
        fprintf(stderr, "insufficient objects set for %s object %p (%i set, %i required).\n", type, obj, obj->n_obj, n_obj);
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
        posix_memalign((void**)&tmp, 16, new_cap*sizeof(vectNd));
        if( tmp == NULL ) {
            perror("malloc");
            return -1;
        }
        *cap = new_cap;

        for(int i=0; i<*n; ++i) {
            vectNd_alloc(&tmp[i], (*list)[i].n);
            vectNd_copy(&tmp[i], &(*list)[i]);
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

    return 0;
}


int object_move(object * obj, vectNd *offset) {
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

int object_rotate(object * obj, vectNd *center, int v1, int v2, double angle) {
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

int object_rotate2(object * obj, vectNd *center, vectNd *v1, vectNd *v2, double angle) {
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

static inline int vect_object_intersect(object* obj, vectNd *o, vectNd *v, vectNd *res, vectNd *normal, object **obj_ptr, double min_dist) {
    int ret = 0;

    /* make sure bounding sphere is set */
    if( obj->bounds.radius == 0 ) {
        pthread_mutex_lock(&lock);
        /* recheck, with lock */
        if( obj->bounds.radius == 0 )
            ret = obj->get_bounds(obj);
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

int trace(vectNd *pos, vectNd *look, object **objs, int n, vectNd *hit, vectNd *hit_normal, object **ptr) {
    double min_dist = -1;
    vectNd res;
    vectNd normal;
    int dim = look->n;
    vectNd unit_look;

    vectNd_alloc(&unit_look,dim);
    vectNd_alloc(&res,dim);
    vectNd_alloc(&normal,dim);

    /* for each object */
    if( ptr!=NULL )
        *ptr = NULL;
    vectNd_copy(&unit_look,look);
    vectNd_unitize(&unit_look);
    for(int i=0; i<n; ++i) {
        int ret = VECTND_FAIL;
        double dist = -1;
        object *tmp_ptr = NULL;

        ret = vect_object_intersect(objs[i], pos, &unit_look, &res, &normal, &tmp_ptr, min_dist);
        if( ret > 0 ) {
            vectNd_dist(pos,&res,&dist);
            if( dist > EPSILON && (dist+EPSILON < min_dist || min_dist < 0) ) {
                min_dist = dist;
                vectNd_copy(hit,&res);
                vectNd_copy(hit_normal,&normal);
                if( ptr!=NULL )
                    *ptr = tmp_ptr;
            }
        }
    }

    vectNd_free(&normal);
    vectNd_free(&res);
    vectNd_free(&unit_look);

    if( min_dist < 0 )
        return 0;

    return 1;
}
