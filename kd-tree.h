#ifndef KD_TREE_H
#define KD_TREE_H

#include "vectNd.h"

/* aabb */

typedef struct aabb {
    vectNd lower, upper;
} aabb_t;

int aabb_init(aabb_t *bb, int dimensions);
int aabb_free(aabb_t *bb);
int aabb_copy(aabb_t *dst, aabb_t *src);
int aabb_add(aabb_t *dst, aabb_t *src);

/* kd_item */

typedef struct kd_item {
    aabb_t bb;
    void *ptr;
} kd_item_t;

int kd_item_init(kd_item_t *item, int dimensions);
int kd_item_free(kd_item_t *item);
int kd_item_copy(kd_item_t *dst, kd_item_t *src);

/* kd_node */

typedef struct kd_node {
    aabb_t bb;
    struct kd_node *left, *right;
    int dim;
    double boundary;
    kd_item_t *items;
    int num_items;
} kd_node_t;

int kd_node_init(kd_node_t *node, int dimensions);
int kd_node_free(kd_node_t *node);

/* kd_tree */

typedef struct kd_tree {
    kd_node_t *root;
} kd_tree_t;

int kd_tree_init(kd_tree_t *tree, int dimensions);
int kd_tree_free(kd_tree_t *tree);
int kd_tree_build(kd_tree_t *tree, kd_item_t *items, int n);
int kd_tree_intersect(kd_tree_t *tree, vectNd *o, vectNd *v, kd_item_t **items, int *n);

#endif /* KD_TREE_H */
