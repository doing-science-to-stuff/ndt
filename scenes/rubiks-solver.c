/*
 * rubiks-solver.c
 * ndt: n-dimensional tracer
 *
 * Copyright (c) 2019 Bryan Franklin. All rights reserved.
 */
#include <stdio.h>
#include <string.h>
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

    /* copy faces */
    memcpy(dst->faces, src->faces, dst->n * sizeof(face_t));

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

typedef struct gen_list {
    void *list;
    size_t width;
    size_t num;
    size_t cap;
} gen_list_t;

int puzzle_print_move(move_t *move) {
    printf("move: dir %i, plane %i,%i; dim %i, offset %i", move->dir, move->rot_dim_0, move->rot_dim_1, move->conn_dim, move->group_offset);
    printf("\n");

    return 0;
}

int move_copy(move_t *dst, move_t *src) {
    memcpy(dst,src,sizeof(move_t));
    dst->trans = NULL;
    return 0;
}

int gen_list_init(gen_list_t *list, size_t width) {
    memset(list, '\0', sizeof(gen_list_t));
    list->width = width;
    return 0;
}

int gen_list_append(gen_list_t *list, void *item) {
    if( list->num == list->cap ) {
        int new_cap = 2*list->cap + 1;
        move_t *tmp = malloc(new_cap*list->width);
        if( tmp==NULL ) {
            perror("malloc");
            exit(1);
        }
        memcpy(tmp,list->list,list->num*list->width);
        free(list->list); list->list = NULL;
        list->list = tmp;
        list->cap = new_cap;
    }

    memcpy(list->list + list->num * list->width, item, list->width);
    ++list->num;

    return 0;
}

void *gen_list_item_ptr(gen_list_t *list, int pos) {
    return list->list + pos*list->width;
}

int gen_list_remove(gen_list_t *list, int pos) {
    if( pos >= list->num )
        return -1;

    memmove(list->list + pos*list->width,
            list->list + (pos+1)*list->width,
            (list->num-pos-1) * sizeof(list->width));
    --list->num;

    return 0;
}

int enumerate_moves(puzzle_t *puzzle, gen_list_t *list) {
    gen_list_init(list, sizeof(move_t));

    /* get number of moves */
    int num_faces = num_n_faces(puzzle_dimensions, 2);
    int num_moves = num_faces * (puzzle_dimensions - 2) * 3;
    #if 0
    printf("%s: %i possible moves.\n", __FUNCTION__, num_moves);
    #endif /* 0 */
    printf("%i expected moves.\n", num_moves);

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
                if( dim == dim0 || dim == dim1 )
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
                    move_t move;
                    move.rot_dim_0 = dim0;
                    move.rot_dim_1 = dim1;
                    move.dir = dir;
                    move.face_id = f;
                    move.conn_dim = dim;
                    move.group_offset = which;
                    gen_list_append(list, &move);
                }
            }
        }
    }
    #if 1
    printf("produced %lu moves.\n", list->num);
    #endif /* 0 */

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

int puzzle_build_moves(puzzle_t *puzzle, gen_list_t *moves) {
    /* make list of possible moves */
    enumerate_moves(puzzle, moves);

    /* for each possible move */
    for(int m=0; m<moves->num; ++m) {
        #if 0
        printf("m: %i / %i\n", m, *num_moves);
        puzzle_print_move(gen_list_item_ptr(moves,m));
        #endif /* 0 */
        /* fill in transitions for each move */
        puzzle_fill_transitions(puzzle, gen_list_item_ptr(moves,m));
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
    puzzle_t puzzle;    /* current puzzle configuration */
    move_t move;        /* move that lead to this state */
    int path_length;    /* number of moves to get here */
    struct state *prev; /* state that was modified by move to get here */
} state_t;

typedef struct priority_entry {
    double priority;
    void *payload;
} priority_entry_t;

typedef struct priority_queue {
    priority_entry_t *entries;
    size_t num;
    size_t cap;
} priority_queue_t;

int priority_init(priority_queue_t *queue) {
    memset(queue,'\0',sizeof(priority_queue_t));
    return 0;
}

int priority_enqueue(priority_queue_t *queue, void *payload, double priority) {

    /* resize as needed */
    if( queue->num == queue->cap ) {
        priority_entry_t *tmp;
        size_t new_cap = queue->cap * 2 + 1;
        new_cap = (new_cap<1)?1:new_cap;
        tmp = malloc(new_cap * sizeof(priority_entry_t));
        if( tmp == NULL ) {
            perror("malloc");
            exit(1);
        }
        queue->cap = new_cap;
        memcpy(tmp, queue->entries, queue->num * sizeof(priority_entry_t));
        free(queue->entries); queue->entries = NULL;
        queue->entries = tmp;
    }

    /* add new entry to the end */
    queue->entries[queue->num].payload = payload;
    queue->entries[queue->num].priority = priority;
    ++queue->num;

    /* heap up */
    int pos = queue->num - 1;
    int parent = (pos-1) / 2;
    if( queue->entries[pos].priority < queue->entries[parent].priority ) {
        priority_entry_t tmp;
        memcpy(&tmp, &queue->entries[pos], sizeof(priority_entry_t));
        memcpy(&queue->entries[pos], &queue->entries[parent], sizeof(priority_entry_t));
        memcpy(&queue->entries[parent], &tmp, sizeof(priority_entry_t));
    }
    #if 0
    printf("%s: queue now contains %zu items.\n", __FUNCTION__, queue->num);
    #endif /* 0 */

    return 0;
}

int priority_dequeue(priority_queue_t *queue, void **ptr) {

    /* check for empty queue */
    if( queue->num < 1 ) {
        *ptr = NULL;
        return -1;
    }

    /* record return value */
    *ptr = queue->entries[0].payload;

    /* replace root with last element */
    if( queue->num > 1 ) {
        memcpy(&queue->entries[0], &queue->entries[queue->num-1], sizeof(priority_entry_t));
        memset(&queue->entries[queue->num-1], '\0', sizeof(priority_entry_t));
    }
    if( queue->num > 0 )
        --queue->num;
    if( queue->num == 0 ) {
        printf("%s: queue now empty.\n", __FUNCTION__);
        return 0;
    }

    /* heap-down */
    int pos = 0;
    int done = 0;
    do {
        done = 1;
        
        int left = pos * 2 + 1;
        int right = pos * 2 + 2;
        int swap = -1;
        double target_prio = queue->entries[pos].priority;

        if( left<queue->num && queue->entries[left].priority < target_prio ) {
            swap = left;
            target_prio = queue->entries[left].priority;
        }
        if( right<queue->num && queue->entries[right].priority < target_prio ) {
            swap = right;
            target_prio = queue->entries[right].priority;
        }

        if( swap > 0 ) {
            priority_entry_t tmp;
            memcpy(&tmp, &queue->entries[pos], sizeof(priority_entry_t));
            memcpy(&queue->entries[pos], &queue->entries[swap], sizeof(priority_entry_t));
            memcpy(&queue->entries[swap], &tmp, sizeof(priority_entry_t));
            pos = swap;
            done = 0;
        }

    } while( done != 1 );
    #if 0
    printf("%s: queue now contains %zu items.\n", __FUNCTION__, queue->num);
    #endif /* 0 */

    return 0;
}

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
                #if 1
                /* to solve in any orientation */
                if( puzzle->faces[f].c[i][j] != puzzle->faces[f].c[0][0] )
                    return  0;
                #else
                /* solved in original orientation */
                if( puzzle->faces[f].c[i][j] != f )
                    return  0;
                #endif /* 0 */
            }
        }
    }

    /* no out of place pieces found, assume solved */
    return 1;
}

int puzzle_solve(puzzle_t *puzzle, gen_list_t *valid, gen_list_t *solution) {
    if( valid == NULL || valid->num == 0 ) {
        fprintf(stderr,"%s requires a list of valid moves.\n", __FUNCTION__);
        return -1;
    }

    /* initialize data storage */
    gen_list_init(solution, sizeof(move_t));
    gen_list_t states;
    gen_list_init(&states, sizeof(state_t*));
    priority_queue_t queue;
    priority_init(&queue);

    /* contruct initial state */
    state_t *initial = NULL;
    initial = calloc(1,sizeof(state_t));
    memset(initial, '\0', sizeof(state_t));
    puzzle_copy(&initial->puzzle, puzzle);
    gen_list_append(&states, &initial);

    /* compute heuristic function */
    double h = heuristic(puzzle);

    /* place initial state into priority queue */
    priority_enqueue(&queue, initial, h);

    /* while not done */
    int solved = 0;
    state_t *curr = NULL;
    int iter = 0;
    while( !solved && queue.num > 0 ) {

        if( queue.entries[0].payload == NULL )
            puzzle_print(&((state_t*)queue.entries[0].payload)->puzzle);

        /* remove state from queue */
        void *ptr = NULL;
        priority_dequeue(&queue, &ptr);
        curr = (state_t*)ptr;
        #if 1
        if( (++iter % 100) == 0 ) {
            printf("%zu states created.\n", states.num);
            printf("curr:\n");
            puzzle_print(&curr->puzzle);
            printf("path length: %i\n", curr->path_length);
            printf("h: %g\n", heuristic(&curr->puzzle));
        }
        #endif /* 1 */

        /* test for solved condition */
        if( puzzle_is_solved(&curr->puzzle) ) {
            printf("Found a solution!\n");
            puzzle_print(&curr->puzzle);
            solved = 1;
            break;
        }

        /* apply all valid moves to get new states */
        state_t *next = NULL;
        for(int i=0; i<valid->num; ++i) {
            /* apply move */
            move_t *move_ptr = gen_list_item_ptr(valid, i);
            next = calloc(1,sizeof(state_t));
            puzzle_copy(&next->puzzle, &curr->puzzle);
            
            #if 0
            printf("before:\n");
            puzzle_print(&curr->puzzle);
            puzzle_print(&next->puzzle);
            #endif /* 0 */

            puzzle_apply_move(&next->puzzle, move_ptr);

            #if 0
            printf("after:\n");
            puzzle_print(&curr->puzzle);
            puzzle_print(&next->puzzle);
            #endif /* 0 */

            move_copy(&next->move, move_ptr);
            next->path_length = curr->path_length+1;
            next->prev = curr;

            /* check for duplicate */
            int is_dupe = 0;
            for(int j=0; j<states.num && !is_dupe; ++j) {
                state_t *check = *((state_t**)gen_list_item_ptr(&states, j));
                if( memcmp(next->puzzle.faces, check->puzzle.faces, next->puzzle.n * sizeof(face_t)) == 0 ) {
                    is_dupe = 1;
                    #if 0
                    puzzle_print(&next->puzzle);
                    puzzle_print(&check->puzzle);
                    #endif /* 0 */
                } else {
                    #if 0
                    printf("not a dupe!\n");
                    puzzle_print(&next->puzzle);
                    #endif /* 0 */
                }
            }
            if( is_dupe ) {
                #if 0
                printf("dupe detected.\n");
                #endif /* 0 */
                puzzle_free(&next->puzzle);
                free(next); next = NULL;
                continue;
            }

            /* compute heuristic for new state */
            double h = heuristic(&next->puzzle);

            /* place new state into priority queue */
            gen_list_append(&states, &next);
            priority_enqueue(&queue, next, next->path_length + h);
        }
    }

    /* once solved */
    gen_list_t temp;
    gen_list_init(&temp, sizeof(move_t));
    /* start from solved state */

    /* traverse prev pointers */
    while( curr != NULL ) {
        /* append move to temp list */
        gen_list_append(&temp, &curr->move);

        /* move to previous state */
        curr = curr->prev;
    }

    /* reverse temp list into solution */
    for(int i=temp.num-1; i>=0; --i) {
        gen_list_append(solution, gen_list_item_ptr(&temp, i));
    }

    return 0;
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

    printf("Finding valid moves...");
    fflush(stdout);
    gen_list_t valid_moves;
    puzzle_build_moves(&puzzle, &valid_moves);
    printf("done!\n");

    /* perturb puzzle, recording moves */
    printf("Perturbing puzzle...");
    fflush(stdout);
    int perturb_steps = 6;
    #if 0
    perturb_steps = valid_moves.num;
    #endif /* 0 */
    for(int i=0; i<perturb_steps; ++i) {
        /* pick random move */
        int which = lrand48()%valid_moves.num;
        #if 0
        which = i;
        #endif /* 0 */

        /* apply move */
        puzzle_apply_move(&puzzle, gen_list_item_ptr(&valid_moves, which));

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
    gen_list_t solve_moves;
    printf("Solving puzzle...");
    fflush(stdout);
    puzzle_solve(&puzzle, &valid_moves, &solve_moves);
    printf("%zu moves in solution...", solve_moves.num);
    /* apply solution */
    for(int i=0; i<solve_moves.num; ++i) {
        puzzle_apply_move(&puzzle, gen_list_item_ptr(&solve_moves, i));
    }
    if( ! puzzle_is_solved(&puzzle) ) {
        puzzle_print(&puzzle);
        fflush(stdout);
        fprintf(stderr, "FAILED!  Puzzle is not solved!\n\n");
        exit(1);
    } else {
        printf("SUCCESS!!!\n");
    }

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
