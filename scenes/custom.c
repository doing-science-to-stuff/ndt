/*
 * empty.c
 * ndt: n-dimensional tracer
 *
 * Copyright (c) 2019 Bryan Franklin. All rights reserved.
 */
#include <stdio.h>
#include "../scene.h"

/* scene_frames is optional, but gives the total number of frames to render
 * for an animated scene. */
int scene_frames(int dimensions, char *config) {
    return 1;
}

int scene_setup(scene *scn, int dimensions, int frame, int frames, char *config)
{
    double t = frame/(double)frames;
    scene_init(scn, "custom", dimensions);

    printf("Generating frame %i of %i scene '%s' (%.2f%% through animation).\n",
            frame, frames, scn->name, 100.0*t);

    /* create camera */
    camera_alloc(&scn->cam, dimensions);
    camera_reset(&scn->cam);

    /* move camera into position */
    vectNd viewPoint;
    vectNd viewTarget;
    vectNd up_vect;
    vectNd_calloc(&viewPoint,dimensions);
    vectNd_calloc(&viewTarget,dimensions);
    vectNd_calloc(&up_vect,dimensions);

    vectNd_setStr(&viewPoint,"60,0,0,0");
    vectNd_setStr(&viewTarget,"0,0,0,0");
    vectNd_set(&up_vect,1,10);  /* 0,10,0,0... */
    camera_set_aim(&scn->cam, &viewPoint, &viewTarget, &up_vect, 0);
    vectNd_free(&up_vect);
    vectNd_free(&viewPoint);
    vectNd_free(&viewTarget);

    /* setup lighting */
    vectNd_calloc(&scn->ambient.pos,dimensions);
    scn->ambient.red = 0.5;
    scn->ambient.green = 0.5;
    scn->ambient.blue = 0.5;

    light *lgt=NULL;
    scene_alloc_light(scn,&lgt);
    vectNd_calloc(&lgt->pos,dimensions);
    vectNd_setStr(&lgt->pos,"0,40,0,-40");
    lgt->red = 300;
    lgt->green = 300;
    lgt->blue = 300;

    /* create objects array */
    object *obj = NULL;

    /* add reflective floor */
    vectNd pos;
    vectNd normal;
    vectNd_calloc(&pos,dimensions);
    vectNd_calloc(&normal,dimensions);
    scene_alloc_object(scn,dimensions,&obj,"hplane");
    obj->red = 0.8;
    obj->green = 0.8;
    obj->blue = 0.8;
    obj->red_r = 0.5;
    obj->green_r = 0.5;
    obj->blue_r = 0.5;
    vectNd_set(&pos,1,-20);
    object_add_pos(obj, &pos);
    vectNd_set(&normal,1,1);
    object_add_dir(obj, &normal);

    scene_alloc_object(scn, 4, &obj, "sphere");
    vectNd center;
    vectNd_calloc(&center, 4);
    vectNd_setStr(&center, "0,5,0,0");  /* centered at <1,2,3,4> */
    object_add_pos(obj, &center);
    object_add_size(obj, 5.0);  /* radius of 5 */
    /* set color */
    obj->red = 0.8;
    obj->green = 0.1;
    obj->blue = 0.1;
    /* set reflectivity */
    obj->red_r = 0.2;
    obj->green_r = 0.2;
    obj->blue_r = 0.2;


    return 1;
}
