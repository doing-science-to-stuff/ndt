/*
 * rubiks-solver.c
 * ndt: n-dimensional tracer
 *
 * Copyright (c) 2019 Bryan Franklin. All rights reserved.
 */
#include <stdio.h>
#include <unistd.h>
#include "../scene.h"

static int puzzle_dimensions = -1;

/*************************\
 * Puzzle Representation *
\*************************/
typedef struct puzzle_face {
    /* Note: using chars limits us to 6 dimensions */
    unsigned char c[3][3];
} face_t;

typedef struct puzzle {
    int n;
    face_t *faces;
} puzzle_t;

typedef struct piece_coord {
    int face;
    int i;
    int j;
} piece_coord_t;

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

static int plane_and_offset_ids(int id, int dims, int *plane_id, int *offset_id) {
    int num_planes = choose(puzzle_dimensions, 2);
    *plane_id = id % num_planes;
    *offset_id = id / num_planes;

    return 0;
}

static int plane_by_id(int id, int dims, int *dim1, int *dim2) {
    int orig_id = id;
    #if 0
    printf("%s: id: %i, dims: %i\n", __FUNCTION__, id, dims);
    #endif /* 0 */
    /* plane ids are essentially points in one half of a adjacency matrix
     * of dimensions.  So, consider just one half of such a matrix, and assign
     * each number to a position it it, then return the coordinates of that
     * position.
     *
     * Examples:
     * x
     * 0 x
     * 1 2 x
     * 
     * x
     * 0 x
     * 1 3 x
     * 2 4 5 x
     * 
     * x
     * 0 x
     * 1 4 x
     * 2 5 7 x
     * 3 6 8 9 x
     */
    int column = 0;
    int col_start = 1;
    int col_size = dims - 1;
    while( id+col_start >= dims && column < dims ) {
        #if 0
        printf("\tid: %i; column: %i; col_size: %i; col_start: %i\n", id, column, col_size, col_start);
        #endif /* 0 */
        id -= col_size;
        --col_size;
        ++col_start;
        ++column;
    }
    #if 0
    printf("\tid: %i; column: %i; col_size: %i; col_start: %i\n", id, column, col_size, col_start);
    #endif /* 0 */
    *dim1 = column;
    *dim2 = col_start + id;
    #if 0
    printf("\tdim1: %i; dim2: %i\n", *dim1, *dim2);
    #endif /* 0 */
    if( *dim1 >= dims || *dim2 >= dims ) {
        fprintf(stderr, "%s: impossible plane dimensions %i,%i (id=%i, dim=%i)\n", __FUNCTION__, *dim1, *dim2, orig_id, dims);
        exit(1);
    }

    return 0;
}

int puzzle_print_face(face_t *face) {
    for(int i=0; i<3; ++i) {
        printf("\t");
        for(int j=0; j<3; ++j) {
            printf("%2i", face->c[i][j]);
        }
        printf("\n");
    }
    return 0;
}

int puzzle_print(puzzle_t *puzzle) {
    int num_planes = choose(puzzle_dimensions, 2);
    int num_offsets = 1<<(puzzle_dimensions-2);
    printf("\n%iD Puzzle:\n", puzzle_dimensions);
    printf("\t%i offsets, %i planes\n", num_offsets, num_planes);
    for(int k=0; k<puzzle->n; ++k) {
        int plane_id, offset_id;
        plane_and_offset_ids(k, puzzle_dimensions, &plane_id, &offset_id);
        int pdim1, pdim2;
        plane_by_id(plane_id, puzzle_dimensions, &pdim1, &pdim2);
        printf("  face %i:", k);
        printf("  parallel to %i,%i", pdim1, pdim2);
        printf("  plane id %i, offset id %i", plane_id, offset_id);
        printf("\n");
        puzzle_print_face(&puzzle->faces[k]);
    }
    printf("\n\n");
    return 0;
}

int puzzle_create(puzzle_t *puzzle, int dimensions) {
    /* allocate faces */
    puzzle_dimensions = dimensions;
    puzzle->n = num_n_faces(dimensions, 2);
    #if 0
    printf("%i faces\n", puzzle->n);
    #endif /* 0 */
    puzzle->faces = calloc(puzzle->n, sizeof(face_t));

    /* color faces */
    for(int k=0; k<puzzle->n; ++k) {
        for(int i=0; i<3; ++i) {
            for(int j=0; j<3; ++j) {
                puzzle->faces[k].c[i][j] = k;
            }
        }
    }

    #if 0
    puzzle_print(puzzle);
    #endif /* 0 */
    return 0;
}

int puzzle_copy(puzzle_t *dst, puzzle_t *src) {
    dst->n = src->n;
    dst->faces = calloc(dst->n, sizeof(face_t));

    /* color faces */
    for(int f=0; f<dst->n; ++f) {
        for(int i=0; i<3; ++i) {
            for(int j=0; j<3; ++j) {
                dst->faces[f].c[i][j] = src->faces[f].c[i][j];
            }
        }
    }

    return 0;
}

int puzzle_face_center(puzzle_t *puzzle, int face, int pos_i, int pos_j, vectNd *pos) {
    vectNd_reset(pos);

    /* 0.5 + i in dim0, 0.5 + j in dim1 in face plane */
    int plane_id, offset_id;
    plane_and_offset_ids(face, puzzle_dimensions, &plane_id, &offset_id);
    int dim0, dim1;
    plane_by_id(plane_id, puzzle_dimensions, &dim0, &dim1);
    vectNd_set(pos, dim0, 0.5+pos_i);
    vectNd_set(pos, dim1, 0.5+pos_j);

    /* bits of offset_id indicate which dimensions to move in */
    int which = 0;
    while( offset_id > 0 ) {
        while( which == dim0 || which == dim1 )
            ++which;
        int value = offset_id % 2;

        vectNd_set(pos, which, value * 3);

        offset_id >>= 1;
        ++which;
    }

    return 0;
}

int puzzle_find_face_coord(puzzle_t *puzzle, vectNd *pos, int *face, int *pos_i, int *pos_j) {
    int num_faces = num_n_faces(puzzle_dimensions, 2);
    vectNd test;
    vectNd_calloc(&test, puzzle_dimensions);
    /* for each face */
    for(int f=0; f<num_faces; ++f) {
        /* for each position */
        for(int j=0; j<3; ++j) {
            for(int i=0; i<3; ++i) {
                puzzle_face_center(puzzle, f, i, j, &test);
                #if 0
                vectNd_print(&test, "testing");
                #endif /* 0 */
                double dist = -1;
                vectNd_dist(&test, pos, &dist);
                if( dist < 0.25 ) {
                    *face = f;
                    *pos_i = i;
                    *pos_j = j;
                    vectNd_free(&test);
                    return 1;
                }
            }
        }
    }
    vectNd_free(&test);

    fprintf(stderr, "Unable to find position on cube. (%i faces)\n", num_faces);
    vectNd_print(pos, "position");
    exit(1);
    return 0;
}

int puzzle_free(puzzle_t *puzzle) {
    free(puzzle->faces);  puzzle->faces = NULL;
    return 0;
}

/***********************\
 * Puzzle Manipulation *
\***********************/
typedef struct transitions {
    /* given a face and position, store the source face and position from
     * before the move occured */
    int num_faces;
    int face_size;
    piece_coord_t *table;
} transitions_t;

typedef struct move {
    /* Given that a rotation needs a single center of rotation,
     * that point must therefore be a point along a line connecting
     * the centers of two parallel faces.
     *
     * Thus, a move must consist of two distinct parallel faces,
     * and a selector for how far along that line the center
     * the rotation is {0.5,1.5,2.5}, or group dividers are
     * {{0,1}, {1,2}, {2,3}}.
     *
     * Additionally, the two parallel faces must be connected by a
     * perpendicular face, thus can only be offset from eachother on one
     * dimension that is othoganal to the plane.
     */
    int rot_dim_0, rot_dim_1;   /* dimensions of rotation plane */
    int conn_dim;   /* dimension connecting faces are in */
    int face_id; /* ids of faces that bound the move */
    int group_offset;   /* offset between faces where move occurs [0,1,2] */
    int dir;    /* which way the pieces rotate [0,1] */

    /* filled in after the move is specified */
    transitions_t *trans;
} move_t;

int puzzle_print_move(move_t *move) {
    printf("move: dir %i, plane %i,%i; dim %i, offset %i", move->dir, move->rot_dim_0, move->rot_dim_1, move->conn_dim, move->group_offset);
    printf("\n");

    return 0;
}

int enumerate_moves(puzzle_t *puzzle, move_t **list, int *num) {
    /* get number of moves */
    int num_faces = num_n_faces(puzzle_dimensions, 2);
    int num_moves = num_faces * (puzzle_dimensions - 2) * 3;
    #if 0
    printf("%s: %i possible moves.\n", __FUNCTION__, num_moves);
    #endif /* 0 */
    *num = num_moves;
    printf("%i expected moves.\n", num_moves);

    move_t *move = calloc(num_moves, sizeof(move_t));
    int curr = 0;
    for(int dir=0; dir<2; ++dir) {
        for(int f=0; f<num_faces; ++f) {
            /* determine rotation cooridinates */
            int plane_id, offset_id;
            plane_and_offset_ids(f, puzzle_dimensions, &plane_id, &offset_id);
            int dim0, dim1;
            plane_by_id(plane_id, puzzle_dimensions, &dim0, &dim1);
            #if 0
            printf("plane id: %i; dim0: %i; dim1: %i\n", p, dim0, dim1);
            #endif /* 0 */

            /* allow any face for which there is at least one dimension where
             * a move in the positive direction leads to another face.
             * i.e. any offset_id where there is a zero bit */
            if( offset_id == (1<<(puzzle_dimensions - 2))-1 )
                continue;

            /* loop through dimensions to find orthoginal ones */
            for(int dim=0; dim<puzzle_dimensions; ++dim) {
                if( dim == dim0 | dim == dim1 )
                    continue;

                /* if dimension `dim` is already 1 in offset_id, skip it */
                int value = offset_id % 2;
                offset_id >>= 1;
                if( value != 0 )
                    continue;

                /* enumerate group offsets */
                for(int which=0; which<3; ++which) {
                    if( curr == num_moves ) {
                        fprintf(stderr, "Too many moves! (%i)\n", curr+1);
                        exit(1);
                    }
                    move[curr].rot_dim_0 = dim0;
                    move[curr].rot_dim_1 = dim1;
                    move[curr].dir = dir;
                    move[curr].face_id = f;
                    move[curr].conn_dim = dim;
                    move[curr].group_offset = which;

                    curr += 1;
                }
            }
        }
    }
    #if 1
    printf("produced %i moves.\n", curr);
    #endif /* 0 */
    *list = move;

    return 0;
}

int puzzle_piece_in_group(move_t *move, vectNd *pos) {

    if( fabs( pos->v[move->conn_dim] - (move->group_offset+0.5) ) > 0.6 ) {
        return 0;
    }

    /* offset_id encodes what the value of `extra` dimensions must be */
    int plane_id, offset_id;
    plane_and_offset_ids(move->face_id, puzzle_dimensions, &plane_id, &offset_id);
    for(int i=0; i<puzzle_dimensions; ++i) {
        if( i==move->rot_dim_0 || i==move->rot_dim_1 || i==move->conn_dim )
            continue;

        /* extract low order bit and remove it */
        int value = offset_id % 2;
        offset_id >>= 1;

        if( fabs( pos->v[i] - value*3 ) > 0.25 )
            return 0;
    }

    return 1;
}

int puzzle_alloc_transitions(puzzle_t *puzzle, transitions_t **trans) {
    *trans = calloc(1,sizeof(transitions_t));
    int num_faces = num_n_faces(puzzle_dimensions, 2);
    int num = num_faces * 3 * 3;
    (*trans)->num_faces = num_faces;
    (*trans)->face_size = 3;
    (*trans)->table = calloc(num, sizeof(piece_coord_t));
    return 0;
}

int puzzle_set_piece_src(puzzle_t *puzzle, transitions_t *trans, int src_face, int src_i, int src_j, int dst_face, int dst_i, int dst_j) {
    int row = (trans->face_size*trans->face_size) * (dst_face) + (trans->face_size*(dst_j)) + (dst_i);
    trans->table[row].face = src_face;
    trans->table[row].i = src_i;
    trans->table[row].j = src_j;
    return 0;
}

int puzzle_get_piece_src(puzzle_t *puzzle, transitions_t *trans, int *face, int *i, int *j) {
    int row = (trans->face_size*trans->face_size) * (*face) + (trans->face_size*(*j)) + (*i);
    *face = trans->table[row].face;
    *i = trans->table[row].i;
    *j = trans->table[row].j;
    return 0;
}

int puzzle_fill_transitions(puzzle_t *puzzle, move_t *move) {
    int num_faces = num_n_faces(puzzle_dimensions, 2);
    vectNd center, piece, rotated;
    vectNd_calloc(&center, puzzle_dimensions);
    vectNd_calloc(&piece, puzzle_dimensions);
    vectNd_calloc(&rotated, puzzle_dimensions);
    /* set center of puzzle */
    for(int i=0; i<puzzle_dimensions; ++i)
        vectNd_set(&center, i, 3 / 2.0);

    /* allocate transistion table for move m */
    puzzle_alloc_transitions(puzzle, &move->trans);

    #if 0
    puzzle_print_move(move);
    #endif /* 0 */

    /* for each face */
    int cells_affected = 0;
    int cells_total = 0;
    for(int f=0; f<num_faces; ++f) {
        /* for each position */
        for(int j=0; j<3; ++j) {
            for(int i=0; i<3; ++i) {
                /* find center coordinate */
                puzzle_face_center(puzzle, f, i, j, &piece);

                /* apply move */
                if( puzzle_piece_in_group(move, &piece) ) {
                    if( move->dir == 0 )
                        vectNd_rotate(&piece, &center, move->rot_dim_0, move->rot_dim_1, M_PI / 2.0, &rotated);
                    else
                        vectNd_rotate(&piece, &center, move->rot_dim_0, move->rot_dim_1, -M_PI / 2.0, &rotated);

                    #if 0
                    printf("face %i, piece %i,%i\n", f, i, j);
                    vectNd_print(&piece, "\tpiece");
                    vectNd_print(&rotated, "\trotated");
                    #endif /* 0 */
                    ++cells_affected;
                } else {
                    vectNd_copy(&rotated, &piece);
                }
                ++cells_total;

                /* determine face and position on face for destination */
                int rot_face, rot_i, rot_j;
                puzzle_find_face_coord(puzzle, &rotated, &rot_face, &rot_i, &rot_j);
                #if 0
                printf("\t%i,%i,%i -> %i,%i,%i%s\t", f, i, j,
                        rot_face, rot_i, rot_j,
                        (f!=rot_face || i!=rot_i || j!=rot_j)?" *":"");
                vectNd_print(&piece, "\tpiece");
                #endif /* 0 */

                /* update transition table */
                puzzle_set_piece_src(puzzle, move->trans, f, i, j, rot_face, rot_i, rot_j);
            }
        }
    }
    vectNd_free(&center);
    vectNd_free(&piece);
    vectNd_free(&rotated);
    #if 0
    printf("move affected %i/%i cells.\n", cells_affected, cells_total);
    #endif /* 0 */

    return 0;
}

int puzzle_build_moves(puzzle_t *puzzle, move_t **moves, int *num_moves) {
    /* make list of possible moves */
    enumerate_moves(puzzle, moves, num_moves);

    /* for each possible move */
    for(int m=0; m<*num_moves; ++m) {
        #if 0
        printf("m: %i / %i\n", m, *num_moves);
        puzzle_print_move(&(*moves)[m]);
        #endif /* 0 */
        /* fill in transitions for each move */
        puzzle_fill_transitions(puzzle, &(*moves)[m]);
    }

    return 0;
}

int puzzle_apply_transitions(puzzle_t *dst, puzzle_t *src, transitions_t *trans) {
    /* color faces */
    for(int f=0; f<dst->n; ++f) {
        for(int i=0; i<3; ++i) {
            for(int j=0; j<3; ++j) {
                int src_f, src_i, src_j;
                src_f = f;
                src_i = i;
                src_j = j;
                puzzle_get_piece_src(src, trans, &src_f, &src_i, &src_j);
                dst->faces[f].c[i][j] = src->faces[src_f].c[src_i][src_j];
            }
        }
    }

    return 0;
}

int puzzle_apply_move(puzzle_t *puzzle, move_t *move) {
    /* make copy of curent state */
    puzzle_t backup;
    puzzle_copy(&backup, puzzle);

    /* fill move's transitions, if needed */
    if( move->trans == NULL ) {
        puzzle_fill_transitions(puzzle, move);
    }

    /* repopulate *puzzle by coping from backup */
    puzzle_apply_transitions(puzzle, &backup, move->trans);

    /* remove backup copy */
    puzzle_free(&backup);

    return 0;
}

/******************\
 * Puzzle Solving *
\******************/
typedef struct state {
    puzzle_t puzzle;
    double score;
} state_t;

double puzzle_state_score(puzzle_t *puzzle) {
    double score = 0.0;
    for(int f=0; f<puzzle->n; ++f) {
        for(int j=0; j<3; ++j) {
            for(int i=0; i<3; ++i) {
                /* count square that dont match center */
                if( puzzle->faces[f].c[i][j] != puzzle->faces[f].c[1][1] )
                    score += 1;
            }
        }
    }

    return score;
}

double puzzle_state_score2(puzzle_t *puzzle) {
    double score = 0.0;
    for(int f=0; f<puzzle->n; ++f) {
        for(int j=0; j<3; ++j) {
            for(int i=0; i<3; ++i) {
                /* count square that dont match original position */
                if( puzzle->faces[f].c[i][j] != f )
                    score += 1;
            }
        }
    }

    return score;
}

double heuristic(puzzle_t *puzzle) {
    /* (pre)determine how many face cells can be affected by a single move */
    int cells_per_move = 12;    /* 12 for 3D */

    /* count how many cells are out of place (as in puzzle_state_score) */
    double count = -1;
    #if 1
    /* to solve in any orientation */
    count = puzzle_state_score(puzzle);
    #else
    /* to get back to original orientation */
    count = puzzle_state_score2(puzzle);
    #endif /* 0 */

    /* divide total out of place cells, but number affected by each move */
    double h = count / cells_per_move;

    return h;
}

int puzzle_is_solved(puzzle_t *puzzle) {
    for(int f=0; f<puzzle->n; ++f) {
        for(int j=0; j<3; ++j) {
            for(int i=0; i<3; ++i) {
                if( puzzle->faces[f].c[i][j] != puzzle->faces[f].c[0][0] )
                    return  0;
            }
        }
    }

    /* no out of place pieces found, assume solved */
    return 1;
}

int puzzle_solve(puzzle_t *puzzle) {
    return -1;
}

/* scene_frames is optional, but gives the total number of frames to render
 * for an animated scene. */
int scene_frames(int dimensions, char *config) {
    return 1;
}

int scene_setup(scene *scn, int dimensions, int frame, int frames, char *config)
{
    double t = frame/(double)frames;
    scene_init(scn, "empty", dimensions);

    printf("Creating puzzle...");
    fflush(stdout);
    puzzle_t puzzle;
    puzzle_create(&puzzle, dimensions);
    printf("done!\n");

    printf("Finding possible moves...");
    fflush(stdout);
    move_t *moves = NULL;
    int num_moves;
    puzzle_build_moves(&puzzle, &moves, &num_moves);
    printf("done!\n");

    /* perturb puzzle, recording moves */
    printf("Perturbing puzzle...");
    fflush(stdout);
    int perturb_steps = 300;
    #if 0
    perturb_steps = num_moves;
    #endif /* 0 */
    for(int i=0; i<perturb_steps; ++i) {
        /* pick random move */
        int which = lrand48()%num_moves;
        #if 0
        which = i;
        #endif /* 0 */

        /* apply move */
        puzzle_apply_move(&puzzle, &moves[which]);

        #if 0
        printf("applied move %i\n", which);
        /* print updated state */
        puzzle_print(&puzzle);
        #if 1
        sleep(1);
        #endif /* 0 */
        #endif /* 0 */
    }
    printf("done!\n");

    /* solve puzzle, also recording moves */
    printf("Solving puzzle...");
    fflush(stdout);
    puzzle_solve(&puzzle);
    printf("done!\n");

    exit(1);

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
    light *lgt=NULL;
    scene_alloc_light(scn,&lgt);
    lgt->type = LIGHT_AMBIENT;
    lgt->red = 0.5;
    lgt->green = 0.5;
    lgt->blue = 0.5;

    scene_alloc_light(scn,&lgt);
    lgt->type = LIGHT_POINT;
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

    return 1;
}
