/*
 * balls.c
 * ndt: n-dimensional tracer
 *
 * Copyright (c) 2019 Bryan Franklin. All rights reserved.
 */
#include <stdio.h>
#include <limits.h>
#include "../scene.h"

static double box_size = 10.0;     /* m */
static double max_velocity = 2;  /* m/s */
static double min_radius = 1;
static double max_radius = 2;
static double min_mass = 1;
static double max_mass = 2;
static int num_balls = 100;

typedef struct ball {
    vectNd pos;
    vectNd vel;
    double radius;
    double mass;
    double red, green, blue;
} ball_t;

static ball_t *balls = NULL;

/* frame rate: 30fps */
static double fps = 24;
int scene_frames(int dimensions, char *config) {
    if( dimensions < 3 )
        return 0;
    if( config==NULL )
        printf("config string omitted.\n");
    return 1500;
}

static void print_total_momentum(ball_t *b1, ball_t *b2) {
    vectNd m1, m2;
    vectNd sum;
    double mag;

    vectNd_print(&b1->vel, "\tball 1 velocity");
    vectNd_l2norm(&b1->vel, &mag);
    printf("\tball 1 speed: %g\n", mag);
    printf("\tball 1 mass:  %g\n", b1->mass);
    vectNd_print(&b2->vel, "\tball 2 velocity");
    vectNd_l2norm(&b2->vel, &mag);
    printf("\tball 2 speed: %g\n", mag);
    printf("\tball 2 mass:  %g\n", b2->mass);

    vectNd_alloc(&m1,b1->vel.n);
    vectNd_alloc(&m2,b2->vel.n);
    vectNd_scale(&b1->vel,b1->mass,&m1);
    vectNd_scale(&b2->vel,b2->mass,&m2);

    vectNd_alloc(&sum,m1.n);
    vectNd_add(&m1,&m2,&sum);

    vectNd_print(&sum,"\ttotal momentum");
    vectNd_l2norm(&sum,&mag);
    printf("\tmomentum magnitude = %g\n\n", mag);

    vectNd_free(&m1);
    vectNd_free(&m2);
    vectNd_free(&sum);
}

static double edge_radius = 0.1;
static double edge_red = 0.4;
static double edge_green = 0.4;
static double edge_blue = 0.4;

static int add_new_corner(scene *scn, vectNd *pos, double radius) {

    double diff;
    int dim = scn->dimensions;
    vectNd offset;
    vectNd_calloc(&offset,dim);

    /* check for existing ball sphere at location */
    char type_name[OBJ_TYPE_MAX_LEN];
    for(int i=0; i<scn->num_objects; ++i) {
        scn->object_ptrs[i]->type_name(type_name,sizeof(type_name));
        if( strcmp(type_name,"sphere") )
            continue;

        if( scn->object_ptrs[i]->size[0] != radius )
            continue;

        vectNd_sub(pos, &scn->object_ptrs[i]->pos[0], &offset);
        vectNd_l2norm(&offset, &diff);

        if( diff > EPSILON )
            continue;

        /* new sphere would be a duplicate */
        return 0;
    }

    vectNd_free(&offset);

    /* add sphere */
    object *obj;
    scene_alloc_object(scn, dim, &obj, "sphere");
    snprintf(obj->name, sizeof(obj->name),"corner");
    obj->red = edge_red;
    obj->green = edge_green;
    obj->blue = edge_blue;
    obj->red_r = obj->green_r = obj->blue_r = 0.1;
    object_add_pos(obj, pos);
    object_add_size(obj, radius + EPSILON);

    return 1;
}

static int recursive_add_edges(scene *scn, double radius, vectNd *curr) {
    
    int dim = scn->dimensions;

    /* add corner at new location */
    add_new_corner(scn,curr,radius);

    vectNd next;
    vectNd_calloc(&next,dim);
    for(int i=0; i<curr->n; ++i) {
        if( curr->v[i] > 0 ) {
            vectNd_copy(&next, curr);
            vectNd_set(&next, i, -box_size);

            /* add cylinder from curr to next */
            object *obj;
            scene_alloc_object(scn, dim, &obj, "cylinder");
            snprintf(obj->name, sizeof(obj->name),"edge");
            obj->red = edge_red;
            obj->green = edge_green;
            obj->blue = edge_blue;
            obj->red_r = obj->green_r = obj->blue_r = 0.1;
            object_add_pos(obj,curr);
            object_add_pos(obj,&next);
            object_add_size(obj,radius);
            object_add_flag(obj,1); /* open ends */

            recursive_add_edges(scn,radius,&next);
        }
    }
    vectNd_free(&next);

    return 0;
}

static int add_edges(scene *scn, double radius, int dimensions) {
    vectNd start;
    vectNd_calloc(&start,dimensions);

    for(int i=0; i<dimensions && i<3; ++i) {
        vectNd_set(&start,i,box_size);
    }

    recursive_add_edges(scn, radius, &start);
    vectNd_free(&start);

    return 0;
}

int scene_setup(scene *scn, int dimensions, int frame, int frames, char *config) {
    scene_init(scn, "balls", dimensions);
    printf("Generating frame %i of %i scene '%s'.\n", frame, frames, scn->name);
    if( config==NULL )
        printf("config string omitted.\n");

    /* make background sky(ish) blue */
    scn->bg_red = 0.3;
    scn->bg_green = 0.5;
    scn->bg_blue = 0.8;

    srand48(1);

    /* create a number of balls, randomly */
    if( balls == NULL ) {
        balls = calloc(num_balls,sizeof(ball_t));
        for(int i=0; i<num_balls; ++i) {
            /* random radiuses? */
            balls[i].radius = (max_radius-min_radius)*drand48()+min_radius;
            balls[i].mass = (max_mass-min_mass)*drand48()+min_mass;
            balls[i].red = drand48();
            balls[i].green = drand48();
            balls[i].blue = drand48();

            /* each with a random position */
            vectNd_alloc(&balls[i].pos,dimensions);
            for(int j=0; j<dimensions; ++j) {
                vectNd_set(&balls[i].pos,j,drand48()*(box_size-balls[i].radius)*2-box_size+balls[i].radius);
            }

            /* check for intersection with previous ball */
            int collision = 0;
            for(int j=0; j<i && !collision; ++j) {
                double dist;
                vectNd_dist(&balls[i].pos, &balls[j].pos, &dist);
                if( dist <= (balls[i].radius+balls[j].radius) )
                    collision = 1;
            }
            if( collision ) {
                vectNd_free(&balls[i].pos);
                --i;
                continue;
            }

            /* each with a random velocity */
            vectNd_alloc(&balls[i].vel,dimensions);
            for(int j=0; j<dimensions; ++j) {
                vectNd_set(&balls[i].vel,j,drand48()*max_velocity*2-max_velocity);
            }
        }
    }

    /* update using smaller time steps (10/frame) */
    int updates_per_frame=1000;
    vectNd movement;
    vectNd dir_i, dir_j, vdiff_i, vdiff_j;
    vectNd velocity_i, velocity_j, momentum_i, momentum_j;
    vectNd_calloc(&movement,dimensions);
    vectNd_alloc(&dir_i,dimensions);
    vectNd_alloc(&dir_j,dimensions);
    vectNd_alloc(&vdiff_i,dimensions);
    vectNd_alloc(&vdiff_j,dimensions);
    vectNd_alloc(&velocity_i,dimensions);
    vectNd_alloc(&velocity_j,dimensions);
    vectNd_alloc(&momentum_i,dimensions);
    vectNd_alloc(&momentum_j,dimensions);
    for(int k=0; k<updates_per_frame; ++k) {

        /* move each ball */
        for(int i=0; i<num_balls; ++i) {
            vectNd_scale(&balls[i].vel,1/(updates_per_frame*fps),&movement);   
            vectNd_add(&balls[i].pos,&movement,&balls[i].pos);

            /* check for wall collision */
            for(int j=0; j<dimensions; ++j) {
                double pos = balls[i].pos.v[j];
                double rad = balls[i].radius;
                if( pos+rad >= box_size ) {
                    balls[i].vel.v[j] *= -1.0;
                    double over_shoot = pos+rad - box_size;
                    balls[i].pos.v[j] = box_size-over_shoot-rad;
                } else if( pos-rad <= -box_size ) {
                    balls[i].vel.v[j] *= -1.0;
                    double over_shoot = pos-rad + box_size;
                    balls[i].pos.v[j] = -box_size-over_shoot+rad;
                }
            }
        }

        /* check for ball collisions */
        static int collision_update = -1;
        collision_update = 1<<30;
        for(int i=0; i<num_balls; ++i) {
            for(int j=i+1; j<num_balls; ++j) {
                if( i==j ) {
                    /* can't collide with self */
                    continue;
                }

                vectNd_sub(&balls[j].pos,&balls[i].pos,&dir_i);
                double dist;
                vectNd_l2norm(&dir_i,&dist);

                if( dist > balls[i].radius+balls[j].radius ) {
                    /* no collision */
                    continue;
                }

                /* balls have collided */

                if( k > collision_update ) {
                    printf("update %i: collision between balls %i and %i\n", k, i, j);
                    printf("dist instrusion = %g\n", balls[i].radius+balls[j].radius-dist);
                    print_total_momentum(&balls[i],&balls[j]);
                }

                /* allocate vectors */
                vectNd v_u1, v_u2;
                vectNd pos_dir; /* towards ball j */
                vectNd_calloc(&pos_dir,dimensions);
                vectNd_calloc(&v_u1,dimensions);
                vectNd_calloc(&v_u2,dimensions);

                /* project velocities onto inter ball vector */
                vectNd_sub(&balls[j].pos,&balls[i].pos,&pos_dir);
                vectNd_proj(&balls[i].vel,&pos_dir,&v_u1);
                vectNd_proj(&balls[j].vel,&pos_dir,&v_u2);

                /* determine direction of linear speed */
                double u1, u2;
                vectNd_l2norm(&v_u1,&u1);
                vectNd_l2norm(&v_u2,&u2);
                double dotProd;
                vectNd_dot(&v_u1,&pos_dir,&dotProd);
                if( dotProd <= 0 )
                    u1 = -u1;
                vectNd_dot(&v_u2,&pos_dir,&dotProd);
                if( dotProd <= 0 )
                    u2 = -u2;

                /* perform single dimension computation */
                double m1, m2;
                m1 = balls[i].mass;
                m2 = balls[j].mass;
                double v1, v2;
                v1 = (u1 * (m1 - m2) + 2 * m2 * u2) / (m1 + m2);
                v2 = (u2 * (m2 - m1) + 2 * m1 * u1) / (m1 + m2);

                /* make changes to actual velocities */

                /* remove original projected velocity */
                vectNd_sub(&balls[i].vel,&v_u1,&balls[i].vel);
                vectNd_sub(&balls[j].vel,&v_u2,&balls[j].vel);

                /* add updated projected velocity */
                vectNd_unitize(&pos_dir);
                vectNd_scale(&pos_dir, v1, &v_u1);
                vectNd_scale(&pos_dir, v2, &v_u2);
                vectNd_add(&balls[i].vel,&v_u1,&balls[i].vel);
                vectNd_add(&balls[j].vel,&v_u2,&balls[j].vel);

                if( k > collision_update ) {
                    print_total_momentum(&balls[i],&balls[j]);
                }

                collision_update = k;

                vectNd_free(&pos_dir);
                vectNd_free(&v_u1);
                vectNd_free(&v_u2);
            }
        }
    }
    vectNd_free(&movement);
    vectNd_free(&dir_i);
    vectNd_free(&dir_j);
    vectNd_free(&vdiff_i);
    vectNd_free(&vdiff_j);
    vectNd_free(&velocity_i);
    vectNd_free(&velocity_j);
    vectNd_free(&momentum_i);
    vectNd_free(&momentum_j);

    for(int i=0; i<num_balls; ++i) {
        /* add ball to scene */
        object *obj;
        scene_alloc_object(scn, dimensions, &obj, "sphere");
        snprintf(obj->name, sizeof(obj->name),"ball %i", i);
        obj->red = balls[i].red;
        obj->green = balls[i].green;
        obj->blue = balls[i].blue;
        obj->red_r = obj->green_r = obj->blue_r = 0.1;
        object_add_pos(obj, &balls[i].pos);
        object_add_size(obj, balls[i].radius);
    }

    /* add box */
    add_edges(scn,edge_radius,dimensions);

    /* add floor */
    /* add background */
    vectNd temp;
    vectNd_calloc(&temp,dimensions);
    object *ground=NULL;
    scene_alloc_object(scn,dimensions,&ground,"hplane");
    snprintf(ground->name, sizeof(ground->name),"ground");
    vectNd_reset(&temp);
    vectNd_set(&temp,2,-1.5*box_size);
    object_add_pos(ground,&temp);  /* position */
    vectNd_reset(&temp);
    vectNd_set(&temp,2,1);
    object_add_dir(ground,&temp);  /* normal */
    ground->red = 0.15;
    ground->green = 1.0;
    ground->blue = 0.2;
    vectNd_free(&temp);

    /* setup lights */
    scn->ambient.red = scn->ambient.green = scn->ambient.blue = 0.4;

    light *lgt = NULL;
    scene_alloc_light(scn,&lgt);
    lgt->type = LIGHT_DIRECTIONAL;
    vectNd_calloc(&lgt->dir,dimensions);
    for(int j=0; j<dimensions; ++j)
        vectNd_set(&lgt->dir,j,-1);
    lgt->red = lgt->green = lgt->blue = 0.2;

    /* setup camera */
    camera_reset(&scn->cam);
    camera_init(&scn->cam);
    vectNd up_vect;
    vectNd_calloc(&up_vect,dimensions);
    vectNd_set(&up_vect,2,10);
    vectNd viewPoint;
    vectNd viewTarget;
    vectNd_calloc(&viewPoint,dimensions);
    vectNd_calloc(&viewTarget,dimensions);
    vectNd_setStr(&viewPoint,"60,30,13,0");
    vectNd_setStr(&viewTarget,"0,0,0,0");
    camera_set_aim(&scn->cam, &viewPoint, &viewTarget, &up_vect, 0);
    vectNd_free(&viewPoint);
    vectNd_free(&viewTarget);
    vectNd_free(&up_vect);

    return 0;
}

int scene_cleanup() {
    for(int i=0; i<num_balls; ++i) {
        vectNd_free(&balls[i].vel);
        vectNd_free(&balls[i].pos);
    }
    free(balls); balls = NULL;
    return 0;
}
