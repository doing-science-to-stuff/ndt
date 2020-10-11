/*
 * empty.c
 * ndt: n-dimensional tracer
 *
 * Copyright (c) 2019 Bryan Franklin. All rights reserved.
 */
#include <stdio.h>
#include "../scene.h"

#include "../../PuzzleMaze/maze.c"

static maze_t maze;
static int framesPerMove = 4;  /* 30s for 91*2 moves in original puzzle */
double edge_size = 30;


/* scene_frames is optional, but gives the total number of frames to render
 * for an animated scene. */
int scene_frames(int dimensions, char *config) {
    maze_load(&maze,config);
    int numFrames = maze.solution.posListNum*framesPerMove;
    return numFrames;
}

int add_mirror(scene *scn, int dimensions, int which, int which2,
               double mirror_size, double mirror_height, double mirror_dist) {

    vectNd hcubeDir;
    vectNd_alloc(&hcubeDir,dimensions);
    vectNd sizes;
    vectNd offset;
    vectNd_calloc(&sizes,dimensions);
    vectNd_calloc(&offset,dimensions);

    /* create a mirrored hcube */
    object *mirror=NULL;
    scene_alloc_object(scn,dimensions,&mirror,"hcube");
    for(int i=0; i<dimensions; ++i)
        object_add_size(mirror, mirror_size);
    mirror->size[which2] = mirror_height;
    mirror->size[which] = 0.1;
    for(int i=0; i<dimensions; ++i) {
        vectNd_reset(&hcubeDir);
        vectNd_set(&hcubeDir, i, 1.0);
        object_add_dir(mirror, &hcubeDir);
    }

    mirror->red = 0.1;
    mirror->green = 0.1;
    mirror->blue = 0.1;
    mirror->red_r = 0.95;
    mirror->green_r = 0.95;
    mirror->blue_r = 0.95;

    vectNd_reset(&offset);
    object_add_pos(mirror,&offset);
    vectNd_set(&offset,which,mirror_dist);
    object_move(mirror, &offset);

    return 0;
}

void add_maze_faces(object *puzzle, maze_t *maze, double edge_size) {

    printf("%s\n", __FUNCTION__);
    int dim = puzzle->dimensions;
    double scale = edge_size/maze->faces[0].rows;

    /* for each face in maze */
    for(int face=0; face < maze->numFaces; ++face) {
        int d1 = maze->faces[face].d1;
        int d2 = maze->faces[face].d2;
        int rows = maze->faces[face].rows;
        int cols = maze->faces[face].cols;
        /* for high and low values in all dimensions not in face's plane */
        vectNd offset;
        vectNd_alloc(&offset,dim);
        vectNd counter;
        vectNd_alloc(&counter,dim-2);
        int done = 0;
        printf("d1=%i; d2=%i\n", d1, d2);
        for(int i=0; i<dim; ++i)
            vectNd_set(&offset,i,-1.0);
        vectNd_set(&offset,d1,0.0);
        vectNd_set(&offset,d2,0.0);
        while( !done ) {
            vectNd_print(&counter,"\tcounter");

            /* create cluster */
            char faceName[64];
            snprintf(faceName,sizeof(faceName),"face %i,%i", d1, d2);
            object *faceCluster = object_alloc(dim, "cluster", faceName);
            object_add_flag(faceCluster,4);

            /* fill in face with hcube for each cell */
            for(int row=0; row<rows; ++row) {
                for(int col=0; col<cols; ++col) {
                    int cell = face_get_cell(&maze->faces[face], row, col);
                    if(cell==0)
                        continue;

                    /* create hcube for face cell */
                    char cellName[64];
                    snprintf(cellName,sizeof(cellName),"cell %i,%i for face %i", col, row, face);
                    object *box = object_alloc(dim, "hcube", cellName);

                    /* configure hcube */
                    vectNd cellPos;
                    vectNd_alloc(&cellPos,dim);
                    vectNd_reset(&cellPos);
                    vectNd_set(&cellPos, d1, (row-0.5*rows)*scale);
                    vectNd_set(&cellPos, d2, (col-0.5*cols)*scale);
                    object_add_pos(box, &cellPos);
                    //vectNd_print(&cellPos, "cellPos");

                    /* set up basis for hcube */
                    vectNd dir;
                    vectNd_alloc(&dir, dim);
                    for(int i=0; i<dim; ++i) {
                        vectNd_reset(&dir);
                        vectNd_set(&dir, i, 1);
                        object_add_dir(box, &dir);
                        object_add_size(box, scale);
                    }

                    switch(face) {
                        case 0: 
                            box->red = 1.0;
                            box->green = 1.0;
                            box->blue = 1.0;
                            break;
                        case 1: 
                            box->red = 0.0;
                            box->green = 1.0;
                            box->blue = 0.0;
                            break;
                        case 2: 
                            box->red = 0.0;
                            box->green = 0.0;
                            box->blue = 1.0;
                            break;
                        default:
                            box->red = 1.0;
                            box->green = 0.0;
                            box->blue = 0.0;
                            break;
                    }

                    /* add cell to face */
                    object_add_obj(faceCluster, box);

                    vectNd_free(&dir);
                    vectNd_free(&cellPos);
                }
            }

            /* convert counter into offset vector, skipping d1 and d2 */
            int k = 0, j = 0;
            vectNd_reset(&offset);
            while(k < dim-2 && j < dim) {
                while( j == d1 || j == d2 ) {
                    ++j;
                    continue;
                }
                /* subtract 1 here to account for thickness of face */
                int dimK = maze->dimensions[k]-1;
                /* subtract 0.5 from counter to center at 0 */
                /* subtract 0.5 to shift both faces by half of their thickness */
                double dist = dimK*(counter.v[k]-0.5)-0.5;
                //printf("copying counter[%i] (%g) into offset[%i], dimK=%i, dist=%g\n", k, counter.v[k], j, dimK, dist);
                vectNd_set(&offset, j++, dist);
                k++;
            }

            /* move face into position */
            vectNd scaledOffset;
            vectNd_alloc(&scaledOffset,dim);
            //vectNd_print(&offset,"\toffset");
            vectNd_scale(&offset, scale+0.01, &scaledOffset);
            object_move(faceCluster, &scaledOffset);
            //vectNd_print(&scaledOffset,"\tscaledOffset");

            /* update counter */
            j = 0;
            while(j < dim-2 && counter.v[j] == 1 ) {
                vectNd_set(&counter,j++,0);
            }
            if( j < dim-2 )
                vectNd_set(&counter,j,1);
            else
                done = 1;

            /* add face cluster to puzzle */
            object_add_obj(puzzle, faceCluster);
            faceCluster = NULL;
        }
        vectNd_free(&counter);
        vectNd_free(&offset);
    }
        
}

void add_movable_piece(object *puzzle, maze_t *maze, double edge_size, int frame, int frames) {
    
    double scale = edge_size / maze->faces[0].rows;

    int dimensions = maze->numDimensions;
    /* create cluster to add sub-pieces to */
    object *piece = object_alloc(dimensions, "cluster", "movable piece");
    object_add_flag(piece, 1);
    object_add_obj(puzzle, piece);

    /* for each dimension, add an hcube lengthened in that dimension */
    vectNd hcubeDir;
    vectNd_alloc(&hcubeDir, dimensions);
    for(int d=0; d<dimensions; ++d) {
        object *obj = object_alloc(dimensions, "hcube", "movable piece part");
        for(int i=0; i<dimensions; ++i) {
            if( i==d )
                object_add_size(obj, 2.0*edge_size);
            else
                object_add_size(obj, scale);
        }
        for(int i=0; i<dimensions; ++i) {
            vectNd_reset(&hcubeDir);
            vectNd_set(&hcubeDir, i, 1.0);
            object_add_dir(obj, &hcubeDir);
        }
        vectNd offset;
        vectNd_alloc(&offset, dimensions);
#if 0
        for(int i=0; i<dimensions; ++i)
            vectNd_set(&offset, i, -edge_size);
#endif // 0
        object_add_pos(obj,&offset);
        obj->red = 0.8;
        obj->blue = 0.8;
        obj->green = 0.8;
        object_add_obj(piece, obj);
    }

    /* get location of center */
    int pos1, pos2;
    double posW;
    int hframes = frames/2;
    if( frame <= hframes ) {
        /* first half goes forward */
        pos1 = frame / framesPerMove;
        pos2 = pos1+1;
        posW = (frame % framesPerMove) / (double)framesPerMove;
    } else {
        /* second half goes backwards */
        int rframe = frames-frame;
        pos1 = rframe / framesPerMove;
        pos2 = pos1+1;
        posW = (rframe % framesPerMove) / (double)framesPerMove;
    }
    vectNd pos;
    vectNd_alloc(&pos, dimensions);
    for(int i=0; i<dimensions; ++i) {
        vectNd_set(&pos, i,
            ((1.0-posW) * maze->solution.positions[pos1][i]
            + posW * maze->solution.positions[pos2][i]
            - maze->dimensions[i]/2.0)
            * scale);
    }
    printf("pos1: %i, pos2: %i, posW: %g\n", pos1, pos2, posW);
    vectNd_print(&pos, "piece at");

    /* move cluster to correct location */
    object_move(piece,&pos);
}

int scene_setup(scene *scn, int dimensions, int frame, int frames, char *config)
{
    double t = frame/(double)frames;
    scene_init(scn, "maze-cube", dimensions);

    printf("Generating frame %i of %i scene '%s' (%.2f%% through animation).\n",
            frame, frames, scn->name, 100.0*t);

    /* zero out camera */
    camera_reset(&scn->cam);

    /* move camera into position */
    vectNd viewPoint;
    vectNd viewTarget;
    vectNd up_vect;
    vectNd lookVec;
    vectNd_calloc(&viewPoint,dimensions);
    vectNd_calloc(&viewTarget,dimensions);
    vectNd_calloc(&up_vect,dimensions);
    vectNd_alloc(&lookVec, dimensions);

    vectNd_setStr(&viewTarget,"-20,-10,20,0");
    vectNd_setStr(&viewPoint,"160,30,-120,0");
    vectNd_set(&up_vect,1,1);  /* 0,1,0,0... */
    vectNd_sub(&viewPoint, &viewTarget, &lookVec);
    //vectNd_rotate2(&viewPoint, &viewTarget, &lookVec, &up_vect, 10.0*M_PI/180.0, &viewPoint);
    camera_set_aim(&scn->cam, &viewPoint, &viewTarget, &up_vect, 0.0);
    camera_set_flip(&scn->cam, 1, 0);
    vectNd_free(&up_vect);
    //vectNd_free(&viewPoint);
    vectNd_free(&viewTarget);

    /* basic setup */
    #if 1
    scn->bg_red = 0.09;
    scn->bg_green = 0.25;
    scn->bg_blue = 0.64;
    #endif /* 0 */
    scn->ambient.red = 0.0;
    scn->ambient.green = 0.0;
    scn->ambient.blue = 0.0;

    /* setup lighting */
    light *lgt=NULL;
    scene_alloc_light(scn,&lgt);
    lgt->type = LIGHT_AMBIENT;
    lgt->red = 0.4;
    lgt->green = 0.4;
    lgt->blue = 0.4;

    scene_alloc_light(scn,&lgt);
    #if 0
    lgt->type = LIGHT_POINT;
    vectNd_calloc(&lgt->pos,dimensions);
    vectNd_setStr(&lgt->pos,"0,40,0,-40");
    lgt->red = 300;
    lgt->green = 300;
    lgt->blue = 300;
    #else
    lgt->type = LIGHT_DIRECTIONAL;
    vectNd_calloc(&lgt->dir,dimensions);
    vectNd_setStr(&lgt->dir,"0,-1,0,-1");
    lgt->red = 0.4;
    lgt->green = 0.4;
    lgt->blue = 0.4;
    #endif

    scene_alloc_light(scn,&lgt);
    lgt->type = LIGHT_DIRECTIONAL;
    vectNd_calloc(&lgt->dir,dimensions);
    vectNd_setStr(&lgt->dir,"-180,-40,0,0");
    lgt->red = 0.1;
    lgt->green = 0.1;
    lgt->blue = 0.1;
    vectNd_free(&viewPoint);

    scene_alloc_light(scn,&lgt);
    lgt->type = LIGHT_DIRECTIONAL;
    vectNd_calloc(&lgt->dir,dimensions);
    vectNd_setStr(&lgt->dir,"0,-40,140,0");
    lgt->red = 0.1;
    lgt->green = 0.1;
    lgt->blue = 0.1;
    vectNd_free(&viewPoint);

    vectNd temp;
    vectNd_calloc(&temp,dimensions);

    #if 1
    /* add background */
    object *ground=NULL;
    scene_alloc_object(scn,dimensions,&ground,"hplane");
    vectNd_reset(&temp);
    vectNd_set(&temp,1,-45);
    object_add_pos(ground,&temp);  /* position */
    vectNd_reset(&temp);
    vectNd_set(&temp,1,1);
    object_add_dir(ground,&temp);  /* normal */
    ground->red = 0.0225;
    ground->green = 1.0;
    ground->blue = 0.04;
    #endif /* 0 */

    #if 1
    /* add mirrors */
    double mirror_size = 140;
    double mirror_height = 80;
    double mirror_dist = 66;
    /* positive z */
    add_mirror(scn, dimensions, 2, 1, mirror_size, mirror_height, mirror_dist);
    /* negative x */
    add_mirror(scn, dimensions, 0, 1, mirror_size, mirror_height, -mirror_dist);
    #if 0
    /* positive w */
    add_mirror(scn, dimensions, 3, 1, mirror_size, mirror_height, mirror_dist);
    #endif /* 0 */
    #endif /* 0 */

    /* cluster will contain puzzle and be used to rotate it */
    object *clstr = NULL;
    scene_alloc_object(scn,dimensions,&clstr,"cluster");
    object_add_flag(clstr, 8);  /* set number of clusters */

    /* create objects */
    #if 0
    /* add placeholder cube */
    /* use object_alloc instead of scene_alloc_object when the object will be
     * added to a cluster */
    vectNd hcubeDir;
    vectNd_alloc(&hcubeDir, dimensions);
    object *obj = object_alloc(dimensions, "hcube", "placeholder cube");
    for(int i=0; i<dimensions; ++i)
        object_add_size(obj, edge_size);
    for(int i=0; i<dimensions; ++i) {
        vectNd_reset(&hcubeDir);
        vectNd_set(&hcubeDir, i, 1.0);
        object_add_dir(obj, &hcubeDir);
    }
    vectNd offset;
    vectNd_alloc(&offset, dimensions);
    #if 0
    for(int i=0; i<dimensions; ++i)
        vectNd_set(&offset, i, -edge_size);
    #endif // 0
    object_add_pos(obj,&offset);
    obj->red = 0.1;
    obj->blue = 0.8;
    obj->green = 0.1;
    object_add_obj(clstr, obj);
    #else
    add_maze_faces(clstr, &maze, edge_size);
    #endif
    add_movable_piece(clstr, &maze, edge_size, frame, frames);

    #if 1
    /* rotate puzzle into view orientation */
    vectNd rotateCenter, rotV1, rotV2;
    vectNd_alloc(&rotateCenter, dimensions);
    vectNd_alloc(&rotV1, dimensions);
    vectNd_alloc(&rotV2, dimensions);
    for(int i=0; i<dimensions; ++i) {
        vectNd_set(&rotV1,i,1.0);
        vectNd_set(&rotV2,i,1.0);
    }
    vectNd_set(&rotV1,1,0.0);
    double angle = (M_PI/2.0) - atan(1.0/sqrt(dimensions-1));
    object_rotate2(clstr, &rotateCenter, &rotV1, &rotV2, angle);
    object_rotate(clstr, &rotateCenter, 0, 2, frame*2.0*M_PI/frames);
    #endif // 1
    
    return 1;
}

int scene_cleanup() {
    /* If any persistent resources were allocated,
     * they should be freed here. */
    return 0;
}
