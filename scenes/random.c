/*
 * random.c
 * ndt: n-dimensional tracer
 *
 * Copyright (c) 2019 Bryan Franklin. All rights reserved.
 */
#include <stdio.h>
#include <stdlib.h>
#include "../scene.h"

#define RAND_COMPONENT() (drand48() * 10 + 2)
#define RAND_SIZE() (drand48() * 3 + 1)

int scene_setup(scene *scn, int dimensions, int frame, int frames, char *config)
{
    object *obj = NULL;

    scene_init(scn, "random", dimensions);

    printf("%i dimensions\n", dimensions);
    int num_objs = 40;
    int num_lights = 5;

    if( config )
        num_objs = atoi(config);

    scn->bg_red = 0.3;
    scn->bg_green = 0.5;
    scn->bg_blue = 0.75;

    char **types = NULL;
    int num_types = 0;
    registered_types(&types,&num_types);

    #if 0
    /* print table of parameters */
    for(int i=0; i<num_types; ++i) {
        int n_pos, n_dir, n_size, n_flag, n_obj;
        scene_alloc_object(scn,dimensions,&obj,types[i]);
        obj->params(obj, &n_pos, &n_dir, &n_size, &n_flag, &n_obj);
        printf("%10s\t% 2i  % 2i  % 2i  % 2i  % 2i\n",
                types[i], n_pos, n_dir, n_size, n_flag, n_obj);
        object_free(obj);
    }
    printf("\n");
    #endif /* 0 */

    /* create scene */
    for(int i=0; i<num_objs; ++i) {
        int type = lrand48() % num_types;
        char *rnd_type = types[type];
        #if 0
        rnd_type = "hfacet";
        #endif /* 0 */
        printf("type: %s (%i)\n", rnd_type, type);
        scene_alloc_object(scn,dimensions,&obj,rnd_type);

        int n_pos, n_dir, n_size, n_flag, n_obj;
        obj->params(obj, &n_pos, &n_dir, &n_size, &n_flag, &n_obj);

        /* skip any object that lacks a position of its own */
        if( n_pos <= 0 ) {
            printf("\tskipping...\n");
            i -= 1;
            scene_remove_object(scn, obj);
            object_free(obj); obj=NULL;
            continue;
        }

        vectNd temp;
        vectNd_calloc(&temp,dimensions);

        /* positions */
        for(int i=0; i<n_pos; ++i) {
            vectNd_reset(&temp);
            for(int j=0; j<dimensions; ++j)
                vectNd_set(&temp, j, RAND_COMPONENT());
            object_add_pos(obj, &temp);
        }

        /* directions */
        for(int i=0; i<n_dir; ++i) {
            vectNd_reset(&temp);
            for(int j=0; j<dimensions; ++j)
                vectNd_set(&temp, j, RAND_COMPONENT());
            vectNd_unitize(&temp);
            object_add_dir(obj, &temp);
        }

        /* sizes */
        for(int i=0; i<n_size; ++i) {
            object_add_size(obj, RAND_SIZE());
        }

        /* flags */
        for(int i=0; i<n_flag; ++i) {
            /* flags are complicated */
            object_add_flag(obj, 1);
        }

        vectNd_free(&temp);

        obj->get_bounds(obj);

        if( obj->bounds.radius < 0 ) {
            printf("removing infinite object...\n");
            i -= 1;
            scene_remove_object(scn, obj);
            object_free(obj); obj=NULL;
            continue;
        }

        obj->red = 0.5*drand48()+0.5;
        obj->green = 0.5*drand48()+0.5;
        obj->blue = 0.5*drand48()+0.5;
        
        obj->red_r = 0.25*drand48();
        obj->green_r = 0.25*drand48();
        obj->blue_r = 0.25*drand48();

        obj->transparent = (drand48()<0.25)?1:0;
        if( obj->transparent )
            obj->refract_index = 1.0 + drand48();
    }

    registered_types_free(types); types=NULL;

    /* create camera */
    camera_init(&scn->cam);

    /* move camera into position */
    vectNd viewPoint;
    vectNd viewTarget;
    vectNd_calloc(&viewPoint,dimensions);
    vectNd_calloc(&viewTarget,dimensions);
    vectNd_setStr(&viewPoint,"30,30,-30,30");
    vectNd_setStr(&viewTarget,"5,5,5,5");
    camera_set_aim(&scn->cam, &viewPoint, &viewTarget, NULL, 0.0);
    vectNd_free(&viewPoint);
    vectNd_free(&viewTarget);

    /* setup lighting */
    light *lgt = NULL;

    scene_alloc_light(scn,&lgt);
    lgt->type = LIGHT_AMBIENT;
    lgt->red = 0.1;
    lgt->green = 0.1;
    lgt->blue = 0.1;

    scene_alloc_light(scn,&lgt);
    vectNd_calloc(&lgt->pos,dimensions);
    vectNd_set(&lgt->pos,0,10);
    vectNd_set(&lgt->pos,1,15);
    vectNd_set(&lgt->pos,2,-15);
    vectNd_set(&lgt->pos,3,10);
    lgt->red = 100;
    lgt->green = 100;
    lgt->blue = 100;

    for(int i=1; i<num_lights; ++i) {
        scene_alloc_light(scn,&lgt);
        vectNd_calloc(&lgt->pos,dimensions);
        vectNd_set(&lgt->pos,0,drand48()*20+15);
        vectNd_set(&lgt->pos,1,drand48()*20+15);
        vectNd_set(&lgt->pos,2,drand48()*20+15);
        vectNd_set(&lgt->pos,3,drand48()*20+15);
        lgt->red = 200;
        lgt->green = 200;
        lgt->blue = 200;
    }

    return 1;
}


