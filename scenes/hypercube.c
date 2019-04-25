/*
 * hypercube.c
 * ndt: n-dimensional tracer
 *
 * Copyright (c) 2019 Bryan Franklin. All rights reserved.
 */
#include <stdio.h>
#include "../scene.h"

#define CUBE_SIZE 15
#define EDGE_SIZE (0.0075 * (CUBE_SIZE))

static int factorial(int n) {
    int ret = 1;
    for(int i=1; i<=n; ++i) {
        ret *= i;
    }
    return ret;
}

static int choose(int n, int m) {
    return factorial(n) / (factorial(m) * factorial(n-m));
}

/* see: https://en.wikipedia.org/wiki/Hypercube#Elements */
static int num_n_faces(int n, int m) {
    /* for an n-cube, how many m-dimensional components does it contain */
    return (1<<(n-m)) * choose(n, m);
}

static int add_faces(object *cube, int m) {
    int n = cube->dimensions;
    int num_faces = num_n_faces(n, m);
    vectNd dir;
    vectNd_calloc(&dir, n);
    vectNd pos;
    vectNd_calloc(&pos, n);

    if( m > 0 )
        add_faces(cube, m-1);

    printf("Adding %i-dimensional faces.\n", m);

    int *dirs_count, *pos_count;
    dirs_count = calloc(m, sizeof(int));
    pos_count = calloc(n-m, sizeof(int));
    for(int i=0; i<n; ++i)
        dirs_count[i] = m-i-1;
    int offset_id = 0;
    int real_offset_id = 0;
    for(int f=0; f<num_faces; ++f) {

        vectNd_reset(&pos);
        vectNd_reset(&dir);
        offset_id = real_offset_id;
        for(int i=0; i<n; ++i) {
            /* determine which dimensions 'face' exists in */
            int is_dir = 0;
            for(int j=0; is_dir==0 && j<m; ++j) {
                if( i == dirs_count[j] )
                    is_dir = 1;
            }
            if( is_dir ) {
                vectNd_set(&pos, i, -0.5 * CUBE_SIZE);
                continue;
            }

            int value = offset_id % 2;
            offset_id >>= 1;

            /* determine location of 'face' */
            vectNd_set(&pos, i, CUBE_SIZE * (value-0.5));
        }

        object *obj = NULL;
        if( m > 2 || (n == 3 && m == 2) ) {
            /* add a orthotope for face */
            obj = object_alloc(cube->dimensions, "orthotope", "");
            object_add_flag(obj, m);
            if( m == 3 || n == 3 ) {
                obj->red = 0.0;
                obj->green = 0.0;
                obj->blue = 0.8;
            } else {
                obj->red = 0.8;
                obj->green = 0.8;
                obj->blue = 0.8;
            }

            for(int i=0; i<m; ++i) {
                vectNd_set(&pos, dirs_count[i], -CUBE_SIZE/2.0);
                vectNd_reset(&dir);
                vectNd_set(&dir, dirs_count[i], CUBE_SIZE);
                object_add_dir(obj, &dir);
            }
            object_add_pos(obj, &pos);
        } else if( m == 2 ) {
            /* add a orthotope for face */
            obj = object_alloc(cube->dimensions, "hcylinder", "");
            object_add_size(obj, EDGE_SIZE);
            object_add_flag(obj, m);
            obj->red = 0.8;
            obj->green = 0.8;
            obj->blue = 0.0;

            for(int i=0; i<m; ++i) {
                vectNd_set(&pos, dirs_count[i], -CUBE_SIZE/2.0);
            }
            object_add_pos(obj, &pos);
            for(int i=0; i<m; ++i) {
                vectNd_reset(&dir);
                vectNd_copy(&dir, &pos);
                vectNd_set(&dir, dirs_count[i], CUBE_SIZE/2.0);
                object_add_pos(obj, &dir);
            }
        } else if( m == 1 ) {
            obj = object_alloc(cube->dimensions, "cylinder", "");
            object_add_size(obj, EDGE_SIZE + EPSILON);
            object_add_flag(obj, 1);
            object_add_pos(obj, &pos);
            obj->red = 0.0;
            obj->green = 0.8;
            obj->blue = 0.0;
            vectNd pos2;
            vectNd_calloc(&pos2,n);
            vectNd_copy(&pos2, &pos);
            for(int i=0; i<m; ++i) {
                int dir = dirs_count[i];
                vectNd_set(&pos2, dir, pos2.v[dir] + CUBE_SIZE);
            }
            object_add_pos(obj, &pos2);
            vectNd_free(&pos2);
        } else if( m == 0 ) {
            obj = object_alloc(cube->dimensions, "sphere", "");
            object_add_size(obj, EDGE_SIZE + 2.0 * EPSILON);
            object_add_pos(obj, &pos);
            obj->red = 0.8;
            obj->green = 0.0;
            obj->blue = 0.0;
        } else {
            fprintf(stderr, "%i-dimensional face shouldn't be requested.\n", m);
            exit(1);
        }
        snprintf(obj->name, sizeof(obj->name), "%id face %i", m, f);
        object_add_obj(cube, obj);

        /* update counters */
        ++real_offset_id;
        int i=0;
        while(pos_count[i] == 1) {
            pos_count[i] = 0;
            ++i;
        }
        if( i < n-m ) {
            pos_count[i] += 1;
        } else {
            /* pos counter reached end, so update dirs_counter */
            int j=0;
            while(dirs_count[j] == n-j-1 ) {
                if( j < m-1 ) {
                    dirs_count[j] = dirs_count[j+1]+1;
                } else {
                    dirs_count[j] = 0;
                }
                ++j;
            }
            if( j < m ) {
                dirs_count[j] += 1;
                --j;
                while( j >= 0 ) {
                    dirs_count[j] = dirs_count[j+1]+1;
                    --j;
                }
            }
        }
    }
    free(dirs_count); dirs_count=NULL;
    free(pos_count); pos_count=NULL;

    return 0;
}

/* scene_frames is optional, but gives the total number of frames to render
 * for an animated scene. */
int scene_frames(int dimensions, char *config) {
    return 300;
}

int scene_setup(scene *scn, int dimensions, int frame, int frames, char *config)
{
    double t = frame/(double)frames;
    int use_hcube = 0;

    if( config && !strcmp("hcube", config) )
        use_hcube = 1;

    if( use_hcube )
        scene_init(scn, "hcube", dimensions);
    else
        scene_init(scn, "hypercube", dimensions);

    printf("Generating frame %i of %i scene '%s' (%.2f%% through animation).\n",
            frame, frames, scn->name, 100.0*t);

    /* zero out camera */
    camera_reset(&scn->cam);

    /* move camera into position */
    vectNd viewPoint;
    vectNd viewTarget;
    vectNd up_vect;
    vectNd_calloc(&viewPoint,dimensions);
    vectNd_calloc(&viewTarget,dimensions);
    vectNd_calloc(&up_vect,dimensions);

    vectNd_setStr(&viewPoint,"60,10,30,20");
    vectNd_setStr(&viewTarget,"0,0,0,0");
    vectNd_set(&up_vect,1,10);  /* 0,10,0,0... */
    #if 0
    if( dimensions > 3 ) {
        vectNd_set(&viewPoint,3,CUBE_SIZE/2.0);
        vectNd_set(&viewTarget,3,CUBE_SIZE/2.0);
    }
    #endif /* 0 */
    camera_set_aim(&scn->cam, &viewPoint, &viewTarget, &up_vect, 0);
    vectNd_free(&up_vect);
    vectNd_free(&viewPoint);
    vectNd_free(&viewTarget);

    /* setup lighting */
    light *lgt=NULL;
    scene_alloc_light(scn,&lgt);
    lgt->type = LIGHT_AMBIENT;
    lgt->red = 0.25;
    lgt->green = 0.25;
    lgt->blue = 0.25;

    scene_alloc_light(scn,&lgt);
    lgt->type = LIGHT_DIRECTIONAL;
    vectNd_calloc(&lgt->dir,dimensions);
    vectNd_setStr(&lgt->dir,"-1,-1,-1,0");
    lgt->red = 0.75;
    lgt->green = 0.75;
    lgt->blue = 0.75;

    #if 0
    scene_alloc_light(scn,&lgt);
    lgt->type = LIGHT_POINT;
    vectNd_calloc(&lgt->pos,dimensions);
    vectNd_setStr(&lgt->pos,"60,10,30,20");
    lgt->red = 600;
    lgt->green = 600;
    lgt->blue = 600;
    #endif /* 1 */

    /* create objects array */
    object *obj = NULL;

    /* add reflective floor */
    vectNd pos;
    vectNd normal;
    vectNd_calloc(&pos,dimensions);
    vectNd_calloc(&normal,dimensions);
    scene_alloc_object(scn,dimensions,&obj,"hplane");
    snprintf(obj->name, sizeof(obj->name), "floor");
    obj->red = 0.8;
    obj->green = 0.8;
    obj->blue = 0.8;
    obj->red_r = 0.5;
    obj->green_r = 0.5;
    obj->blue_r = 0.5;
    vectNd_set(&pos,1,-CUBE_SIZE*1.5);
    object_add_pos(obj, &pos);
    vectNd_set(&normal,1,1);
    object_add_dir(obj, &normal);

    #if 0
    /* add sphere where cube should be */
    scene_alloc_object(scn,dimensions,&obj,"sphere");
    obj->red = 0.8;
    vectNd_reset(&pos);
    object_add_pos(obj, &pos);
    object_add_size(obj, CUBE_SIZE/2.0);
    #endif /* 0 */

    if( use_hcube ) {
        /* add a single hcube object */
        scene_alloc_object(scn,dimensions,&obj,"hcube");
        snprintf(obj->name, sizeof(obj->name), "the hypercube");
        for(int i=0; i<dimensions; ++i)
            object_add_size(obj, CUBE_SIZE);
        vectNd origin;
        vectNd_calloc(&origin, dimensions);
        object_add_pos(obj, &origin);
        vectNd_free(&origin);
        obj->red = 0.0;
        obj->green = 0.0;
        obj->blue = 0.8;
        obj->get_bounds(obj);   /* this needs to be done before any rotations can happen */
    } else {
        /* add all of the faces for a (hyper)cube */
        scene_alloc_object(scn,dimensions,&obj,"cluster");
        object_add_flag(obj, 2*dimensions);
        add_faces(obj, dimensions-1);
    }

    /* rotate (hyper)cube */
    vectNd dir1, dir2, origin;
    vectNd_calloc(&dir1, dimensions);
    vectNd_calloc(&dir2, dimensions);
    vectNd_calloc(&origin, dimensions);
    vectNd_set(&dir1, 1, 1.0);
    for(int i=0; i<dimensions; ++i) {
        vectNd_set(&dir2, i, 1);
    }
    double angle = frame * (2 * M_PI) / (double)frames;
    object_rotate2(obj, &origin, &dir1, &dir2, angle);

    return 1;
}

int scene_cleanup() {
    /* If any persistent resources were allocated,
     * they should be freed here. */
    return 0;
}
