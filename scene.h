/*
 * scemne.h
 * ndt: n-dimensional tracer
 *
 * Copyright (c) 2019 Bryan Franklin. All rights reserved.
 */
#ifndef SCENE_H
#define SCENE_H
#include "vectNd.h"
#include "object.h"
#include "camera.h"

/* see:
 * https://stackoverflow.com/questions/9907160/how-to-convert-enum-names-to-string-in-c
 */
#define FOREACH_LIGHT_TYPE(LIGHT_TYPE) \
    LIGHT_TYPE(LIGHT_AMBIENT)   /* no location, no direction */ \
    LIGHT_TYPE(LIGHT_POINT)     /* location, no direction */ \
    LIGHT_TYPE(LIGHT_DIRECTIONAL)  /* no location, direction */ \
    LIGHT_TYPE(LIGHT_SPOT)      /* location, direction */ \
    LIGHT_TYPE(LIGHT_DISK)      /* location, circular area */ \
    LIGHT_TYPE(LIGHT_RECT)      /* location, rectangular area */

#define GENERATE_ENUM(ENUM) ENUM,
#define GENERATE_STRING(STRING) #STRING,

#define LIGHT_NAME_MAX_LEN 32
#define SCENE_NAME_MAX_LEN 256

typedef enum LIGHT_TYPE_ENUM {
    FOREACH_LIGHT_TYPE(GENERATE_ENUM)
} light_type;

extern const char *LIGHT_TYPE_STRING[];

typedef struct light_t
{
    vectNd pos;
    vectNd target;
    vectNd dir;     /* for directional and spot lights */
    vectNd u, v;    /* basis for area lights */
    double radius;  /* radius for disk lights */
    light_type type;
    double red, green, blue;
    double angle;
    vectNd u1, v1;    /* ortho-normal basis for area lights */
    unsigned int prepared:1;
    char name[LIGHT_NAME_MAX_LEN];
} light;

typedef struct scene_t
{
    int dimensions;
   camera cam;
    int num_objects;
    int num_lights;
    object **object_ptrs;
    light **lights;
    light ambient;
    double bg_red, bg_green, bg_blue, bg_alpha;
    char name[SCENE_NAME_MAX_LEN];
} scene;

int scene_init(scene *scn, char *name, int dim);
int scene_free(scene *scn);
int scene_alloc_object(scene *scn, int dimensions, object **obj, char *type);
int scene_remove_object(scene *scn, object *obj);
int scene_alloc_light(scene *scn, light **lgt);
int scene_free_light(light *lgt);
int scene_aim_light(light *lgt, vectNd *target);
int scene_prepare_light(light *lgt);
int scene_validate_objects(scene *scn);
int scene_cluster(scene *scn, int k);
int scene_print(scene *scn);
int scene_find_dupes(scene *scn);
int scene_remove_dupes(scene *scn);

int scene_setup(scene *scn, int dimensions, int frame, int frames, char *config);

#ifdef WITH_YAML
int scene_write_yaml(scene *scn, char *fname);
int scene_write_yaml_buffer(scene *scn, unsigned char **buffer, size_t *length);
int scene_read_yaml(scene *scn, char *fname, int frame);
int scene_read_yaml_buffer(scene *scn, unsigned char *buffer, size_t length, int frame);
int scene_yaml_count_frames(char *fname);
#endif /* WITH_YAML */

#endif /* SCENE_H */
