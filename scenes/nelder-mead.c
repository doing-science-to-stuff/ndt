/*
 * nelder-mead.c
 * ndt: n-dimensional tracer
 *
 * Copyright (c) 2019 Bryan Franklin. All rights reserved.
 */
#include <stdio.h>
#include "../scene.h"
#include "../nelder-mead.h"

/* This scene demonstrates how Nelder-Mead can be used to find the center of a
 * sphere that circumscribes a set of points with minimum radius. */

#define BOUNDING_RADIUS 0.25
#define SIMPLEX_RADIUS 0.1
#define CURR_RADIUS 0.125
#define HISTORY_RADIUS 0.0625
#define FINAL_RADIUS 0.125

static bounds_list bounding_set;
static int initialized = 0;
static int total_iterations = 0;
static vectNd final_point;

/* scene_frames is optional, but gives the total number of frames to render
 * for an animated scene. */
int scene_frames(int dimensions, char *config) {
    /* create bounding points */
    vectNd point;
    vectNd_calloc(&point, dimensions);

    int num_points = 20;
    if( config ) {
        num_points = atoi(config);
    }

    if( !initialized ) {
        bounds_list_init(&bounding_set);
        #if 1
        for(int i=0; i<num_points; ++i) {
            for(int j=0; j<dimensions; ++j) {
                vectNd_set(&point, j, (drand48()-0.5) * 20);
            }
            bounds_list_add(&bounding_set, &point, 0.0);
        }
        #else
        char *point_str[] = {
            "0.0, 0.0, 10.0, 1.0",
            "0.0, 10.0, -5.0, 2.0",
            "10.0, 0.0, -5.0, 3.0",
            "-10.0, 0.0, -5.0, 4.0",
            };
        for(int i=0; i<sizeof(point_str)/sizeof(*point_str); ++i) {
            vectNd_setStr(&point, point_str[i]);
            bounds_list_add(&bounding_set, &point, 0.0);
        }
        #endif /* 0 */

        initialized = 1;
    }

    /* do a full run to get number of iterations and final point */
    void *nm = NULL;
    nm_init(&nm, dimensions);

    /* get initial guess for center */
    double curr_radius = -1.0;
    vectNd curr_centroid;
    vectNd_calloc(&curr_centroid, dimensions);
    bounds_list_centroid(&bounding_set, &curr_centroid);
    bounds_list_radius(&bounding_set, &curr_centroid, &curr_radius);
    nm_set_seed(nm, &curr_centroid);

    int max_iterations = 1000;
    total_iterations = 0;
    while( !nm_done(nm, EPSILON, max_iterations) ) {
        nm_add_result(nm, &curr_centroid, curr_radius);
        nm_next_point(nm, &curr_centroid);
        bounds_list_radius(&bounding_set, &curr_centroid, &curr_radius);
        ++total_iterations;
    }

    /* get final result */
    vectNd_calloc(&final_point, dimensions);
    nm_best_point(nm, &final_point);
    vectNd_free(&curr_centroid);

    return 2*total_iterations;
}

int scene_setup(scene *scn, int dimensions, int frame, int frames, char *config)
{
    double t = frame/(double)frames;
    scene_init(scn, "nelder-mead", dimensions);

    printf("Generating frame %i of %i scene '%s' (%.2f%% through animation).\n",
            frame, frames, scn->name, 100.0*t);
    if( config==NULL )
        printf("config string omitted.\n");

    /* zero out camera */
    camera_reset(&scn->cam);

    /* move camera into position */
    vectNd viewPoint;
    vectNd viewTarget;
    vectNd up_vect;
    vectNd_calloc(&viewPoint,dimensions);
    vectNd_calloc(&viewTarget,dimensions);
    vectNd_calloc(&up_vect,dimensions);

    vectNd_setStr(&viewPoint,"60,8,0,10");
    vectNd_setStr(&viewTarget,"0,0,0,0");
    vectNd_set(&up_vect,1,10);  /* 0,10,0,0... */

    /* spin around the center */
    double angle = (2 * M_PI) * (frame / (double)total_iterations) + 1.0;
    double camRadius = 60.0;
    vectNd_set(&viewPoint, 0, camRadius * cos(angle));
    vectNd_set(&viewPoint, 2, camRadius * sin(angle));

    if( frame < total_iterations ) {
        /* move from origin toward final point */
        vectNd_scale(&final_point, frame/(double)total_iterations, &viewTarget);

        /* slowly spiral in towards final point */
        vectNd_scale(&viewPoint, pow(0.975, frame), &viewPoint);
        vectNd_add(&viewPoint, &viewTarget, &viewPoint);
    } else {
        /* stay aimed at final point */
        vectNd_copy(&viewTarget, &final_point);

        /* spiral back out */
        vectNd_scale(&viewPoint, pow(0.975, 2*total_iterations - frame), &viewPoint);
        vectNd_add(&viewPoint, &viewTarget, &viewPoint);
    }

    camera_set_aim(&scn->cam, &viewPoint, &viewTarget, &up_vect, 0);
    vectNd_free(&up_vect);
    vectNd_free(&viewPoint);
    vectNd_free(&viewTarget);

    /* setup lighting */
    light *lgt=NULL;
    scene_alloc_light(scn,&lgt);
    lgt->type = LIGHT_AMBIENT;
    lgt->red = 0.5;
    lgt->green = 0.5;
    lgt->blue = 0.5;

    scene_alloc_light(scn,&lgt);
    lgt->type = LIGHT_DIRECTIONAL;
    vectNd_calloc(&lgt->dir,dimensions);
    vectNd_setStr(&lgt->dir,"0,-1,0,0");
    lgt->red = 0.5;
    lgt->green = 0.5;
    lgt->blue = 0.5;

    /* create objects */
    object *obj = NULL;

    #if 1
    /* add floor */
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
    vectNd_set(&pos,1,-11);
    object_add_pos(obj, &pos);
    vectNd_set(&normal,1,1);
    object_add_dir(obj, &normal);
    #endif /* 0 */

    /* add spheres to indicate the bounding points */
    bounds_node *curr = bounding_set.head;
    while( curr ) {
        scene_alloc_object(scn,dimensions,&obj,"sphere");
        obj->red = 0.0;
        obj->green = 0.0;
        obj->blue = 0.8;
        
        object_add_pos(obj, &curr->bounds.center);
        object_add_size(obj, BOUNDING_RADIUS);

        curr = curr->next;
    }

    /* start nelder-mead */
    vectNd center;
    vectNd_calloc(&center, dimensions);
    double radius = -1.0;
    void *nm = NULL;
    nm_init(&nm, dimensions);

    bounds_list_centroid(&bounding_set, &center);
    nm_set_seed(nm, &center);
    bounds_list_radius(&bounding_set, &center, &radius);
    #if 1
    printf("I: %g\t", radius);
    vectNd_print(&center, "at");
    #endif /* 0 */

    for(int i=0; i <= frame && !nm_done(nm, EPSILON, frame); ++i) {
        /* add a sphere at each point */
        nm_add_result(nm, &center, radius);
        nm_next_point(nm, &center);
        bounds_list_radius(&bounding_set, &center, &radius);

        #if 1
        printf("%i: %g\t", i, radius);
        vectNd_print(&center, "at");
        #endif /* 0 */

        scene_alloc_object(scn,dimensions,&obj,"sphere");
        obj->red = 0.0;
        obj->green = 1.0;
        obj->blue = 0.0;
        
        object_add_pos(obj, &center);
        if( nm_done(nm, EPSILON, frames+1) ) {
            object_add_size(obj, FINAL_RADIUS);
            obj->red = 0.8;
            obj->green = 0.0;
            obj->blue = 0.8;
        } else if( i < frame ) {
            object_add_size(obj, HISTORY_RADIUS * pow(0.975,frame-i) );
        } else {
            object_add_size(obj, CURR_RADIUS);
        }
    }

    /* draw simplex */
    vectNd p;
    vectNd p2;
    vectNd_calloc(&p, dimensions);
    vectNd_calloc(&p2, dimensions);
    for(int j=0; j<dimensions+1; ++j) {
        if( nm_simplex_point(nm, j, &p, NULL) == 0 )
            continue;

        scene_alloc_object(scn,dimensions,&obj,"sphere");
        obj->red = 0.8;
        obj->green = 0.0;
        obj->blue = 0.0;
        
        object_add_pos(obj, &p);
        object_add_size(obj, SIMPLEX_RADIUS);

        for(int k=j; k<dimensions+1; ++k) {
            if( nm_simplex_point(nm, k, &p2, NULL) == 0 )
                continue;

            scene_alloc_object(scn,dimensions,&obj,"cylinder");
            obj->red = 0.4;
            obj->green = 0.2;
            obj->blue = 0.2;

            object_add_pos(obj, &p);
            object_add_pos(obj, &p2);
            object_add_flag(obj, 1);
            object_add_size(obj, SIMPLEX_RADIUS/2.0);
        }
    }
    vectNd_free(&p);
    vectNd_free(&p2);

    /* cleanup */
    nm_free(nm);

    return 1;
}

int scene_cleanup() {
    /* If any persistent resources were allocated,
     * they should be freed here. */
    vectNd_free(&final_point);
    bounds_list_free(&bounding_set);
    return 0;
}
