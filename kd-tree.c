/*
 * kd-tree.c
 * ndt: n-dimensional tracer
 *
 * Copyright (c) 2021 Bryan Franklin. All rights reserved.
 */
#include <float.h>
#include <stdio.h>

#ifndef WITHOUT_KDTREE
#include "kd-tree.h"
#include "object.h"

//typedef struct object_t object;

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

int aabb_print(aabb_t *bb) {
    vectNd_print(&bb->lower, "bb lower");
    vectNd_print(&bb->upper, "bb upper");
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
            vectNd_set(&bb->lower, i, bbl-EPSILON);
        }
        if( pv > bbu ) {
            bbu = pv;
            vectNd_set(&bb->upper, i, bbu+EPSILON);
        }
    }
    return 1;
}

/* test if ray o+v*t intersects bounding box bb */
static int aabb_intersect(aabb_t *bb, vectNd *o, vectNd *v, double *tl_ptr, double *tu_ptr) {
    int dimensions = v->n;

    /* find smallest and largest values of t where o+v*t is inside bb */
    double tl = -DBL_MAX, tu = DBL_MAX;
    double v_i, o_i;
    double bbl_i, bbu_i;
    for(int i=0; i<dimensions; ++i) {
        vectNd_get(v, i, &v_i);
        vectNd_get(o, i, &o_i);
        vectNd_get(&bb->lower, i, &bbl_i);
        vectNd_get(&bb->upper, i, &bbu_i);

        if( fabs(v_i) < EPSILON2 ) {
            continue;
        }

        double tl_i, tu_i;
        /* TODO: do this subtraction as vector ops outside of loop. */
        tl_i = (bbl_i - o_i) / v_i;
        tu_i = (bbu_i - o_i) / v_i;
        if( tl_i > tu_i ) {
            /* if upper bound is closer to o than lower bound,
             * swap tl_i and tu_i. */
            double tmp = tl_i;
            tl_i = tu_i;
            tu_i = tmp;
        }

        /* get minimum upper bound and maximum lower bound */
        if( tl_i > tl )    tl = tl_i;
        if( tu_i < tu )    tu = tu_i;
        if( tu < -EPSILON )
            return 0;   /* entire intersection is behind o wrt v */
    }
    tl -= EPSILON;
    tu += EPSILON;

    /* record results */
    if( tl_ptr!=NULL ) *tl_ptr = tl;
    if( tu_ptr!=NULL ) *tu_ptr = tu;

    return (tu >= -EPSILON) && (tl <= tu);
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
    dst->id = src->id;
    dst->obj_ptr = src->obj_ptr;
    return 1;
}

/* kd_item_list */

int kd_item_list_init(kd_item_list_t *list) {
    memset(list, '\0', sizeof(kd_item_list_t));
    list->cap = 1;
    list->items = (kd_item_t**)malloc(list->cap*sizeof(kd_item_t));
    return (list->items!=NULL);
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

    return 1;
}

int kd_item_list_remove(kd_item_list_t *list, int idx) {

    if( idx<0 || idx >= list->n )
        return 0;

    /* move last element to deleted position */
    list->items[idx] = list->items[list->n-1];
    list->items[list->n-1] = NULL;

    /* descrease number of items */
    --list->n;

    return 1;
}

/* kd_node */

int kd_node_init(kd_node_t *node) {
    memset(node, '\0', sizeof(kd_node_t));
    node->dim = -1; /* mark as unspecified */
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
    free(node->obj_ids); node->obj_ids=NULL;
    free(node->objs); node->objs=NULL;
    free(node); node=NULL;
    return 1;
}

static int kd_tree_print_node(kd_node_t *node, int depth) {
    int pad_n = depth * 4;
    char *padding = calloc(pad_n+1,sizeof(char));
    if( padding == NULL )
        return 0;
    memset(padding, ' ', pad_n*sizeof(char));

    printf("%sdim: %i; boundary: %g; items: %i\n", padding, node->dim, node->boundary, node->num);
    if( node->left )
        kd_tree_print_node(node->left, depth+1);
    if( node->right )
        kd_tree_print_node(node->right, depth+1);

    free(padding); padding=NULL;

    return 1;
}

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

/* kd_tree */

int kd_tree_init(kd_tree_t *tree, int dimensions) {
    memset(tree, '\0', sizeof(*tree));
    tree->root = calloc(1, sizeof(kd_node_t));
    kd_node_init(tree->root);
    aabb_init(&tree->bb, dimensions);
    return 1;
}

int kd_tree_free(kd_tree_t *tree) {
    kd_tree_free_node(tree->root);  tree->root=NULL;
    aabb_free(&tree->bb);
    if( tree->obj_ptrs!=NULL ) {
        free(tree->obj_ptrs); tree->obj_ptrs = NULL;
    }
    if( tree->inf_obj_ptrs != NULL ) {
        free(tree->inf_obj_ptrs); tree->inf_obj_ptrs = NULL;
    }
    if( tree->ids != NULL ) {
        free(tree->ids); tree->ids = NULL;
    }
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

static int kd_tree_split_node(kd_node_t *node, kd_item_list_t *items, int levels_remaining, int min_per_node, int dimensions) {

    /* pick split point */
    int found_split = 0;
    int split_dim = node->dim;
    double split_pos = 0.0;
    double split_score = -DBL_MAX, best_score = -DBL_MAX;
    if( levels_remaining != 0 && items->n >= min_per_node ) {
        for(int cand_dim=0; cand_dim<dimensions; ++cand_dim) {
            for(int i=0; i<items->n; ++i) {
                double il, iu;
                vectNd_get(&items->items[i]->bb.lower, cand_dim, &il);
                vectNd_get(&items->items[i]->bb.upper, cand_dim, &iu);
                double cand_pos = il-2*EPSILON;
                if( kdtree_split_score(items, cand_dim, cand_pos, &split_score)
                        && split_score > best_score) {
                    split_dim = cand_dim;
                    split_pos = cand_pos;
                    best_score = split_score;
                    found_split = 1;
                }
                cand_pos = iu+2*EPSILON;
                if( kdtree_split_score(items, cand_dim, cand_pos, &split_score)
                        && split_score > best_score) {
                    split_dim = cand_dim;
                    split_pos = cand_pos;
                    best_score = split_score;
                    found_split = 1;
                }
            }
        }
    } else {
        printf("giving up on line %d.\n", __LINE__);
        printf("  levels_remaining: %i\n", levels_remaining);
        printf("  node->items.n: %i\n", node->num);
        printf("  min_per_node: %i\n", min_per_node);
        found_split = 0;
    }
    if( !found_split ) {
        /* record object ids in leaf nodes */
        int num = items->n;
        node->num = num;
        node->dim = -1;
        node->boundary = 0.0;
        node->obj_ids = calloc(num, sizeof(int*));
        node->objs = calloc(num, sizeof(void*));
        for(int i=0; i<num; ++i) {
            node->obj_ids[i] = items->items[i]->id;
            node->objs[i] = items->items[i]->obj_ptr;
        }
        node->left = NULL;
        node->right = NULL;
        return 1;
    }

    /* assign split criteria */
    node->dim = split_dim;
    node->boundary = split_pos;

    /* make child nodes */
    node->left = calloc(1, sizeof(kd_node_t));
    node->right = calloc(1, sizeof(kd_node_t));
    kd_node_init(node->left);
    kd_node_init(node->right);

    /* assign items to child nodes */
    kd_item_list_t left_items, right_items;
    kd_item_list_init(&left_items);
    kd_item_list_init(&right_items);
    for(int i=0; i<items->n; ++i) {
        double radius = ((object*)items->items[i]->obj_ptr)->bounds.radius;
        if( radius < 0.0 ) {
            printf("Infinite object detected in k-d tree!");
            continue;
        }

        double il, iu;
        vectNd_get(&items->items[i]->bb.lower, split_dim, &il);
        vectNd_get(&items->items[i]->bb.upper, split_dim, &iu);
        if( iu < split_pos-EPSILON ) {
            kd_item_list_add(&left_items, items->items[i]);
        } else if( il > split_pos+EPSILON ) {
            kd_item_list_add(&right_items, items->items[i]);
        } else {
            kd_item_list_add(&left_items, items->items[i]);
            kd_item_list_add(&right_items, items->items[i]);
        }
    }

    /* recurse to children only if useful split occured */
    node->left->dim = (node->dim+1)%dimensions;
    node->right->dim = (node->dim+1)%dimensions;
    if( left_items.n > 0 && right_items.n > 0 ) {
        /* try to recursively split new leaves */
        kd_tree_split_node(node->left, &left_items, levels_remaining-1, min_per_node, dimensions);
        kd_tree_split_node(node->right, &right_items, levels_remaining-1, min_per_node, dimensions);
    }
    kd_item_list_free(&left_items, 0);
    kd_item_list_free(&right_items, 0);

    /* TODO: check both left and right children and move any ids that are in
     * both up to this node. */

    return 0;
}

int kd_tree_build(kd_tree_t *tree, kd_item_list_t *items) {
    /* populate root node with all items */
    if( tree == NULL )
        return 0;
    if( tree->root == NULL )
        tree->root = calloc(1, sizeof(kd_node_t));
    kd_item_list_t root_items;
    kd_item_list_init(&root_items);
    /* TODO: count number of each type first */
    tree->root->objs = calloc(items->n, sizeof(void*));
    tree->inf_obj_ptrs = calloc(items->n, sizeof(void*));
    tree->obj_num = 0;
    tree->inf_obj_num = 0;
    for(int i=0; i<items->n; ++i) {
        /* assign id */
        kd_item_t *item = items->items[i];
        item->id = i;

        if( ((object*)item->obj_ptr)->bounds.radius >= 0.0 ) {
            /* assign to root node */
            tree->root->objs[tree->obj_num++] = item->obj_ptr;
            kd_item_list_add(&root_items, item);

            /* adjust top-level AABB */
            aabb_add(&tree->bb, &item->bb);
        } else {
            /* add infinite object to special list */
            tree->inf_obj_ptrs[tree->inf_obj_num++] = item->obj_ptr;
        }
    }
    aabb_print(&tree->bb);

    printf("%i finite objects, %i infinite objects.\n", tree->obj_num, tree->inf_obj_num);

    /* recursively split root node */
    tree->root->dim = 0;
    printf("building k-d tree with %d items.\n", items->n);
    tree->obj_num = items->n;
    int ret = 1;
    int dimensions = tree->bb.lower.n;
    ret = kd_tree_split_node(tree->root, &root_items, -1, -1, dimensions);
    //ret = kd_tree_split_node(tree->root, items, 1, 1, dimensions);
    kd_item_list_free(&root_items, 0);
    //kd_tree_print(tree);
    return ret;
}

#define INV_EPSILON (1.0/(EPSILON))
#define INV_EPSILON2 (1.0/(EPSILON2))

static int kd_node_intersect(kd_node_t *node, vectNd *o, vectNd *v, vectNd *v_inv, vectNd *hit, vectNd *hit_normal, char *obj_mask, object **ptr, double *t_ptr, double dist_limit, double tl, double tu) {
    if( node==NULL )
        return 0;

    /* TODO: use a local variable for t_ptr for most comparisons and recursive
     * calls */

    #if 1
    if( tu < 0.0 )
        return 0;
    #endif /* 1 */

    int num = node->num;
        int node_dim = node->dim;
    int ret = 0;
    if( num > 0 ) {
        double t;
        int dim = o->n;
        object *obj_ptr;
        vectNd lhit, lhit_normal;
        vectNd_alloc(&lhit, dim);
        vectNd_alloc(&lhit_normal, dim);
        //printf("node %p: %i objects (tl, tu: %g, %g).\n", (void*)node, node->num, tl, tu);
        ret = trace(o, v, (object**)node->objs, node->obj_ids, node->num, obj_mask, &lhit, &lhit_normal, &obj_ptr, &t, dist_limit);
        if( ret && t<*t_ptr ) {
            *t_ptr = t;
            *ptr = obj_ptr;
            vectNd_copy(hit, &lhit);
            vectNd_copy(hit_normal, &lhit_normal);
        }
        vectNd_free(&lhit);
        vectNd_free(&lhit_normal);

        if( node_dim < 0 ) {
            /* is a leaf */
            return ret;
        }
    }

    /* adjust for direction of v in split dimension */
    double node_boundary = node->boundary;
    double v_inv_i, o_i;
    kd_node_t *near = node->left, *far = node->right;
    vectNd_get(v_inv, node_dim, &v_inv_i);
    vectNd_get(o, node_dim, &o_i);
    if( v_inv_i < EPSILON2 ) {
        kd_node_t *tmp = near;
        near = far;
        far = tmp;
    }

    /* check for intersection with bb */
    if( -INV_EPSILON2 <= v_inv_i && v_inv_i <= INV_EPSILON2 ) {
        /* compute intersection with dividing plane */
        /* find t where o+v*t crossed an axis-aligned plane x_dim=pos. */
        double tp = (node_boundary - o_i) * v_inv_i;

        /* TODO: only recurse to sub-nodes where lower t limit is reachable */
        /* use t values to identify children to recurse to */
        if( tu < tp-EPSILON && *t_ptr > tl ) {
            /* recurse to near sub-AABB with tl and tu */
            ret |= kd_node_intersect(near, o, v, v_inv, hit, hit_normal, obj_mask, ptr, t_ptr, dist_limit, tl, tu);
        } else if( tl > tp+EPSILON && *t_ptr > tl ) {
            /* recurse to far sub-AABB with tl and tu */
            ret |= kd_node_intersect(far, o, v, v_inv, hit, hit_normal, obj_mask, ptr, t_ptr, dist_limit, tl, tu);
        } else {
            /* ray crosses dividing plane inside AABB,
             * recurse both directions, using tl,tp and tp,tu */
            if( *t_ptr > tl )
                ret |= kd_node_intersect(near, o, v, v_inv, hit, hit_normal, obj_mask, ptr, t_ptr, dist_limit, tl, tp+EPSILON);
            if( *t_ptr > tp )
                ret |= kd_node_intersect(far, o, v, v_inv, hit, hit_normal, obj_mask, ptr, t_ptr, dist_limit, tp-EPSILON, tu);
        }
    } else {
        /* plane is parallel to v, compare o_dim and pos */
        if( o_i < node_boundary+EPSILON && *t_ptr > tl ) {
            /* recurse left with tl and tu */
            ret |= kd_node_intersect(near, o, v, v_inv, hit, hit_normal, obj_mask, ptr, t_ptr, dist_limit, tl, tu);
        }
        if( o_i > node_boundary-EPSILON && *t_ptr > tl ) {
            /* recurse right with tl and tu */
            ret |= kd_node_intersect(far, o, v, v_inv, hit, hit_normal, obj_mask, ptr, t_ptr, dist_limit, tl, tu);
        }
    }

    return ret;
}

int kd_tree_intersect(kd_tree_t *tree, vectNd *o, vectNd *v, vectNd *hit, vectNd *hit_normal, void **ptr, double dist_limit) {
    /* find all leaf nodes that ray o+x*v cross, and return items they contain */
    if( !tree ) {
        printf("tree is null.\n");
        return 0;
    }
    int ret = 0;
    int dimensions = v->n;
    vectNd_unitize(v);
    vectNd v_inv;
    vectNd_alloc(&v_inv, dimensions);
    for(int i=0; i<dimensions; ++i) {
        double v_i, v_inv_i;
        vectNd_get(v, i, &v_i);
        if( v_i < EPSILON2 && v_i >= 0.0 )
            v_inv_i = INV_EPSILON2;
        else if( v_i > -EPSILON2 && v_i <= 0.0 )
            v_inv_i = -INV_EPSILON2;
        else
            v_inv_i = 1.0/v_i;
        vectNd_set(&v_inv, i, v_inv_i);
    }

    /* check infinite objects */
    double t = DBL_MAX;
    ret = trace(o, v, (object**)tree->inf_obj_ptrs, NULL, tree->inf_obj_num, NULL, hit, hit_normal, (object**)ptr, &t, dist_limit);

    /* TODO: aabb_intersect should be faster using v_inv */
    double tl, tu;
    if( aabb_intersect(&tree->bb, o, v, &tl, &tu) ) {
        double lt = DBL_MAX;
        char *obj_mask = calloc(tree->obj_num, sizeof(char));

        object *obj_ptr=NULL;
        vectNd lhit, lhit_normal;
        vectNd_alloc(&lhit, dimensions);
        vectNd_alloc(&lhit_normal, dimensions);

        int lret = kd_node_intersect(tree->root, o, v, &v_inv, &lhit, &lhit_normal, obj_mask, &obj_ptr, &lt, dist_limit, tl, tu);

        if( lret ) {
            /* check if intersection with finite objects is closer than
             * intersection with infinite objects. */
            if( !ret || (lt > EPSILON && lt+EPSILON < t)) {
                vectNd_copy(hit, &lhit);
                vectNd_copy(hit_normal, &lhit_normal);
                *ptr = obj_ptr;
                ret |= lret;
            }
        }
        vectNd_free(&lhit);
        vectNd_free(&lhit_normal);
        free(obj_mask); obj_mask=NULL;
    }
    vectNd_free(&v_inv);
    return ret;
}
#endif /* !WITHOUT_KDTREE */
