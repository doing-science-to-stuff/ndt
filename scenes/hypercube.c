/*
 * hypercube.c
 * ndt: n-dimensional tracer
 *
 * Copyright (c) 2019-2021 Bryan Franklin. All rights reserved.
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

    int *dirs_count = NULL, *pos_count = NULL;
    if( m > 0 ) {
        dirs_count = calloc(m, sizeof(int));
        for(int i=0; i<m; ++i)
            dirs_count[i] = m-i-1;
    }
    pos_count = calloc(n-m, sizeof(int));
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
        if( m > 1 && m != n-2 ) {
            /* add a orthotope for face */
            obj = object_alloc(cube->dimensions, "orthotope", "");
            snprintf(obj->name, sizeof(obj->name), "face %i", f);
            object_add_flag(obj, m);

            for(int i=0; i<m; ++i) {
                vectNd_set(&pos, dirs_count[i], -CUBE_SIZE/2.0);
                vectNd_reset(&dir);
                vectNd_set(&dir, dirs_count[i], CUBE_SIZE);
                object_add_dir(obj, &dir);
            }
            object_add_pos(obj, &pos);
        } else if( m == n-2 ) {
            /* add a hcylinder for face */
            obj = object_alloc(cube->dimensions, "hcylinder", "");
            snprintf(obj->name, sizeof(obj->name), "'edge' %i", f);
            object_add_size(obj, EDGE_SIZE + (n-m) * (EDGE_SIZE*0.05 + EPSILON) );
            object_add_flag(obj, m);

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
            snprintf(obj->name, sizeof(obj->name), "edge %i", f);
            object_add_size(obj, EDGE_SIZE + (n-m) * (EDGE_SIZE*0.05 + EPSILON) );
            object_add_flag(obj, 1);
            object_add_pos(obj, &pos);
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
            snprintf(obj->name, sizeof(obj->name), "corner %i", f);
            object_add_size(obj, EDGE_SIZE + (n-m) * (EDGE_SIZE*0.05 + EPSILON) );
            object_add_pos(obj, &pos);
        } else {
            fprintf(stderr, "%i-dimensional face shouldn't be requested.\n", m);
            exit(1);
        }

        /* set color based on dimensions */
        if( m == n ) {
            /* entire object, which doesn't work */
            obj->red = 0.8;
            obj->green = 0.0;
            obj->blue = 0.8;
        } else if( m == n-1 ) {
            /* main faces */
            obj->red = 0.0;
            obj->green = 0.0;
            obj->blue = 0.8;
        } else if( m == n-2 ) {
            /* main edges */
            obj->red = 0.8;
            obj->green = 0.8;
            obj->blue = 0.0;
        } else if( m == n-2 ) {
            /* secondary edges, possibly points */
            obj->red = 0.8;
            obj->green = 0.0;
            obj->blue = 0.0;
        } else if( m == n-3 ) {
            /* tertiary edges, probably points */
            obj->red = 0.0;
            obj->green = 0.8;
            obj->blue = 0.0;
        } else {
            obj->red = obj->green = obj->blue = 0.8;
        }

        snprintf(obj->name, sizeof(obj->name), "%id face %i", m, f);
        object_add_obj(cube, obj);

        /* update counters */
        ++real_offset_id;
        int i=0;
        while(i < (n-m) && pos_count[i] == 1) {
            pos_count[i] = 0;
            ++i;
        }
        if( i < n-m ) {
            pos_count[i] += 1;
        } else if( m > 0 ) {
            /* pos counter reached end, so update dirs_counter */
            int j=0;
            while(j < m && dirs_count[j] == n-j-1 ) {
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
static int frames_per_rotation = 300;
int scene_frames(int dimensions, char *config) {
    if( dimensions < 3 )
        return 0;
    if( config==NULL )
        printf("config string omitted.\n");
    return 8 * frames_per_rotation;
}

int scene_setup(scene *scn, int dimensions, int frame, int frames, char *config)
{
    double t = frame/(double)frames;
    int use_hcube = 0;
    int with_walls = 0;

    /* process options */
    if( config && strstr("hcube", config) )
        use_hcube = 1;
    if( config && strstr("walls", config) )
        with_walls = 1;

    printf("config: %s; use_hcube: %i, with_walls: %i\n",
        config?config:"null", use_hcube, with_walls);

    /* determine name of scene */
    char *prefix = "hypercube";
    char *suffix = "";
    char scene_name[64];
    if( use_hcube )
        prefix = "hcube";
    if( with_walls )
        suffix = "-reflect";
    snprintf(scene_name, sizeof(scene_name), "%s%s", prefix, suffix);

    /* create scene */
    scene_init(scn, scene_name, dimensions);

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

    if( with_walls ) {
        vectNd_setStr(&viewPoint,"65.7,22.25,55,0");
        vectNd_setStr(&viewTarget,"3,-2.5,0,0");
    } else {
        vectNd_setStr(&viewPoint,"60,10,50,0");
        vectNd_setStr(&viewTarget,"0,-1.5,0,0");
    }

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
    #if 1
    scene_alloc_light(scn,&lgt);
    lgt->type = LIGHT_AMBIENT;
    lgt->red = 0.25;
    lgt->green = 0.25;
    lgt->blue = 0.25;
    #endif /* 0 */

    #if 1
    scene_alloc_light(scn,&lgt);
    lgt->type = LIGHT_DIRECTIONAL;
    vectNd_calloc(&lgt->dir,dimensions);
    if( with_walls )
        vectNd_setStr(&lgt->dir,"0,-1,0,0");
    else
        vectNd_setStr(&lgt->dir,"-1,-1,-1,0");
    lgt->red = 0.75;
    lgt->green = 0.75;
    lgt->blue = 0.75;
    #endif /* 0 */

    #if 0
    if( !with_walls ) {
        scene_alloc_light(scn,&lgt);
        lgt->type = LIGHT_POINT;
        vectNd_calloc(&lgt->pos,dimensions);
        vectNd_setStr(&lgt->pos,"60,10,30,0");
        lgt->red = 600;
        lgt->green = 600;
        lgt->blue = 600;
    }
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

    if( with_walls ) {
        /* add some walls for shadows be be on */
        double wall_dist = CUBE_SIZE*1.5;
        scene_alloc_object(scn,dimensions,&obj,"hplane");
        snprintf(obj->name, sizeof(obj->name), "wall 1");
        obj->red = obj->green = obj->blue = 0.0;
        obj->red_r = obj->green_r = obj->blue_r = 0.95;
        vectNd_reset(&pos);
        vectNd_reset(&normal);
        vectNd_set(&pos, 0, -wall_dist);
        vectNd_set(&normal, 0, 1.0);
        object_add_pos(obj, &pos);
        object_add_dir(obj, &normal);

        scene_alloc_object(scn,dimensions,&obj,"hplane");
        snprintf(obj->name, sizeof(obj->name), "wall 2");
        obj->red = obj->green = obj->blue = 0.0;
        obj->red_r = obj->green_r = obj->blue_r = 0.95;
        vectNd_reset(&pos);
        vectNd_reset(&normal);
        vectNd_set(&pos, 2, -wall_dist);
        vectNd_set(&normal, 2, 1.0);
        object_add_pos(obj, &pos);
        object_add_dir(obj, &normal);
    }

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
        vectNd temp;
        vectNd_calloc(&temp,dimensions);
        for(int i=0; i<dimensions; ++i) {
            vectNd_reset(&temp);
            vectNd_set(&temp, i, 1.0);
            object_add_dir(obj, &temp);
        }
        vectNd_free(&temp);
        obj->red = 0.0;
        obj->green = 0.0;
        obj->blue = 0.8;
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

    /* alter the plane of rotation based on phase of the animation */
    int which_rotation = frame / frames_per_rotation;
    vectNd_rotate(&dir2, NULL, 0, 2, which_rotation * (M_PI / 4.0), &dir2);

    /* rotate (hyper)cube */
    double angle = ((2 * M_PI) * (frame % frames_per_rotation)) / (frames_per_rotation-1);
    object_rotate2(obj, &origin, &dir1, &dir2, angle);

    printf("Rotating %6.2f degrees through plane %i.\n", angle * 180.0 / M_PI, which_rotation);

    return 1;
}

int scene_cleanup() {
    /* If any persistent resources were allocated,
     * they should be freed here. */
    return 0;
}
