/*
 * kd-tree.c
 * ndt: n-dimensional tracer
 *
 * Copyright (c) 2021 Bryan Franklin. All rights reserved.
 */
#include "kd-tree.h"
#include <float.h>
#include <stdio.h>

/* aabb */

int aabb_init(aabb_t *bb, int dimensions) {
    vectNd_alloc(&bb->lower, dimensions);
    vectNd_alloc(&bb->upper, dimensions);
    vectNd_fill(&bb->lower, DBL_MAX);
    vectNd_fill(&bb->upper, -DBL_MAX);
    return 1;
}

int aabb_free(aabb_t *bb) {
    vectNd_free(&bb->lower);
    vectNd_free(&bb->upper);
    return 1;
}

int aabb_copy(aabb_t *dst, aabb_t *src) {
    vectNd_copy(&dst->lower, &src->lower);
    vectNd_copy(&dst->upper, &src->upper);
    return 1;
}

int aabb_add(aabb_t *dst, aabb_t *src) {
    int dimensions = dst->lower.n;
    for(int i=0; i<dimensions; ++i) {
        double vl_d, vu_d, vl_s, vu_s;
        vectNd_get(&dst->lower, i, &vl_d);
        vectNd_get(&dst->upper, i, &vu_d);
        vectNd_get(&src->lower, i, &vl_s);
        vectNd_get(&src->upper, i, &vu_s);

        if( vl_s < vl_d ) {
            vl_d = vl_s;
            vectNd_set(&dst->lower, i, vl_d);
        }
        if( vu_s > vu_d ) {
            vu_d = vu_s;
            vectNd_set(&dst->upper, i, vu_d);
        }
    }
    return 1;
}

int aabb_add_point(aabb_t *bb, vectNd *pnt) {
    double bbl, bbu, pv;
    int dimensions = pnt->n;
    for(int i=0; i<dimensions; ++i) {
        vectNd_get(&bb->lower, i, &bbl);
        vectNd_get(&bb->upper, i, &bbu);
        vectNd_get(pnt, i, &pv);

        if( pv < bbl ) {
            bbl = pv;
            vectNd_set(&bb->lower, i, bbl);
        }
        if( pv > bbu ) {
            bbu = pv;
            vectNd_set(&bb->upper, i, bbu);
        }
    }
    return 1;
}

/* test if ray o+v*t intersects bounding box bb */
static int aabb_intersect(aabb_t *bb, vectNd *o, vectNd *v) {
    int dimensions = o->n;
    vectNd pl, pu, tmp;
    vectNd_alloc(&pl, dimensions);
    vectNd_alloc(&pu, dimensions);
    vectNd_alloc(&tmp, dimensions);
    for(int i=0; i<dimensions; ++i) {
        double vo, vv, vl, vu;
        vectNd_get(o, i, &vo);
        vectNd_get(v, i, &vv);
        vectNd_get(&bb->lower, i, &vl);
        vectNd_get(&bb->upper, i, &vu);

        /* find paramters tu and tl for o+v*{tl,tu} */
        double vvMvo = vv-vo;
        if( fabs(vvMvo) < EPSILON )
            continue;   /* v is parallel to dimension i */
        double tl, tu;
        tl = (vl-vo) / vvMvo;
        tu = (vu-vo) / vvMvo;

        if( tl < -EPSILON && tu < -EPSILON ) {
            vectNd_free(&pl);
            vectNd_free(&pu);
            vectNd_free(&tmp);
            return 0;   /* bb is behind o along v */
        }

        /* compute intersection points along faces */
        vectNd_scale(v, tl, &tmp);
        vectNd_add(o, &tmp, &pl);
        vectNd_scale(v, tu, &tmp);
        vectNd_add(o, &tmp, &pu);

        int il=1, iu=1;
        for(int j=0; j<dimensions; ++j) {
            if( j==i ) continue;

            double plj, puj, bbl, bbu;
            vectNd_get(&pl, j, &plj);
            vectNd_get(&pu, j, &puj);
            vectNd_get(&bb->lower, j, &bbl);
            vectNd_get(&bb->upper, j, &bbu);
            if( plj < bbl || plj > bbu )
                il = 0;
            if( puj < bbl || puj > bbu )
                iu = 0;
        }

        if( il!=0 || iu!=0 ) {
            vectNd_free(&pl);
            vectNd_free(&pu);
            vectNd_free(&tmp);
            return 1;   /* pl or pu is inside one of the faces of bb */
        }
    }
    vectNd_free(&pl);
    vectNd_free(&pu);
    vectNd_free(&tmp);

    return 0;
}

/* kd_item */

int kd_item_init(kd_item_t *item, int dimensions) {
    memset(item, '\0', sizeof(kd_item_t));
    aabb_init(&item->bb, dimensions);
    return 1;
}

int kd_item_free(kd_item_t *item) {
    aabb_free(&item->bb);
    memset(item, '\0', sizeof(kd_item_t));
    return 1;
}

int kd_item_copy(kd_item_t *dst, kd_item_t *src) {
    vectNd_copy(&dst->bb.lower, &src->bb.lower);
    vectNd_copy(&dst->bb.upper, &src->bb.upper);
    dst->ptr = src->ptr;
    return 1;
}

/* kd_item_list */

/* Note: the item list is a list of pointers to kd_item_t structs, and is
 *       maintained in heap order (by pointer value). */

int kd_item_list_init(kd_item_list_t *list) {
    memset(list, '\0', sizeof(kd_item_list_t));
    list->items = (kd_item_t**)malloc(101*sizeof(kd_item_t));
    return 1;
}

int kd_item_list_free(kd_item_list_t *list, int free_items) {
    if( free_items ) {
        for(int i=0; i<list->n; ++i)
            kd_item_free(list->items[i]);
    }
    free(list->items); list->items = NULL;
    memset(list, '\0', sizeof(kd_item_list_t));
    return 1;
}

static int kd_item_list_heapup(kd_item_list_t *list, int pos) {
    int parent_pos = (pos-1)/2;

    kd_item_t **items = list->items;
    while( parent_pos>=0 && items[pos] < items[parent_pos] ) {
        kd_item_t *tmp = items[pos];
        items[pos] = items[parent_pos];
        items[parent_pos] = tmp;
        pos = parent_pos;
        parent_pos = (pos-1)/2;
    }

    return 1;
}

static int kd_item_list_heapdown(kd_item_list_t *list, int pos) {
    int left_pos = pos*2+1;
    int right_pos = pos*2+2;
    if( left_pos >= list->n )
        return 1;

    kd_item_t *val = list->items[pos];
    kd_item_t *left = list->items[left_pos];

    int swap_pos = left_pos;
    if( right_pos < list->n ) {
        kd_item_t *right = list->items[right_pos];
        if( left > right ) {
            swap_pos = right_pos;
        }
    }

    kd_item_t **items = list->items;
    if( val > items[swap_pos]) {
        kd_item_t *tmp = items[pos];
        items[pos] = items[swap_pos];
        items[swap_pos] = tmp;

        kd_item_list_heapdown(list, swap_pos);
    }

    return 1;
}

int kd_item_list_min(kd_item_list_t *list, kd_item_t **item) {
    /* report minimum */
    *item = list->items[0];

    /* copy last element to root */
    list->items[0] = list->items[list->n-1];

    /* decrease size by one */
    --list->n;
    list->items[list->n] = NULL;

    /* new root item to correct position */
    kd_item_list_heapdown(list, 0);

    return 1;
}

int kd_item_list_add(kd_item_list_t *list, kd_item_t *item) {
    /* resize, if needed */
    if( list->n >= list->cap ) {
        int new_size = (list->cap*2)+1;
        kd_item_t **tmp = (kd_item_t**)malloc(new_size*sizeof(kd_item_t));
        if( !tmp ) return 0;
        int num = list->n;
        memcpy(tmp, list->items, num*sizeof(kd_item_t*));
        free(list->items);
        list->items = tmp;
        list->cap = new_size;
    }

    /* write item pointer into list */
    int pos = list->n++;
    list->items[pos] = item;

    /* restore heap property */
    kd_item_list_heapup(list, pos);

    return 1;
}

int kd_item_list_remove(kd_item_list_t *list, int idx) {

    if( idx<0 || idx >= list->n )
        return 0;

    /* set idx element to NULL and move it to the root */
    list->items[idx] = NULL;
    kd_item_list_heapup(list, idx);

    /* copy last element to the root */
    list->items[0] = list->items[list->n-1];
    list->items[list->n-1] = NULL;

    /* descrease number of items */
    --list->n;

    /* heap it back down */
    kd_item_list_heapdown(list, 0);

    return 1;
}

/* kd_node */

int kd_node_init(kd_node_t *node, int dimensions) {
    memset(node, '\0', sizeof(kd_node_t));
    node->dim = -1; /* mark as unspecified */
    kd_item_list_init(&node->items);
    aabb_init(&node->bb, dimensions);
    return 1;
}

int kd_node_free(kd_node_t *node) {
    if( node->left ) {
        kd_node_free(node->left);
        node->left = NULL;
    }
    if( node->right ) {
        kd_node_free(node->right);
        node->right = NULL;
    }
    kd_item_list_free(&node->items, 0);
    aabb_free(&node->bb);
    return 1;
}

static int kd_tree_print_node(kd_node_t *node, int depth) {
    int pad_n = depth * 4;
    char *padding = calloc(pad_n+1,sizeof(char));
    if( padding == NULL )
        return 0;
    memset(padding, ' ', pad_n*sizeof(char));

    printf("%sdim: %i; items: %i\n", padding, node->dim, node->items.n);
    if( node->left )
        kd_tree_print_node(node->left, depth+1);
    if( node->right )
        kd_tree_print_node(node->right, depth+1);

    free(padding); padding=NULL;

    return 1;
}

/* kd_tree */

static int kd_tree_free_node(kd_node_t *node) {
    if( !node )
        return 0;

    if( node->left ) {
        kd_tree_free_node(node->left);
        node->left = NULL;
    }
    if( node->right ) {
        kd_tree_free_node(node->right);
        node->right = NULL;
    }

    kd_node_free(node); node=NULL;
    return 1;
}

int kd_tree_init(kd_tree_t *tree, int dimensions) {
    memset(tree, '\0', sizeof(*tree));
    tree->root = calloc(1, sizeof(kd_node_t));
    kd_node_init(tree->root, dimensions);
    return 1;
}

int kd_tree_free(kd_tree_t *tree) {
    kd_tree_free_node(tree->root);
    tree->root = NULL;
    return 1;
}

int kd_tree_print(kd_tree_t *tree) {
    printf("K-D Tree:\n");
    return kd_tree_print_node(tree->root, 0);
}

static int kdtree_split_score(kd_item_list_t *items, int dim, double pos, double *score) {
    if( items == NULL )
        return 0;

    int num = items->n;
    int left_num=0, right_num=0, unsplit_num=0;
    for(int i=0; i<num; ++i) {
        double il, iu;
        vectNd_get(&items->items[i]->bb.lower, dim, &il);
        vectNd_get(&items->items[i]->bb.upper, dim, &iu);
        if( iu < pos-EPSILON )
            ++left_num;
        else if( il > pos+EPSILON )
            ++right_num;
        else
            ++unsplit_num;
    }
    *score = num - (abs(left_num-right_num) + 2*unsplit_num);
    return (left_num>0 && right_num>0)?1:0;
}

static int kd_tree_split_node(kd_node_t *node, int levels_remaining, int min_per_node) {

    int dimensions = node->bb.lower.n;

    if( levels_remaining == 0 || node->items.n < min_per_node ) {
        printf("giving up on line %d.\n", __LINE__);
        printf("  levels_remaining: %i\n", levels_remaining);
        printf("  node->items.n: %i\n", node->items.n);
        printf("  min_per_node: %i\n", min_per_node);
        return 1;
    }

    /* pick split point */
    int split_dim = node->dim;
    double split_pos = 0.0;
    double split_score = -DBL_MAX, best_score = -DBL_MAX;
    int found_split = 0;
    for(int cand_dim=0; cand_dim<dimensions; ++cand_dim) {
        for(int i=0; i<node->items.n; ++i) {
            double il, iu;
            vectNd_get(&node->items.items[i]->bb.lower, cand_dim, &il);
            vectNd_get(&node->items.items[i]->bb.upper, cand_dim, &iu);
            double cand_pos = il-2*EPSILON;
            if( kdtree_split_score(&node->items, cand_dim, cand_pos, &split_score)
                && split_score > best_score) {
                split_dim = cand_dim;
                split_pos = cand_pos;
                best_score = split_score;
                found_split = 1;
            }
            cand_pos = iu+2*EPSILON;
            if( kdtree_split_score(&node->items, cand_dim, cand_pos, &split_score)
                && split_score > best_score) {
                split_dim = cand_dim;
                split_pos = cand_pos;
                best_score = split_score;
                found_split = 1;
            }
        }
    }
    if( !found_split ) {
        return 1;
    }

    /* make child nodes */
    node->left = calloc(1, sizeof(kd_node_t));
    node->right = calloc(1, sizeof(kd_node_t));
    kd_node_init(node->left, dimensions);
    kd_node_init(node->right, dimensions);

    /* set bounding boxes for child nodes */
    aabb_copy(&node->left->bb, &node->bb);
    aabb_copy(&node->right->bb, &node->bb);
    vectNd_set(&node->left->bb.upper, split_dim, split_pos);
    vectNd_set(&node->right->bb.lower, split_dim, split_pos);

    /* assign items to child nodes */
    for(int i=0; i<node->items.n; ++i) {
        double il, iu;
        vectNd_get(&node->items.items[i]->bb.lower, split_dim, &il);
        vectNd_get(&node->items.items[i]->bb.upper, split_dim, &iu);
        if( iu < split_pos-EPSILON ) {
            kd_item_list_add(&node->left->items, node->items.items[i]);
            kd_item_list_remove(&node->items, i);
            --i;
        } else if( il > split_pos+EPSILON ) {
            kd_item_list_add(&node->right->items, node->items.items[i]);
            kd_item_list_remove(&node->items, i);
            --i;
        } else {
            kd_item_list_add(&node->left->items, node->items.items[i]);
            kd_item_list_add(&node->right->items, node->items.items[i]);
            kd_item_list_remove(&node->items, i);
            --i;
        }
    }

    /* recurse to children only if useful split occured */
    node->left->dim = (node->dim+1)%dimensions;
    node->right->dim = (node->dim+1)%dimensions;
    if( node->left->items.n > 0 && node->right->items.n > 0 ) {
        kd_tree_split_node(node->left, levels_remaining-1, min_per_node);
        kd_tree_split_node(node->right, levels_remaining-1, min_per_node);
    }

    return 0;
}

int kd_tree_build(kd_tree_t *tree, kd_item_list_t *items) {
    /* populate root node with all items */
    for(int i=0; i<items->n; ++i) {
        kd_item_list_add(&tree->root->items, items->items[i]);
        aabb_add(&tree->root->bb, &items->items[i]->bb);
    }

    /* recursively split root node */
    tree->root->dim = 0;
    //printf("building k-d tree with %d items.\n", items->n);
    int ret = kd_tree_split_node(tree->root, -1, -1);
    kd_tree_print(tree);
    return ret;
}

static int kd_node_intersect(kd_node_t *node, vectNd *o, vectNd *v, kd_item_list_t *items) {
    if( node==NULL )
        return 0;

    /* check for intersection with bb */
    if( !aabb_intersect(&node->bb, o, v) )
        return 0; /* return if not intersected */

    /* copy items into items list. */
    int num = node->items.n;
    for(int i=0; i<num; ++i)
        kd_item_list_add(items, node->items.items[i]);

    /* call for children */
    if( node->left!=NULL )
        num += kd_node_intersect(node->left, o, v, items);
    if( node->right!=NULL )
        num += kd_node_intersect(node->right, o, v, items);

    return num;
}

int kd_tree_intersect(kd_tree_t *tree, vectNd *o, vectNd *v, kd_item_list_t *items) {
    /* find all leaf nodes that ray o+x*v cross, and return items they contain */
    if( !tree ) {
        printf("tree is null.\n");
        return 0;
    }
    return kd_node_intersect(tree->root, o, v, items);
}

