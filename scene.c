/*
 * scene.c
 * ndt: n-dimensional tracer
 *
 * Copyright (c) 2019 Bryan Franklin. All rights reserved.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#ifdef WITH_YAML
#include <yaml.h>
#include <ctype.h>
#include "object.h"
#endif /* WITH_YAML */
#include "scene.h"
#include "matrix.h"

const char *LIGHT_TYPE_STRING[] = {
    FOREACH_LIGHT_TYPE(GENERATE_STRING)
};

int scene_init(scene *scn, char *name, int dim)
{
    memset(scn,'\0',sizeof(scene));

    scn->object_ptrs = NULL;
    scn->lights = NULL;

    /* record name of scene */
    strncpy(scn->name, name, sizeof(scn->name));
    scn->name[sizeof(scn->name)-1] = '\0';

    /* create camera */
    camera_alloc(&scn->cam,dim);
    camera_init(&scn->cam);

    scn->dimensions = dim;

    return 1;
}

int scene_free(scene *scn)
{
    int i=0;
    for(i=0; i<scn->num_objects; ++i) {
        object_free(scn->object_ptrs[i]);
        free(scn->object_ptrs[i]); scn->object_ptrs[i]=NULL;
    }
    free(scn->object_ptrs); scn->object_ptrs=NULL;
    for(i=0; i<scn->num_lights; ++i) {
        free(scn->lights[i]); scn->lights[i]=NULL;
    }
    free(scn->lights); scn->lights=NULL;
    camera_free(&scn->cam);

    return 1;
}

int scene_add_object(scene *scn, object *obj) {
    object **tmp = NULL;

    tmp = realloc(scn->object_ptrs,(scn->num_objects+1)*sizeof(*obj));
    if( tmp==NULL ) {
        return 0;
    }
    scn->object_ptrs = tmp;
    
    scn->object_ptrs[scn->num_objects] = obj;
    scn->num_objects+=1;

    return 1;
}

int scene_alloc_object(scene *scn, int dimensions, object **obj, char *type)
{
    *obj = object_alloc(dimensions, type, "");
    if( *obj == NULL ) {
        return 0;
    }

    if( ! scene_add_object(scn, *obj) ) {
        free(*obj); *obj = NULL;
        return 0;
    }
    return 1;
}

int scene_remove_object(scene *scn, object *obj)
{
    for(int i=0; i<scn->num_objects; ++i) {
        if( scn->object_ptrs[i] == obj) {
            for(int j=i; j<scn->num_objects-1; ++j) {
                scn->object_ptrs[j] = scn->object_ptrs[j+1];
            }
            scn->object_ptrs[scn->num_objects-1] = NULL;
            scn->num_objects -= 1;
            i -= 1;
        }
    }

    return 0;
}

int scene_alloc_light(scene *scn, light **lgt)
{
    light **tmp = NULL;

    *lgt = calloc(1,sizeof(**lgt));
    tmp = realloc(scn->lights,(scn->num_lights+1)*sizeof(*lgt));
    if( tmp==NULL )
        return 0;
    scn->lights = tmp;
    scn->lights[scn->num_lights] = *lgt;
    memset(*lgt,'\0',sizeof(light));
    (*lgt)->type = LIGHT_POINT;
    scn->num_lights+=1;

    return 1;
}

int scene_aim_light(light *lgt, vectNd *target) {
    if( target->n  > 3 ) {
        printf("%s may give weird results above with more than 3 dimensions.\n", __FUNCTION__);
    }

    vectNd aim_dir;
    vectNd_calloc(&aim_dir,target->n);
    vectNd_sub(target,&lgt->pos,&aim_dir);
    vectNd_unitize(&aim_dir);

    vectNd temp;
    vectNd_alloc(&temp,target->n);

    /* create vector that differs from aim vector */
    vectNd_copy(&temp,&aim_dir);
    if( fabs(aim_dir.v[0]) < EPSILON )
        vectNd_set(&temp,0,1.0);
    else
        vectNd_set(&temp,0,-aim_dir.v[0]);
    /* convert new vector into one that is orthogonal to aim */
    vectNd_orthogonalize(&temp,&aim_dir,&lgt->u,NULL);

    /* create another vector that differs from aim vector */
    vectNd_copy(&temp,&aim_dir);
    if( fabs(aim_dir.v[1]) < EPSILON )
        vectNd_set(&temp,1,1.0);
    else
        vectNd_set(&temp,1,-aim_dir.v[1]);
    /* convert nother new vector into one that is orthogonal to aim */
    vectNd_orthogonalize(&temp,&aim_dir,&lgt->v,NULL);

    return 0;
}

int scene_prepare_light(light *lgt)
{
    if( lgt->type == LIGHT_DISK ||
        lgt->type == LIGHT_RECT ) {
        vectNd_alloc(&lgt->u1, lgt->pos.n);
        vectNd_alloc(&lgt->v1, lgt->pos.n);
        vectNd_orthogonalize(&lgt->u, &lgt->v, &lgt->u1, &lgt->v1);
        vectNd_unitize(&lgt->u1);
        vectNd_unitize(&lgt->v1);
    }
    lgt->prepared = 1;

    return 0;
}

static vectNd sort_pos;

int scene_sort_compar(const void *a, const void *b)
{
    object *left = *(object**)a;
    object *right = *(object**)b;
    double dist_a;
    double dist_b;
    vectNd_dist(&sort_pos,&left->bounds.center,&dist_a);
    dist_a -= left->bounds.radius;
    vectNd_dist(&sort_pos,&right->bounds.center,&dist_b);
    dist_b -= right->bounds.radius;

    if( dist_a > dist_b )
        return 1;
    else if( dist_a < dist_b )
        return -1;
    return 0;
}

int scene_sort_from(scene *scn, vectNd *pos)
{
    vectNd_alloc(&sort_pos,pos->n);   
    vectNd_copy(&sort_pos,pos);
    qsort(scn->object_ptrs,scn->num_objects,sizeof(*scn->object_ptrs),scene_sort_compar);

    vectNd_free(&sort_pos);

    return 0;
}

int scene_validate_objects(scene *scn)
{
    for(int i=0; i<scn->num_objects; ++i) {
        object *obj = scn->object_ptrs[i];
        if( object_validate(obj) != 0 ) {
            fprintf(stderr,"Unable to validate object %i.\n", i);
            return -1;
        }
    }

    return 0;
}

int scene_cluster(scene *scn, int k)
{
    if( scn->num_objects <= 0) {
        return -1;
    }

    /* create a cluster for finite objects */
    object *finite = object_alloc(scn->dimensions, "cluster", "finite");
    object_add_flag(finite, k);

    /* create a second cluster for infinite objects */
    object *infinite = object_alloc(scn->dimensions, "cluster", "infinite");
    object_add_flag(infinite, k);

    /* get bounds for all objects */
    for(int i=0; i<scn->num_objects; ++i) {
        if( object_validate(scn->object_ptrs[i]) < 0 ) {
            fprintf(stderr, "Object '%s' (%p) failed to validate.\n",
                scn->object_ptrs[i]->name, scn->object_ptrs[i]);
        }
        if( scn->object_ptrs[i]->bounds.radius == 0 ) {
            scn->object_ptrs[i]->get_bounds(scn->object_ptrs[i]);
        }
    }
        
    /* sort objects by distance from camera */
    scene_sort_from(scn, &scn->cam.pos);

    /* split objects into infinite and finite top-level clusters */
    for(int i=0; i<scn->num_objects; ++i) {
        if( scn->object_ptrs[i]->bounds.radius >= 0 ) {
            /* add non-infinite objects to finite cluster */
            object_add_obj(finite, scn->object_ptrs[i]);
        } else if( scn->object_ptrs[i]->bounds.radius < 0 ) {
            /* add infinite objects to infinite cluster */
            printf("adding infinite object\n");
            object_add_obj(infinite, scn->object_ptrs[i]);
            /* disable bounding on top level cluster if it contains an infinite
             * object */
            infinite->bounds.radius = -1;
        }
    }

    /* create new list of object pointers */
    free(scn->object_ptrs); scn->object_ptrs=NULL;
    scn->object_ptrs = calloc(2,sizeof(object**));
    scn->num_objects = 0;

    /* replace object list with single cluster without bounds */
    if( finite->n_obj > 0 ) {
        printf("adding cluster of %i finite objects.\n", finite->n_obj);
        scn->object_ptrs[scn->num_objects] = finite;
        scn->num_objects += 1;
    }
    if( infinite->n_obj > 0 ) {
        printf("adding cluster of %i infinite objects.\n", infinite->n_obj);
        scn->object_ptrs[scn->num_objects] = infinite;
        scn->num_objects += 1;
    }

    return 0;
}

int scene_find_dupes(scene *scn)
{
    int i=0;
    int j=0;
    int dupes=0;
    for(i=0; i<scn->num_objects; ++i) {
        for(j=i+1; j<scn->num_objects; ++j) {
            if( scn->object_ptrs[i] == scn->object_ptrs[j] ) {
                printf("Objects %i and %i are the same object with multiple pointers to it. (%p==%p)\n", i, j, scn->object_ptrs[i], scn->object_ptrs[j]);
                continue;
            }

            if( memcmp(scn->object_ptrs[i], scn->object_ptrs[j], sizeof(object))==0 )
            {
                char shape_str[OBJ_TYPE_MAX_LEN];
                scn->object_ptrs[i]->type_name(shape_str,sizeof(shape_str));
                printf("Objects %i and %i are identical (shape=%s)\n", i, j,
                        shape_str);
                dupes++;
            }
        }
    }
    if( dupes > 0 )
        printf("%i duplicate objects found\n", dupes);

    return dupes;
}

int scene_remove_dupes(scene *scn)
{
    int i=0;
    int j=0;
    int dupes=0;
    for(i=0; i<scn->num_objects; ++i) {
        for(j=i+1; j<scn->num_objects; ++j) {
            if( scn->object_ptrs[i] == scn->object_ptrs[j] ) {
                printf("Objects %i and %i are the same object with multiple pointers to it. (%p==%p)\n", i, j, scn->object_ptrs[i], scn->object_ptrs[j]);
                memmove(scn->object_ptrs+j, scn->object_ptrs+j+1,
                    sizeof(object*)*scn->num_objects-j);
                scn->num_objects-=1;
                continue;
            }

            if( memcmp(scn->object_ptrs[i], scn->object_ptrs[j], sizeof(object))==0 )
            {
                free(scn->object_ptrs[j]); scn->object_ptrs[j]=NULL;
                memmove(scn->object_ptrs+j, scn->object_ptrs+j+1,
                    sizeof(object*)*scn->num_objects-j);
                scn->num_objects-=1;
                dupes++;
            }
        }
    }
    printf("%i duplicate objects removed\n", dupes);

    return dupes;
}

int scene_setup(scene *scn, int dimensions, int frame, int frames, char *config)
{
    object *obj = NULL;
    light *lgt = NULL;
    vectNd temp;

    double t = frame/(double)frames;

    scene_init(scn, "test", dimensions);

    vectNd_calloc(&temp, dimensions);

    /* create scene */
    #if 1
    scene_alloc_object(scn, dimensions, &obj, "hplane");
    obj->red = 0.9;
    obj->green = 0.9;
    obj->blue = 0.9;
    obj->red_r = 0.6;
    obj->green_r = 0.6;
    obj->blue_r = 0.6;
    vectNd_reset(&temp);
    vectNd_set(&temp,1,-7);
    object_add_pos(obj, &temp); /* position */
    vectNd_reset(&temp);
    vectNd_set(&temp,1,1);
    object_add_dir(obj, &temp); /* normal */
    #endif /* 0 */

    #if 1
    scene_alloc_object(scn, dimensions, &obj, "sphere");
    obj->red = 0.9;
    obj->green = 0.1;
    obj->blue = 0.1;
    obj->red_r = 0.5;
    obj->green_r = 0.5;
    obj->blue_r = 0.5;
    vectNd_reset(&temp);
    vectNd_set(&temp,2,20);
    vectNd_set(&temp,1,-1);
    object_add_pos(obj, &temp); /* center */
    object_add_size(obj, 5.0);  /* radius */
    obj->transparent = 1;
    obj->refract_index = 2.4;
    #endif /* 0 */

    #if 1
    scene_alloc_object(scn, dimensions, &obj, "hfacet");
    obj->red = 0.9;
    obj->green = 0.9;
    obj->blue = 0.9;
    object_add_posStr(obj, "10,5,25,0");
    object_add_posStr(obj, "-10,5,20,0");
    object_add_posStr(obj, "3,-8,9,4");
    object_add_dirStr(obj, "3,-8,90,4");
    object_add_dirStr(obj, "3,-8,90,4");
    object_add_dirStr(obj, "3,-8,90,4");
    object_add_flag(obj, 0);    /* use normals */
    #endif /* 0 */

    #if 1
    scene_alloc_object(scn, dimensions, &obj, "hcylinder");
    obj->red = 0.1;
    obj->green = 0.9;
    obj->blue = 0.1;
    obj->red_r = 0.1;
    obj->green_r = 0.1;
    obj->blue_r = 0.1;
    object_add_posStr(obj, "-10,-6,20,0");  /* bottom */
    object_add_posStr(obj, "-10,10,20,0");  /* top */
    if( dimensions>3 ) {
        object_add_posStr(obj, "-10,10,36,0");  /* another top */
    }
    if( dimensions>4 ) {
        object_add_posStr(obj, "-10,10,20,-5,10");  /* yet another top */
    }
    object_add_size(obj, 3.0);  /* radius */
    object_add_flag(obj, 1); /* end-style (OPEN) */
    obj->transparent = 1;
    obj->refract_index = 1.33;
    #endif /* 0 */

    /* create camera */
    camera_alloc(&scn->cam,dimensions);
    camera_init(&scn->cam);

    /* move camera into position */
    vectNd viewPoint;
    vectNd viewTarget;
    vectNd_calloc(&viewPoint,dimensions);
    vectNd_calloc(&viewTarget,dimensions);
    vectNd_set(&viewPoint,0,60*cos(2*M_PI*t));
    vectNd_set(&viewPoint,1,40);
    vectNd_set(&viewPoint,2,60*sin(2*M_PI*t));
    vectNd_set(&viewPoint,3,5);
    vectNd_setStr(&viewTarget,"0,-1,20,-5");
    vectNd up_vect;
    vectNd_calloc(&up_vect,dimensions);
    vectNd_set(&up_vect,1,10);
    camera_set_aim(&scn->cam, &viewPoint, &viewTarget, &up_vect, 0.0);

    /* setup lighting */
    vectNd_calloc(&scn->ambient.pos,dimensions);
    scn->ambient.red = .25;
    scn->ambient.green = .25;
    scn->ambient.blue = .25;

    scene_alloc_light(scn,&lgt);
    vectNd_calloc(&lgt->pos,dimensions);
    vectNd_setStr(&lgt->pos,"0,15,15,0");
    lgt->red = 200;
    lgt->green = 200;
    lgt->blue = 200;

    scene_alloc_light(scn,&lgt);
    vectNd_calloc(&lgt->pos,dimensions);
    vectNd_setStr(&lgt->pos,"-16,3,0,1");
    lgt->red = 150;
    lgt->green = 150;
    lgt->blue = 150;

    scene_alloc_light(scn,&lgt);
    vectNd_calloc(&lgt->pos,dimensions);
    vectNd_setStr(&lgt->pos,"16,16,-16,16");
    lgt->red = 150;
    lgt->green = 150;
    lgt->blue = 150;

    printf("\n\nRendering test scene, to render a different scene, use the -s flag.\n");
    printf("\n\tExample:\n\t\tndt -s scenes/balls.so\n\tor:\n");
    #ifdef WITH_YAML
    printf("\t\tndt -s scenes/yaml.so -u filename.yaml\n");
    #endif /* WITH_YAML */
    printf("\n");

    return 1;
}

#ifdef WITH_YAML
/************************
 * Start YAML emmitting *
 ************************/
static int scene_yaml_error(yaml_emitter_t *emitter, yaml_event_t *event) {
    fprintf(stderr, "yaml error occured: %sn", emitter->problem);
    exit(1);

    return 0;
}

static int scene_yaml_emit_event(yaml_emitter_t *emitter, yaml_event_t *event) {
    if (!yaml_emitter_emit(emitter, event))
        scene_yaml_error(emitter,event);

    return 0;
}

static void scene_yaml_start_sequence(yaml_emitter_t *emitter, char *name, char *tag, int flowed) {
    yaml_event_t event;
    memset(&event,'\0',sizeof(event));
    int style = YAML_ANY_SEQUENCE_STYLE;
    if( flowed )
        style = YAML_FLOW_SEQUENCE_STYLE;
    yaml_sequence_start_event_initialize(&event,
        (yaml_char_t *) name,
        (yaml_char_t *) tag,
        tag!=NULL, style);
    scene_yaml_emit_event(emitter, &event);
}

static void scene_yaml_end_sequence(yaml_emitter_t *emitter) {
    yaml_event_t event;
    memset(&event,'\0',sizeof(event));
    yaml_sequence_end_event_initialize(&event);
    scene_yaml_emit_event(emitter, &event);
}

static void scene_yaml_start_mapping(yaml_emitter_t *emitter, char *name, char *tag, int flowed) {
    yaml_event_t event;
    memset(&event,'\0',sizeof(event));
    int style = YAML_ANY_MAPPING_STYLE;
    if( flowed )
        style = YAML_FLOW_MAPPING_STYLE;
    yaml_mapping_start_event_initialize(&event,
        (yaml_char_t *) name,
        (yaml_char_t *) tag,
        0, style);
    scene_yaml_emit_event(emitter, &event);
}

static void scene_yaml_end_mapping(yaml_emitter_t *emitter) {
    yaml_event_t event;
    memset(&event,'\0',sizeof(event));
    yaml_mapping_end_event_initialize(&event);
    scene_yaml_emit_event(emitter, &event);
}

static void scene_yaml_scalar_string(yaml_emitter_t *emitter, char *name, char *tag, char *value) {
    yaml_event_t event;
    memset(&event,'\0',sizeof(event));

    if( value == NULL )
        value = "(null)";

    int length = strlen(value);

    /* check for whitespace in value */
    int quotes = 0;
    char *curr = value;
    while( *curr && !quotes ) {
        quotes = isspace(*curr);
        curr++;
    }

    yaml_scalar_event_initialize(&event,
        (yaml_char_t *) name,
        (yaml_char_t *) tag,
        (yaml_char_t *) value,
        length, !quotes, 1, YAML_ANY_SCALAR_STYLE);
    scene_yaml_emit_event(emitter, &event);
}

static void scene_yaml_scalar_int(yaml_emitter_t *emitter, char *name, char *tag, int value) {
    yaml_event_t event;
    memset(&event,'\0',sizeof(event));

    char valueStr[16];
    int length = snprintf(valueStr, sizeof(valueStr), "%i", value);

    yaml_scalar_event_initialize(&event,
        (yaml_char_t *) name,
        (yaml_char_t *) tag,
        (yaml_char_t *) valueStr,
        length, 1, 1, YAML_ANY_SCALAR_STYLE);
    scene_yaml_emit_event(emitter, &event);
}

static void scene_yaml_scalar_double(yaml_emitter_t *emitter, char *name, char *tag, double value) {
    yaml_event_t event;
    memset(&event,'\0',sizeof(event));

    char valueStr[32];
    int length = snprintf(valueStr, sizeof(valueStr), "%.16g", value);

    yaml_scalar_event_initialize(&event,
        (yaml_char_t *) name,
        (yaml_char_t *) tag,
        (yaml_char_t *) valueStr,
        length, 1, 1, YAML_ANY_SCALAR_STYLE);
    scene_yaml_emit_event(emitter, &event);
}

static void scene_yaml_mapped_string(yaml_emitter_t *emitter, char *name, char *tag, char *value) {
    scene_yaml_scalar_string(emitter,NULL,name,name);
    scene_yaml_scalar_string(emitter,NULL,name,value);
}

static void scene_yaml_mapped_int(yaml_emitter_t *emitter, char *name, char *tag, int value) {
    scene_yaml_scalar_string(emitter,NULL,name,name);
    scene_yaml_scalar_int(emitter,NULL,name,value);
}

static void scene_yaml_mapped_double(yaml_emitter_t *emitter, char *name, char *tag, double value) {
    scene_yaml_scalar_string(emitter,NULL,name,name);
    scene_yaml_scalar_double(emitter,NULL,name,value);
}

static void scene_yaml_emit_vect(yaml_emitter_t *emitter, vectNd *vec) {
    yaml_event_t event;
    memset(&event,'\0',sizeof(event));
    yaml_sequence_start_event_initialize(&event,
        (yaml_char_t *) NULL,
        (yaml_char_t *) NULL,
        0, YAML_FLOW_SEQUENCE_STYLE);
    scene_yaml_emit_event(emitter, &event);

    for(int i=0; i<vec->n; ++i) {
        scene_yaml_scalar_double(emitter,NULL,NULL,vec->v[i]);
    }

    scene_yaml_end_sequence(emitter);
}

static void scene_yaml_mapped_vect(yaml_emitter_t *emitter, char *name, char *tag, vectNd *vec) {
    scene_yaml_scalar_string(emitter,NULL,name,name);
    scene_yaml_emit_vect(emitter,vec);
}

static void scene_yaml_emit_light(yaml_emitter_t *emitter, light *lgt) {
    scene_yaml_start_mapping(emitter, NULL, NULL, 0);

    light_type type = lgt->type;
    scene_yaml_mapped_string(emitter,"type",NULL,(char*)LIGHT_TYPE_STRING[type]);

    /* color/brightness info */
    scene_yaml_scalar_string(emitter,NULL,NULL,"color");
    scene_yaml_start_mapping(emitter, NULL, NULL, 1);
    scene_yaml_mapped_double(emitter, "red", NULL,  lgt->red);
    scene_yaml_mapped_double(emitter, "green", NULL, lgt->green);
    scene_yaml_mapped_double(emitter, "blue", NULL, lgt->blue);
    scene_yaml_end_mapping(emitter);

    /* position, as needed */
    if( type == LIGHT_POINT || type == LIGHT_SPOT
        || type == LIGHT_DISK || type == LIGHT_RECT ) {
        scene_yaml_mapped_vect(emitter,"pos",NULL,&lgt->pos);
    }

    /* direction, as needed */
    if( type == LIGHT_DIRECTIONAL || type == LIGHT_SPOT ) {
        scene_yaml_mapped_vect(emitter,"dir",NULL,&lgt->dir);
    }

    /* 2D basis, as needed */
    if( type == LIGHT_DISK || type == LIGHT_RECT ) {
        scene_yaml_mapped_vect(emitter,"u",NULL,&lgt->u);
        scene_yaml_mapped_vect(emitter,"v",NULL,&lgt->v);
    }
    if( type == LIGHT_DISK ) {
        scene_yaml_mapped_double(emitter, "radius", NULL, lgt->radius);
    }

    /* angle, as needed */
    if( type == LIGHT_SPOT ) {
        scene_yaml_mapped_double(emitter,"angle", NULL, lgt->angle);
    }

    if( lgt->prepared ) {
        scene_yaml_mapped_double(emitter, "prepared", NULL, lgt->prepared);
        if( type == LIGHT_DISK || type == LIGHT_RECT ) {
            scene_yaml_mapped_vect(emitter,"u1",NULL,&lgt->u1);
            scene_yaml_mapped_vect(emitter,"v1",NULL,&lgt->v1);
        }
    }

    scene_yaml_end_mapping(emitter);
}

static void scene_yaml_emit_camera(yaml_emitter_t *emitter, camera *cam) {
    scene_yaml_scalar_string(emitter, NULL, NULL, "camera");
    scene_yaml_start_mapping(emitter, NULL, NULL, 0);

    scene_yaml_mapped_vect(emitter,"viewPoint",NULL,&cam->viewPoint);
    scene_yaml_mapped_vect(emitter,"viewTarget",NULL,&cam->viewTarget);
    double up_len = 0.0;
    vectNd_l2norm(&cam->up, &up_len);
    if( up_len > 0.0 )
        scene_yaml_mapped_vect(emitter,"up",NULL,&cam->up);
    if( cam->rotation != 0 )
        scene_yaml_mapped_double(emitter,"rotation", NULL, cam->rotation);
    if( cam->eye_offset != EYE_OFFSET )
        scene_yaml_mapped_double(emitter,"eye_offset", NULL, cam->eye_offset);
    if( cam->flip_x != 0 )
        scene_yaml_mapped_int(emitter,"flip_x", NULL, cam->flip_x);
    if( cam->flip_y != 0 )
        scene_yaml_mapped_int(emitter,"flip_y", NULL, cam->flip_y);
    if( cam->zoom != 1.0 )
        scene_yaml_mapped_double(emitter,"zoom", NULL, cam->zoom);

    if( cam->aperture_radius != 0 ) {
        scene_yaml_mapped_double(emitter,"aperture_radius", NULL, cam->aperture_radius);
        scene_yaml_mapped_double(emitter,"focal_distance", NULL, cam->focal_distance);
    }

    if( cam->prepared ) {
        scene_yaml_mapped_int(emitter,"prepared", NULL, cam->prepared);

        scene_yaml_mapped_double(emitter,"leveling",NULL, cam->leveling);

        scene_yaml_mapped_vect(emitter,"pos",NULL,&cam->pos);
        scene_yaml_mapped_vect(emitter,"leftEye",NULL,&cam->leftEye);
        scene_yaml_mapped_vect(emitter,"rightEye",NULL,&cam->rightEye);
        scene_yaml_mapped_vect(emitter,"dirX",NULL,&cam->dirX);
        scene_yaml_mapped_vect(emitter,"dirY",NULL,&cam->dirY);
        scene_yaml_mapped_vect(emitter,"imgOrig",NULL,&cam->imgOrig);

        scene_yaml_mapped_vect(emitter,"localX",NULL,&cam->localX);
        scene_yaml_mapped_vect(emitter,"localY",NULL,&cam->localY);
        scene_yaml_mapped_vect(emitter,"localZ",NULL,&cam->localZ);
    }

    scene_yaml_end_mapping(emitter);
}

static void scene_yaml_emit_object(yaml_emitter_t *emitter, object *obj) {
    scene_yaml_start_mapping(emitter, NULL, NULL, 0);

    /* name (optional) */
    if( obj->name[0] != '\0' ) {
        scene_yaml_mapped_string(emitter, "name", NULL, obj->name);
    }

    /* type */
    char type_name[OBJ_TYPE_MAX_LEN];
    obj->type_name(type_name,sizeof(type_name));
    scene_yaml_mapped_string(emitter, "type", NULL, type_name);

    scene_yaml_mapped_int(emitter, "dimensions", NULL, obj->dimensions);

    /* material info */
    scene_yaml_scalar_string(emitter,NULL,NULL,"material");
    scene_yaml_start_mapping(emitter, NULL, NULL, 0);
    if( obj->transparent ) {
        scene_yaml_mapped_int(emitter, "transparent", NULL, obj->transparent);
        scene_yaml_mapped_double(emitter, "refract_index", NULL, obj->refract_index);
    }
    scene_yaml_scalar_string(emitter,NULL,NULL,"color");
    scene_yaml_start_mapping(emitter, NULL, NULL, 1);
    scene_yaml_mapped_double(emitter, "red", NULL, obj->red);
    scene_yaml_mapped_double(emitter, "green", NULL, obj->green);
    scene_yaml_mapped_double(emitter, "blue", NULL, obj->blue);
    scene_yaml_end_mapping(emitter);    /* color */

    if( obj->red_r != 0 && obj->green_r != 0 && obj->blue_r != 0 ) {
        scene_yaml_scalar_string(emitter,NULL,NULL,"reflectivity");
        scene_yaml_start_mapping(emitter, NULL, NULL, 1);
        scene_yaml_mapped_double(emitter, "red", NULL, obj->red_r);
        scene_yaml_mapped_double(emitter, "green", NULL, obj->green_r);
        scene_yaml_mapped_double(emitter, "blue", NULL, obj->blue_r);
        scene_yaml_end_mapping(emitter);    /* reflectivity */
    }
    scene_yaml_end_mapping(emitter);    /* material */

    /* positions */
    if( obj->n_pos > 0 ) {
        scene_yaml_scalar_string(emitter,NULL,NULL,"positions");
        scene_yaml_start_sequence(emitter, NULL, NULL, 0);
        for(int i=0; i<obj->n_pos; ++i) {
            scene_yaml_emit_vect(emitter, &obj->pos[i]);
        }
        scene_yaml_end_sequence(emitter);
    }

    /* directions */
    if( obj->n_dir > 0 ) {
        scene_yaml_scalar_string(emitter,NULL,NULL,"directions");
        scene_yaml_start_sequence(emitter, NULL, NULL, 0);
        for(int i=0; i<obj->n_dir; ++i) {
            scene_yaml_emit_vect(emitter, &obj->dir[i]);
        }
        scene_yaml_end_sequence(emitter);
    }

    /* sizes */
    if( obj->n_size > 0 ) {
        scene_yaml_scalar_string(emitter,NULL,NULL,"sizes");
        scene_yaml_start_sequence(emitter, NULL, NULL, 1);
        for(int i=0; i<obj->n_size; ++i) {
            scene_yaml_scalar_double(emitter, NULL, NULL, obj->size[i]);
        }
        scene_yaml_end_sequence(emitter);
    }

    /* flags */
    if( obj->n_flag > 0 ) {
        scene_yaml_scalar_string(emitter,NULL,NULL,"flags");
        scene_yaml_start_sequence(emitter, NULL, NULL, 1);
        for(int i=0; i<obj->n_flag; ++i) {
            scene_yaml_scalar_int(emitter, NULL, NULL, obj->flag[i]);
        }
        scene_yaml_end_sequence(emitter);
    }

    /* objects */
    if( obj->n_obj > 0 ) {
        scene_yaml_scalar_string(emitter,NULL,NULL,"objects");
        scene_yaml_start_sequence(emitter, NULL, NULL, 0);
        for(int i=0; i<obj->n_obj; ++i) {
            scene_yaml_emit_object(emitter, obj->obj[i]);
        }
        scene_yaml_end_sequence(emitter);
    }

    /* bounding sphere */
    if( obj->prepared ) {
        scene_yaml_mapped_int(emitter, "prepared", NULL, obj->prepared);
        if( obj->prepared )
            fprintf(stderr, "Warning: exporting of prepared objects to YAML not fully supported, expect weirdness!\n");
        scene_yaml_scalar_string(emitter,NULL,NULL,"bounds");
        scene_yaml_start_mapping(emitter, NULL, NULL, 0);
        scene_yaml_mapped_vect(emitter, "center", NULL, &obj->bounds.center);
        scene_yaml_mapped_double(emitter, "radius", NULL, obj->bounds.radius);
        scene_yaml_end_mapping(emitter);    /* bounds */
    }

    scene_yaml_end_mapping(emitter);    /* object */
}

/* see: https://pyyaml.org/wiki/LibYAML#emitter-api-synopsis */
static int scene_yaml_emit_scene(yaml_emitter_t *emitter, scene *scn) {

    int implicit = 1;
    yaml_event_t event;

    /* start stream */
    yaml_stream_start_event_initialize(&event, YAML_UTF8_ENCODING);
    scene_yaml_emit_event(emitter, &event);

    /* start document */
    yaml_document_start_event_initialize(&event, NULL, NULL, NULL, 0);
    scene_yaml_emit_event(emitter, &event);

    /* start scene mapping */
    scene_yaml_start_mapping(emitter, NULL, NULL, 0);

    /* write basic scene data */
    scene_yaml_mapped_string(emitter, "scene", "name", scn->name);

    scene_yaml_mapped_int(emitter, "dimensions", NULL, scn->dimensions);

    if( scn->bg_red != 0 || scn->bg_green != 0 || scn->bg_blue != 0 ) {
        scene_yaml_scalar_string(emitter, NULL, NULL, "background");
        scene_yaml_start_mapping(emitter, NULL, NULL, 1);
        scene_yaml_mapped_double(emitter, "red", NULL,  scn->bg_red);
        scene_yaml_mapped_double(emitter, "green", NULL, scn->bg_green);
        scene_yaml_mapped_double(emitter, "blue", NULL, scn->bg_blue);
        scene_yaml_end_mapping(emitter);
    }

    /* write camera info */
    scene_yaml_emit_camera(emitter, &scn->cam);

    /* write list of lights */
    scene_yaml_scalar_string(emitter, NULL, NULL, "lights");
    scene_yaml_start_sequence(emitter, NULL, NULL, 0);

    if( scn->ambient.red != 0.0 || scn->ambient.green != 0.0 || scn->ambient.blue != 0.0 ) {
        scene_yaml_emit_light(emitter, &scn->ambient);
    }

    for(int i=0; i<scn->num_lights; ++i) {
        scene_yaml_emit_light(emitter, scn->lights[i]);
    }

    scene_yaml_end_sequence(emitter);

    /* write list of objects */
    scene_yaml_scalar_string(emitter, NULL, NULL, "objects");
    scene_yaml_start_sequence(emitter, NULL, NULL, 0);

    for(int i=0; i<scn->num_objects; ++i) {
        scene_yaml_emit_object(emitter, scn->object_ptrs[i]);
    }

    scene_yaml_end_sequence(emitter);
    scene_yaml_end_mapping(emitter);

    /* clean up */
    yaml_document_end_event_initialize(&event, implicit);
    scene_yaml_emit_event(emitter, &event);
    yaml_stream_end_event_initialize(&event);
    scene_yaml_emit_event(emitter, &event);
    yaml_emitter_delete(emitter);

    return 0;
}

int scene_write_yaml(scene *scn, char *fname) {
    printf("%s writing to '%s'.\n", __FUNCTION__, fname);

    /* create output file */
    FILE *fp = fopen(fname, "wb");
    if( fp == NULL ) {
        perror("fopen");
        exit(1);
    }
    
    /* initialize YAML emitter */
    yaml_emitter_t emitter;
    yaml_emitter_initialize(&emitter);
    yaml_emitter_set_output_file(&emitter, fp);

    int ret = scene_yaml_emit_scene(&emitter, scn);

    yaml_emitter_delete(&emitter);
    fclose(fp); fp=NULL;

    return ret;
}

/**********************
 * Start YAML parsing *
 **********************/
/* see: https://pyyaml.org/wiki/LibYAML#parser-api-synopsis */
static int scene_yaml_parse_string(yaml_parser_t *parser, char *value, int size) {

    yaml_event_t event;
    /* Get the next event. */
    if (!yaml_parser_parse(parser, &event)) {
        fprintf(stderr, "Parse error, %i\n", parser->error);
        exit(1);
    }
    yaml_event_type_t type = event.type;

    if( type != YAML_SCALAR_EVENT ) {
        return -1;
    }
        
    /* process event */
    char *valueStr = (char*)event.data.scalar.value;
    if( valueStr ) {
        strncpy(value, valueStr, size);
    }

    /* cleanup event */
    yaml_event_delete(&event);

    return 0;
}

static int scene_yaml_parse_double(yaml_parser_t *parser, double *value) {

    yaml_event_t event;
    /* Get the next event. */
    if (!yaml_parser_parse(parser, &event)) {
        fprintf(stderr, "Parse error, %i\n", parser->error);
        exit(1);
    }
    yaml_event_type_t type = event.type;

    if( type != YAML_SCALAR_EVENT ) {
        return -1;
    }
        
    /* process event */
    char *valueStr = (char*)event.data.scalar.value;
    if( valueStr ) {
        *value = atof(valueStr);
    }

    /* cleanup event */
    yaml_event_delete(&event);

    return 0;
}

static int scene_yaml_parse_int(yaml_parser_t *parser, int *value) {

    yaml_event_t event;
    /* Get the next event. */
    if (!yaml_parser_parse(parser, &event)) {
        fprintf(stderr, "Parse error, %i\n", parser->error);
        exit(1);
    }
    yaml_event_type_t type = event.type;

    if( type != YAML_SCALAR_EVENT ) {
        return -1;
    }
        
    /* process event */
    char *valueStr = (char*)event.data.scalar.value;
    if( valueStr ) {
        *value = atoi(valueStr);
    }

    /* cleanup event */
    yaml_event_delete(&event);

    return 0;
}

static int scene_yaml_parse_vect(yaml_parser_t *parser, vectNd *vec) {

    int done = 0;
    yaml_event_t event;
    int n=0;
    int cap=1;
    double *values = calloc(cap,sizeof(double));
    while( !done ) {
        /* Get the next event. */
        if (!yaml_parser_parse(parser, &event)) {
            fprintf(stderr, "Parse error, %i\n", parser->error);
            exit(1);
        }
        yaml_event_type_t type = event.type;

        if( type == YAML_SEQUENCE_END_EVENT ) {
            done = 1;
            continue;
        }
        
        /* process event */
        if (type == YAML_SCALAR_EVENT) {
            char *value = (char*)event.data.scalar.value;
            if( value ) {
                /* resize, as needed */
                if( n >= cap ) {
                    double *tmp = NULL;
                    int new_cap = cap * 2 + 1;
                    tmp = realloc(values, new_cap*sizeof(double));
                    if( tmp == NULL ) {
                        perror("realloc");
                        return -1;
                    }
                    values = tmp;
                    cap = new_cap;
                }

                /* store values */
                values[n++] = atof(value);
            }
        }

        /* cleanup event */
        yaml_event_delete(&event);
    }

    /* fill in vector as read */
    vectNd_calloc(vec, n);
    for(int i=0; i<n; ++i) {
        vectNd_set(vec, i, values[i]);
    }
    free(values); values=NULL;
    n = cap = 0;

    return 0;
}

static int scene_yaml_parse_color(yaml_parser_t *parser, double *red, double *green, double *blue) {

    yaml_event_t event;

    /* Get the first event. */
    if (!yaml_parser_parse(parser, &event)) {
        fprintf(stderr, "Parse error, %i\n", parser->error);
        exit(1);
    }
    yaml_event_type_t type = event.type;
    yaml_event_delete(&event);

    if( type != YAML_MAPPING_START_EVENT ) {
        fprintf(stderr, "%s: Expected start of map.\n", __FUNCTION__);
        return -1;
    }

    int done = 0;
    while( !done ) {
        /* Get the next event. */
        if (!yaml_parser_parse(parser, &event)) {
            fprintf(stderr, "Parse error, %i\n", parser->error);
            exit(1);
        }
        yaml_event_type_t type = event.type;

        if( type == YAML_MAPPING_END_EVENT ) {
            done = 1;
            continue;
        }
        
        /* process event */
        if (type == YAML_SCALAR_EVENT) {
            char *value = (char*)event.data.scalar.value;
            if( value ) {
                if( strcasecmp("red", value) == 0 ) {
                    scene_yaml_parse_double(parser, red);
                } else if( strcasecmp("green", value) == 0 ) {
                    scene_yaml_parse_double(parser, green);
                } else if( strcasecmp("blue", value) == 0 ) {
                    scene_yaml_parse_double(parser, blue);
                } else {
                    fprintf(stderr, "%s: Unhandled color component '%s'.\n", __FUNCTION__, value);
                }
            }
        }

        /* cleanup event */
        yaml_event_delete(&event);
    }

    return 0;
}

static int scene_yaml_parse_material(yaml_parser_t *parser, object *obj) {

    yaml_event_t event;

    /* Get the first event. */
    if (!yaml_parser_parse(parser, &event)) {
        fprintf(stderr, "Parse error, %i\n", parser->error);
        exit(1);
    }
    yaml_event_type_t type = event.type;
    yaml_event_delete(&event);

    if( type != YAML_MAPPING_START_EVENT ) {
        fprintf(stderr, "%s: Expected start of mapping.\n", __FUNCTION__);
        return -1;
    }

    int done = 0;
    while( !done ) {
        /* Get the next event. */
        if (!yaml_parser_parse(parser, &event)) {
            fprintf(stderr, "Parse error, %i\n", parser->error);
            exit(1);
        }
        yaml_event_type_t type = event.type;

        if( type == YAML_MAPPING_END_EVENT ) {
            done = 1;
            continue;
        }
        
        /* process event */
        if (type == YAML_SCALAR_EVENT) {
            char *value = (char*)event.data.scalar.value;
            if( value ) {
                if( strcasecmp("color", value) == 0) {
                    scene_yaml_parse_color(parser,&obj->red, &obj->green, &obj->blue);
                } else if( strcasecmp("reflectivity", value) == 0) {
                    scene_yaml_parse_color(parser,&obj->red_r, &obj->green_r, &obj->blue_r);
                } else if( strcasecmp("transparent", value) == 0) {
                    int v = 0;
                    scene_yaml_parse_int(parser,&v);
                    obj->transparent = v;
                } else if( strcasecmp("prepared", value) == 0) {
                    int v = 0;
                    scene_yaml_parse_int(parser,&v);
                    obj->prepared = v;
                } else if( strcasecmp("refract_index", value) == 0) {
                    scene_yaml_parse_double(parser, &obj->refract_index);
                } else {
                    fprintf(stderr, "%s: Unhandled key '%s'.\n", __FUNCTION__, value);
                }
            }
        }

        /* cleanup event */
        yaml_event_delete(&event);
    }

    return 0;
}

static int scene_yaml_parse_bounds(yaml_parser_t *parser, object *obj) {

    yaml_event_t event;

    /* Get the first event. */
    if (!yaml_parser_parse(parser, &event)) {
        fprintf(stderr, "Parse error, %i\n", parser->error);
        exit(1);
    }
    yaml_event_type_t type = event.type;
    yaml_event_delete(&event);

    if( type != YAML_MAPPING_START_EVENT ) {
        fprintf(stderr, "%s: Expected start of map.\n", __FUNCTION__);
        return -1;
    }

    int done = 0;
    while( !done ) {
        /* Get the next event. */
        if (!yaml_parser_parse(parser, &event)) {
            fprintf(stderr, "Parse error, %i\n", parser->error);
            exit(1);
        }
        yaml_event_type_t type = event.type;

        if( type == YAML_MAPPING_END_EVENT ) {
            done = 1;
            continue;
        }
        
        /* process event */
        if (type == YAML_SCALAR_EVENT) {
            char *value = (char*)event.data.scalar.value;
            if( value ) {
                if( strcasecmp("radius", value) == 0 ) {
                    scene_yaml_parse_double(parser, &obj->bounds.radius);
                } else if( strcasecmp("center", value) == 0 ) {
                    scene_yaml_parse_vect(parser, &obj->bounds.center);
                } else {
                    fprintf(stderr, "%s: Unhandled bounds component '%s'.\n", __FUNCTION__, value);
                }
            }
        }

        /* cleanup event */
        yaml_event_delete(&event);
    }

    return 0;
}

static int scene_yaml_parse_positions(yaml_parser_t *parser, object *obj) {

    yaml_event_t event;

    /* Get the first event. */
    if (!yaml_parser_parse(parser, &event)) {
        fprintf(stderr, "Parse error, %i\n", parser->error);
        exit(1);
    }
    yaml_event_type_t type = event.type;
    yaml_event_delete(&event);

    if( type != YAML_SEQUENCE_START_EVENT ) {
        fprintf(stderr, "%s: Expected start of list.\n", __FUNCTION__);
        return -1;
    }

    /* process list */
    int done = 0;
    while( !done ) {
        /* Get the next event. */
        if (!yaml_parser_parse(parser, &event)) {
            fprintf(stderr, "Parse error, %i\n", parser->error);
            exit(1);
        }
        yaml_event_type_t type = event.type;

        if( type == YAML_SEQUENCE_END_EVENT ) {
            done = 1;
            continue;
        }
        
        /* process event */
        if (type == YAML_SEQUENCE_START_EVENT) {
            vectNd vec;
            scene_yaml_parse_vect(parser, &vec);
            object_add_pos(obj, &vec);
        }

        /* cleanup event */
        yaml_event_delete(&event);
    }

    return 0;
}

static int scene_yaml_parse_directions(yaml_parser_t *parser, object *obj) {

    yaml_event_t event;

    /* Get the first event. */
    if (!yaml_parser_parse(parser, &event)) {
        fprintf(stderr, "Parse error, %i\n", parser->error);
        exit(1);
    }
    yaml_event_type_t type = event.type;
    yaml_event_delete(&event);

    if( type != YAML_SEQUENCE_START_EVENT ) {
        fprintf(stderr, "%s: Expected start of list.\n", __FUNCTION__);
        return -1;
    }

    /* process list */
    int done = 0;
    while( !done ) {
        /* Get the next event. */
        if (!yaml_parser_parse(parser, &event)) {
            fprintf(stderr, "Parse error, %i\n", parser->error);
            exit(1);
        }
        yaml_event_type_t type = event.type;

        if( type == YAML_SEQUENCE_END_EVENT ) {
            done = 1;
            continue;
        }
        
        /* process event */
        if (type == YAML_SEQUENCE_START_EVENT) {
            vectNd vec;
            scene_yaml_parse_vect(parser, &vec);
            object_add_dir(obj, &vec);
        }

        /* cleanup event */
        yaml_event_delete(&event);
    }

    return 0;
}

static int scene_yaml_parse_sizes(yaml_parser_t *parser, object *obj) {

    yaml_event_t event;

    /* Get the first event. */
    if (!yaml_parser_parse(parser, &event)) {
        fprintf(stderr, "Parse error, %i\n", parser->error);
        exit(1);
    }
    yaml_event_type_t type = event.type;
    yaml_event_delete(&event);

    if( type != YAML_SEQUENCE_START_EVENT ) {
        fprintf(stderr, "%s: Expected start of list.\n", __FUNCTION__);
        return -1;
    }

    /* process list */
    int done = 0;
    while( !done ) {
        /* Get the next event. */
        if (!yaml_parser_parse(parser, &event)) {
            fprintf(stderr, "Parse error, %i\n", parser->error);
            exit(1);
        }
        yaml_event_type_t type = event.type;

        if( type == YAML_SEQUENCE_END_EVENT ) {
            done = 1;
            continue;
        }
        
        /* process event */
        if (type == YAML_SCALAR_EVENT) {
            double size = atof((char*)event.data.scalar.value);
            object_add_size(obj, size);
        }

        /* cleanup event */
        yaml_event_delete(&event);
    }

    return 0;
}

static int scene_yaml_parse_flags(yaml_parser_t *parser, object *obj) {

    yaml_event_t event;

    /* Get the first event. */
    if (!yaml_parser_parse(parser, &event)) {
        fprintf(stderr, "Parse error, %i\n", parser->error);
        exit(1);
    }
    yaml_event_type_t type = event.type;
    yaml_event_delete(&event);

    if( type != YAML_SEQUENCE_START_EVENT ) {
        fprintf(stderr, "%s: Expected start of list.\n", __FUNCTION__);
        return -1;
    }

    /* process list */
    int done = 0;
    while( !done ) {
        /* Get the next event. */
        if (!yaml_parser_parse(parser, &event)) {
            fprintf(stderr, "Parse error, %i\n", parser->error);
            exit(1);
        }
        yaml_event_type_t type = event.type;

        if( type == YAML_SEQUENCE_END_EVENT ) {
            done = 1;
            continue;
        }
        
        /* process event */
        if (type == YAML_SCALAR_EVENT) {
            double flag = atoi((char*)event.data.scalar.value);
            object_add_flag(obj, flag);
        }

        /* cleanup event */
        yaml_event_delete(&event);
    }

    return 0;
}

static int scene_yaml_parse_object(yaml_parser_t *parser, object **obj);

static int scene_yaml_parse_subobjects(yaml_parser_t *parser, object *obj) {

    yaml_event_t event;

    /* Get the first event. */
    if (!yaml_parser_parse(parser, &event)) {
        fprintf(stderr, "Parse error, %i\n", parser->error);
        exit(1);
    }
    yaml_event_type_t type = event.type;
    yaml_event_delete(&event);

    if( type != YAML_SEQUENCE_START_EVENT ) {
        fprintf(stderr, "%s: Expected start of list.\n", __FUNCTION__);
        return -1;
    }

    /* process list */
    int done = 0;
    while( !done ) {
        /* Get the next event. */
        if (!yaml_parser_parse(parser, &event)) {
            fprintf(stderr, "Parse error, %i\n", parser->error);
            exit(1);
        }
        yaml_event_type_t type = event.type;

        if( type == YAML_SEQUENCE_END_EVENT ) {
            done = 1;
            continue;
        }
        
        /* process event */
        if (type == YAML_MAPPING_START_EVENT) {
            object *obj = NULL;
            scene_yaml_parse_object(parser, &obj);
            object_add_obj(obj, obj);
        }

        /* cleanup event */
        yaml_event_delete(&event);
    }

    return 0;
}

static int scene_yaml_parse_object(yaml_parser_t *parser, object **obj) {

    int done = 0;
    yaml_event_t event;
    object *tmp = calloc(1,sizeof(object));
    char typeStr[OBJ_TYPE_MAX_LEN] = "unspecified";
    int dimensions=0;
    while( !done ) {
        /* Get the next event. */
        if (!yaml_parser_parse(parser, &event)) {
            fprintf(stderr, "Parse error, %i\n", parser->error);
            exit(1);
        }
        yaml_event_type_t type = event.type;

        if( type == YAML_MAPPING_END_EVENT ) {
            done = 1;
            continue;
        }
        
        /* process event */
        if (type == YAML_SCALAR_EVENT) {
            char *value = (char*)event.data.scalar.value;
            if( value ) {
                if( strcasecmp("type", value) == 0 ) {
                    scene_yaml_parse_string(parser, typeStr, sizeof(typeStr));
                } else if( strcasecmp("dimensions", value) == 0 ) {
                    scene_yaml_parse_int(parser, &dimensions);
                } else if( strcasecmp("name", value) == 0 ) {
                    scene_yaml_parse_string(parser, tmp->name, sizeof(tmp->name));
                } else if( strcasecmp("prepared", value) == 0 ) {
                    int v=0;
                    scene_yaml_parse_int(parser, &v);
                    tmp->prepared = v;
                } else if( strcasecmp("material", value) == 0 ) {
                    scene_yaml_parse_material(parser, tmp);
                } else if( strcasecmp("positions", value) == 0 ) {
                    scene_yaml_parse_positions(parser, tmp);
                } else if( strcasecmp("directions", value) == 0 ) {
                    scene_yaml_parse_directions(parser, tmp);
                } else if( strcasecmp("sizes", value) == 0 ) {
                    scene_yaml_parse_sizes(parser, tmp);
                } else if( strcasecmp("flags", value) == 0 ) {
                    scene_yaml_parse_flags(parser, tmp);
                } else if( strcasecmp("objects", value) == 0 ) {
                    scene_yaml_parse_subobjects(parser, tmp);
                } else if( strcasecmp("bounds", value) == 0 ) {
                    scene_yaml_parse_bounds(parser, tmp);
                } else {
                    fprintf(stderr,"%s: Unhandled key '%s'.\n", __FUNCTION__, value);
                }
            }
        }

        /* cleanup event */
        yaml_event_delete(&event);
    }

    /* copy collected data into an actual object */
    *obj = object_alloc(dimensions, typeStr, tmp->name);

    /* copy data here! */
    (*obj)->red = tmp->red;
    (*obj)->green = tmp->green;
    (*obj)->blue = tmp->blue;
    (*obj)->red_r = tmp->red_r;
    (*obj)->green_r = tmp->green_r;
    (*obj)->blue_r = tmp->blue_r;
    (*obj)->transparent = tmp->transparent;
    (*obj)->refract_index = tmp->refract_index;
    (*obj)->prepared = tmp->prepared;
    if( (*obj)->prepared ) {
        fprintf(stderr, "Warning: loading of prepared objects not fully supported, expect weirdness!\n");
        (*obj)->prepared = 0;
        (*obj)->bounds.radius = tmp->bounds.radius;
        vectNd_copy(&(*obj)->bounds.center, &tmp->bounds.center);
    }
    strncpy((*obj)->name, tmp->name, sizeof((*obj)->name));

    for(int i=0; i<tmp->n_pos; ++i)
        object_add_pos(*obj, &tmp->pos[i]);
    for(int i=0; i<tmp->n_dir; ++i)
        object_add_dir(*obj, &tmp->dir[i]);
    for(int i=0; i<tmp->n_size; ++i)
        object_add_size(*obj, tmp->size[i]);
    for(int i=0; i<tmp->n_flag; ++i)
        object_add_flag(*obj, tmp->flag[i]);
    for(int i=0; i<tmp->n_obj; ++i)
        object_add_obj(*obj, tmp->obj[i]);
    tmp->n_obj = tmp->cap_obj = 0;  /* prevent removal objects referenced by *obj */

    if( object_validate(*obj) < 0 ) {
        fprintf(stderr, "%s: loaded %s failed to validate.\n", __FUNCTION__, typeStr);
        object_free(*obj); *obj=NULL;
    }

    /* cleanup */
    object_free(tmp); tmp=NULL;

    return 0;
}

static int scene_yaml_parse_objects(yaml_parser_t *parser, scene *scn) {

    yaml_event_t event;

    /* Get the first event. */
    if (!yaml_parser_parse(parser, &event)) {
        fprintf(stderr, "Parse error, %i\n", parser->error);
        exit(1);
    }
    yaml_event_type_t type = event.type;
    yaml_event_delete(&event);

    if( type != YAML_SEQUENCE_START_EVENT ) {
        fprintf(stderr, "%s: Expected start of list.\n", __FUNCTION__);
        return -1;
    }

    /* process list */
    int done = 0;
    while( !done ) {
        /* Get the next event. */
        if (!yaml_parser_parse(parser, &event)) {
            fprintf(stderr, "Parse error, %i\n", parser->error);
            exit(1);
        }
        yaml_event_type_t type = event.type;

        if( type == YAML_SEQUENCE_END_EVENT ) {
            done = 1;
            continue;
        }
        
        /* process event */
        if (type == YAML_MAPPING_START_EVENT) {
            object *obj = NULL;
            scene_yaml_parse_object(parser, &obj);
            if( obj ) 
                scene_add_object(scn, obj);
        }

        /* cleanup event */
        yaml_event_delete(&event);
    }

    return 0;
}

static int scene_yaml_parse_light(yaml_parser_t *parser, light *lgt) {

    int done = 0;
    yaml_event_t event;
    while( !done ) {
        /* Get the next event. */
        if (!yaml_parser_parse(parser, &event)) {
            fprintf(stderr, "Parse error, %i\n", parser->error);
            exit(1);
        }
        yaml_event_type_t type = event.type;

        if( type == YAML_MAPPING_END_EVENT ) {
            done = 1;
            continue;
        }
        
        /* process event */
        if (type == YAML_SCALAR_EVENT) {
            char *value = (char*)event.data.scalar.value;
            if( value ) {
                if( strcasecmp("type", value) == 0 ) {
                    char buffer[64];
                    scene_yaml_parse_string(parser, buffer, sizeof(buffer));
                    if( strcasecmp("LIGHT_AMBIENT", buffer) == 0 
                        || strcasecmp("AMBIENT", buffer) == 0 ) {
                        lgt->type = LIGHT_AMBIENT;
                    } else if( strcasecmp("LIGHT_POINT", buffer) == 0 
                        || strcasecmp("POINT", buffer) == 0 ) {
                        lgt->type = LIGHT_POINT;
                    } else if( strcasecmp("LIGHT_DIRECTIONAL", buffer) == 0
                        || strcasecmp("DIRECTIONAL", buffer) == 0 ) {
                        lgt->type = LIGHT_DIRECTIONAL;
                    } else if( strcasecmp("LIGHT_SPOT", buffer) == 0
                        || strcasecmp("SPOT", buffer) == 0 ) {
                        lgt->type = LIGHT_SPOT;
                    } else if( strcasecmp("LIGHT_DISK", buffer) == 0
                        || strcasecmp("DISK", buffer) == 0 ) {
                        lgt->type = LIGHT_DISK;
                    } else if( strcasecmp("LIGHT_RECT", buffer) == 0
                        || strcasecmp("RECT", buffer) == 0 ) {
                        lgt->type = LIGHT_RECT;
                    } else {
                        fprintf(stderr, "%s: Unknown light type '%s'\n", __FUNCTION__, buffer);
                    }
                } else if( strcasecmp("color", value) == 0 ) {
                    double red=0.0, green=0.0, blue=0.0;
                    scene_yaml_parse_color(parser, &red, &green, &blue);
                    lgt->red = red;
                    lgt->green = green;
                    lgt->blue = blue;
                } else if( strcasecmp("pos", value) == 0 ) {
                    scene_yaml_parse_vect(parser, &lgt->pos);
                } else if( strcasecmp("target", value) == 0 ) {
                    scene_yaml_parse_vect(parser, &lgt->target);
                } else if( strcasecmp("dir", value) == 0 ) {
                    scene_yaml_parse_vect(parser, &lgt->dir);
                } else if( strcasecmp("radius", value) == 0 ) {
                    scene_yaml_parse_double(parser, &lgt->radius);
                } else if( strcasecmp("u", value) == 0 ) {
                    scene_yaml_parse_vect(parser, &lgt->u);
                } else if( strcasecmp("v", value) == 0 ) {
                    scene_yaml_parse_vect(parser, &lgt->v);
                } else if( strcasecmp("prepared", value) == 0 ) {
                    int v=0;
                    scene_yaml_parse_int(parser, &v);
                    lgt->prepared = v;
                } else if( strcasecmp("u1", value) == 0 ) {
                    scene_yaml_parse_vect(parser, &lgt->u1);
                } else if( strcasecmp("v1", value) == 0 ) {
                    scene_yaml_parse_vect(parser, &lgt->v1);
                } else {
                    fprintf(stderr, "%s: Unhandled key '%s'.\n", __FUNCTION__, value);
                }
            }
        }

        /* cleanup event */
        yaml_event_delete(&event);
    }

    return 0;
}

static int scene_yaml_parse_lights(yaml_parser_t *parser, scene *scn) {

    yaml_event_t event;

    /* Get the first event. */
    if (!yaml_parser_parse(parser, &event)) {
        fprintf(stderr, "Parse error, %i\n", parser->error);
        exit(1);
    }
    yaml_event_type_t type = event.type;
    yaml_event_delete(&event);

    if( type != YAML_SEQUENCE_START_EVENT ) {
        fprintf(stderr, "%s: Expected start of list.\n", __FUNCTION__);
        return -1;
    }

    /* process list */
    int done = 0;
    while( !done ) {
        /* Get the next event. */
        if (!yaml_parser_parse(parser, &event)) {
            fprintf(stderr, "Parse error, %i\n", parser->error);
            exit(1);
        }
        yaml_event_type_t type = event.type;

        if( type == YAML_SEQUENCE_END_EVENT ) {
            done = 1;
            continue;
        }
        
        /* process event */
        if (type == YAML_MAPPING_START_EVENT) {
            light *lgt = NULL;
            scene_alloc_light(scn, &lgt);
            scene_yaml_parse_light(parser, lgt);
        }

        /* cleanup event */
        yaml_event_delete(&event);
    }

    return 0;
}

static int scene_yaml_parse_camera(yaml_parser_t *parser, camera *cam) {

    int done = 0;
    yaml_event_t event;
    while( !done ) {
        /* Get the next event. */
        if (!yaml_parser_parse(parser, &event)) {
            fprintf(stderr, "Parse error, %i\n", parser->error);
            exit(1);
        }
        yaml_event_type_t type = event.type;

        if( type == YAML_MAPPING_END_EVENT ) {
            done = 1;
            continue;
        }
        
        /* process event */
        if (type == YAML_SCALAR_EVENT) {
            char *value = (char*)event.data.scalar.value;
            if( value ) {
                if( strcasecmp("viewPoint", value) == 0 ) {
                    scene_yaml_parse_vect(parser, &cam->viewPoint);
                } else if( strcasecmp("viewTarget", value) == 0 ) {
                    scene_yaml_parse_vect(parser, &cam->viewTarget);
                } else if( strcasecmp("up", value) == 0 ) {
                    scene_yaml_parse_vect(parser, &cam->up);
                } else if( strcasecmp("rotation", value) == 0 ) {
                    scene_yaml_parse_double(parser, &cam->rotation);
                } else if( strcasecmp("eye_offset", value) == 0 ) {
                    scene_yaml_parse_double(parser, &cam->eye_offset);
                } else if( strcasecmp("aperture_radius", value) == 0 ) {
                    scene_yaml_parse_double(parser, &cam->aperture_radius);
                } else if( strcasecmp("focal_distance", value) == 0 ) {
                    scene_yaml_parse_double(parser, &cam->focal_distance);
                } else if( strcasecmp("zoom", value) == 0 ) {
                    scene_yaml_parse_double(parser, &cam->zoom);
                } else if( strcasecmp("hFov", value) == 0 ) {
                    scene_yaml_parse_double(parser, &cam->hFov);
                } else if( strcasecmp("vFov", value) == 0 ) {
                    scene_yaml_parse_double(parser, &cam->vFov);
                } else if( strcasecmp("flip_x", value) == 0 ) {
                    int v = 0;
                    scene_yaml_parse_int(parser, &v);
                    cam->flip_x = v;
                } else if( strcasecmp("flip_y", value) == 0 ) {
                    int v = 0;
                    scene_yaml_parse_int(parser, &v);
                    cam->flip_y = v;
                } else if( strcasecmp("flatten", value) == 0 ) {
                    int v = 0;
                    scene_yaml_parse_int(parser, &v);
                    cam->flatten = v;
                } else if( strcasecmp("prepared", value) == 0 ) {
                    int v = 0;
                    scene_yaml_parse_int(parser, &v);
                    cam->prepared = v;
                } else if( strcasecmp("leveling", value) == 0 ) {
                    scene_yaml_parse_double(parser, &cam->leveling);
                } else if( strcasecmp("pos", value) == 0 ) {
                    scene_yaml_parse_vect(parser, &cam->pos);
                } else if( strcasecmp("leftEye", value) == 0 ) {
                    scene_yaml_parse_vect(parser, &cam->leftEye);
                } else if( strcasecmp("rightEye", value) == 0 ) {
                    scene_yaml_parse_vect(parser, &cam->rightEye);
                } else if( strcasecmp("dirX", value) == 0 ) {
                    scene_yaml_parse_vect(parser, &cam->dirX);
                } else if( strcasecmp("dirY", value) == 0 ) {
                    scene_yaml_parse_vect(parser, &cam->dirY);
                } else if( strcasecmp("imgOrig", value) == 0 ) {
                    scene_yaml_parse_vect(parser, &cam->imgOrig);
                } else if( strcasecmp("localX", value) == 0 ) {
                    scene_yaml_parse_vect(parser, &cam->localX);
                } else if( strcasecmp("localY", value) == 0 ) {
                    scene_yaml_parse_vect(parser, &cam->localY);
                } else if( strcasecmp("localZ", value) == 0 ) {
                    scene_yaml_parse_vect(parser, &cam->localZ);
                } else {
                    fprintf(stderr,"%s: Unhandled value, '%s'.\n", __FUNCTION__, value);
                }
            }
        }

        /* cleanup event */
        yaml_event_delete(&event);
    }

    return 0;
}

static int scene_yaml_parse_scene(yaml_parser_t *parser, scene *scn) {
    int done = 0;
    yaml_event_t event;

    while( !done ) {
        /* Get the next event. */
        if (!yaml_parser_parse(parser, &event)) {
            fprintf(stderr, "Parse error, %i\n", parser->error);
            exit(1);
        }

        /* Check if this is the stream end. */
        if (event.type == YAML_DOCUMENT_END_EVENT) {
            done = 1;
            continue;
        }
    
        /* process event */
        yaml_event_type_t type = event.type;
        if (type == YAML_SCALAR_EVENT) {
            char *value = (char*)event.data.scalar.value;
            if( value ) {
                if( strcasecmp("lights", value) == 0 ) {
                    scene_yaml_parse_lights(parser, scn);
                } else if( strcasecmp("objects", value) == 0 ) {
                    scene_yaml_parse_objects(parser, scn);
                } else if( strcasecmp("camera", value) == 0 ) {
                    scene_yaml_parse_camera(parser,&scn->cam);
                } else if( strcasecmp("dimensions", value) == 0 ) {
                    scene_yaml_parse_int(parser, &scn->dimensions);
                } else if( strcasecmp("scene", value) == 0 ) {
                    scene_yaml_parse_string(parser, scn->name, sizeof(scn->name));
                } else if( strcasecmp("background", value) == 0 ) {
                    scene_yaml_parse_color(parser, &scn->bg_red, &scn->bg_green, &scn->bg_blue);
                } else {
                    fprintf(stderr, "%s: Unhandled key '%s'.\n", __FUNCTION__, value);
                }
            }
        }

        /* cleanup event */
        yaml_event_delete(&event);
    }

    return 0;
}

static int scene_yaml_skip_to_frame(yaml_parser_t *parser, int frame) {
    yaml_event_t event;
    int done = 0;
    while( !done && frame > 0 ) {
        if (!yaml_parser_parse(parser, &event)) {
            fprintf(stderr, "Parse error, %i\n", parser->error);
            exit(1);
        }

        /* Check if this is the stream end. */
        if (event.type == YAML_STREAM_END_EVENT) {
            done = 1;
            continue;
        }
    
        if( event.type == YAML_DOCUMENT_END_EVENT ) {
            frame -= 1;
        }

        /* cleanup event */
        yaml_event_delete(&event);
    }

    return 0;
}

int scene_read_yaml(scene *scn, char *fname, int frame) {
    printf("%s reading from '%s'.\n", __FUNCTION__, fname);

    /* create output file */
    FILE *fp = fopen(fname, "rb");
    if( fp == NULL ) {
        perror("fopen");
        exit(1);
    }
    
    /* initialize YAML emitter */
    yaml_parser_t parser;
    yaml_parser_initialize(&parser);
    yaml_parser_set_input_file(&parser, fp);

    /* skip to requested frame */
    scene_yaml_skip_to_frame(&parser, frame);

    int ret = scene_yaml_parse_scene(&parser, scn);

    yaml_parser_delete(&parser);
    fclose(fp); fp=NULL;

    return ret;
}

int scene_yaml_count_frames(char *fname) {
    int frames = 0;

    /* create output file */
    FILE *fp = fopen(fname, "rb");
    if( fp == NULL ) {
        perror("fopen");
        exit(1);
    }
    
    /* initialize YAML emitter */
    yaml_parser_t parser;
    yaml_parser_initialize(&parser);
    yaml_parser_set_input_file(&parser, fp);

    yaml_event_t event;
    int done = 0;
    while( !done ) {
        if (!yaml_parser_parse(&parser, &event)) {
            fprintf(stderr, "Parse error, %i\n", parser.error);
            exit(1);
        }

        /* Check if this is the stream end. */
        if (event.type == YAML_STREAM_END_EVENT) {
            done = 1;
            continue;
        }
    
        if( event.type == YAML_DOCUMENT_START_EVENT ) {
            frames += 1;
        }

        /* cleanup event */
        yaml_event_delete(&event);
    }

    yaml_parser_delete(&parser);
    fclose(fp); fp=NULL;

    return frames;
}

#endif /* WITH_YAML */
