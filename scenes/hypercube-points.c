/*
 * balls.c
 * ndt: n-dimensional tracer
 *
 * Copyright (c) 2019-2021 Bryan Franklin. All rights reserved.
 */
#include <stdio.h>
#include "../scene.h"

int scene_setup(scene *scn, int dimensions, int frame, int frames, char *config)
{
    double t=frame/(double)frames;
    int i;

    scene_init(scn, "hypercube_points", dimensions);
    if( config==NULL )
        printf("config string omitted.\n");

    /* determine center of hypercube */
    vectNd cube_shift;
    vectNd_calloc(&cube_shift,dimensions);
    for(i=0; i<dimensions; ++i)
        vectNd_set(&cube_shift, i, -10);

    /* zero camera */
    camera_reset(&scn->cam);

    /* move camera into position */
    vectNd viewPoint;
    vectNd viewTarget;
    vectNd_calloc(&viewPoint,dimensions);
    vectNd_calloc(&viewTarget,dimensions);

    double viewDist = 150;
    vectNd_set(&viewPoint,0,viewDist*cos(2*M_PI*t));
    vectNd_set(&viewPoint,1,30);
    vectNd_set(&viewPoint,2,viewDist*sin(2*M_PI*t));
    vectNd_set(&viewPoint,3,-10*cos(2*M_PI*t));
    vectNd_setStr(&viewTarget,"0,0,0,-10");
    vectNd up_vect;
    vectNd_calloc(&up_vect,dimensions);
    vectNd_set(&up_vect,1,10);
    camera_set_aim(&scn->cam, &viewPoint, &viewTarget, &up_vect, 0.0);
    vectNd_free(&up_vect);

    /* setup lighting */
    vectNd_calloc(&scn->ambient.pos,dimensions);
    scn->ambient.red = .5;
    scn->ambient.green = .5;
    scn->ambient.blue = .5;

    light *lgt=NULL;
    scene_alloc_light(scn,&lgt);
    vectNd_calloc(&lgt->pos,dimensions);
    vectNd_setStr(&lgt->pos,"0,40,0,-40");
    lgt->red = 300;
    lgt->green = 300;
    lgt->blue = 300;

    scene_alloc_light(scn,&lgt);
    vectNd_calloc(&lgt->pos,dimensions);
    vectNd_setStr(&lgt->pos,"-40,40,0,40");
    lgt->red = 300;
    lgt->green = 300;
    lgt->blue = 300;

    scene_alloc_light(scn,&lgt);
    vectNd_calloc(&lgt->pos,dimensions);
    vectNd_setStr(&lgt->pos,"40,40,0,-40");
    lgt->red = 300;
    lgt->green = 300;
    lgt->blue = 300;

    scene_alloc_light(scn,&lgt);
    vectNd_calloc(&lgt->pos,dimensions);
    vectNd_setStr(&lgt->pos,"0,40,-40,40");
    lgt->red = 300;
    lgt->green = 300;
    lgt->blue = 300;

    scene_alloc_light(scn,&lgt);
    vectNd_calloc(&lgt->pos,dimensions);
    vectNd_setStr(&lgt->pos,"0,40,40,40");
    lgt->red = 300;
    lgt->green = 300;
    lgt->blue = 300;

    /* create objects */
    vectNd temp;
    vectNd_calloc(&temp,dimensions);
    int num_spheres = pow(2,dimensions);

    i = 0;

    #if 1
    /* add reflective floor */
    object *obj = NULL;
    scene_alloc_object(scn,dimensions,&obj,"hplane");
    obj->red = 0.8;
    obj->green = 0.8;
    obj->blue = 0.8;
    obj->red_r = 0.5;
    obj->green_r = 0.5;
    obj->blue_r = 0.5;
    vectNd_reset(&temp);
    vectNd_set(&temp,1,-20);
    object_add_pos(obj,&temp);  /* position */
    vectNd_reset(&temp);
    vectNd_set(&temp,1,1);
    object_add_dir(obj,&temp);  /* normal */
    #else
    scn->bg_alpha = 0.0;
    #endif /* 0 */

    /* create spheres */
    vectNd center;
    vectNd_calloc(&center,dimensions);
    vectNd_reset(&center);
    for(int i=0; i<num_spheres; ++i) {

        /* add corner sphere */
        object *sph = NULL;
        scene_alloc_object(scn,dimensions,&sph,"sphere");
        sph->red = 0.0;
        sph->green = 0.0;
        sph->blue = 0.9;
        sph->red_r = 0.3;
        sph->green_r = 0.3;
        sph->blue_r = 0.3;
        vectNd_reset(&temp);
        vectNd_scale(&center,20,&temp);
        vectNd_add(&temp,&cube_shift,&temp);
        object_add_pos(sph,&temp);  /* center */
        object_add_size(sph,5.0);   /* radius */

        /* add cylinders */
        for(int k=0; k<center.n; ++k) {
            if( center.v[k] == 1 ) {
                object *cyl = NULL;
                scene_alloc_object(scn,dimensions,&cyl,"cylinder");
                cyl->red = 0.9;
                cyl->green = 0.1;
                cyl->blue = 0.1;
                cyl->red_r = 0.3;
                cyl->green_r = 0.3;
                cyl->blue_r = 0.3;
                object_add_flag(cyl,1);    /* open */

                object_add_size(cyl, 2.0);  /* radius */
                vectNd other;
                vectNd_calloc(&other,dimensions);
                vectNd_copy(&other,&sph->pos[0]);
                vectNd_set(&other,k,-10);
                object_add_pos(cyl,&other);         /* bottom */
                object_add_pos(cyl,&sph->pos[0]);   /* top */
                vectNd_free(&other);
            }
        }

        /* update center */
        int j=0;
        while( j<dimensions && center.v[j]==1 )
            center.v[j++] = 0;
        center.v[j] = 1;
    }

    return 1;
}
