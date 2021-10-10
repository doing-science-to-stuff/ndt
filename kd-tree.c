#include "kd-tree.h"

/* aabb */

int aabb_init(aabb_t *bb, int dimensions) {
    vectNd_calloc(&bb->lower, dimensions);
    vectNd_calloc(&bb->upper, dimensions);
    return 1;
}

int aabb_free(aabb_t *bb) {
    vectNd_free(&bb->lower);
    vectNd_free(&bb->lower);
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
        if( fabs(vvMvo) < 1e6 )
            continue;   /* v is parallel to dimension i */
        double tl, tu;
        tl = (vl-vo) / vvMvo;
        tu = (vu-vo) / vvMvo;

        if( tl < -1e6 && tu < -1e6 ) {
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

/* kd_node */

int kd_node_init(kd_node_t *node, int dimensions) {
    memset(node, '\0', sizeof(kd_node_t));
    node->dim = -1; /* mark as unspecified */
    node->items = NULL;
    node->num_items = 0;
    vectNd_calloc(&node->bb.lower, dimensions);
    vectNd_calloc(&node->bb.upper, dimensions);
    return 1;
}

int kd_node_free(kd_node_t *node) {
    vectNd_free(&node->bb.lower);
    vectNd_free(&node->bb.upper);
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

static int kd_tree_split_node(kd_node_t *node, int levels_remaining, int min_per_node) {

    int dimensions = node->bb.lower.n;

    if( levels_remaining <= 0 || node->num_items < min_per_node )
        return 1;

    /* pick split point */
    int split_dim = 0;
    double split_pos = 0.0;
    /* pick dimension and point. */
    /* TODO */

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
    /* TODO */

    /* recurse */
    if( node->left->num_items > 0 && node->left->num_items < node->num_items )
        kd_tree_split_node(node->left, levels_remaining-1, min_per_node);
    if( node->right->num_items > 0 && node->right->num_items < node->num_items )
        kd_tree_split_node(node->right, levels_remaining-1, min_per_node);

    return 0;
}

int kd_tree_build(kd_tree_t *tree, kd_item_t *items, int n) {
    /* populate root node with all items */
    for(int i=0; i<n; ++i) {
        kd_item_copy(&tree->root->items[i], &items[i]);
        aabb_add(&tree->root->bb, &items[i].bb);
    }

    /* recursively split root node */
    return kd_tree_split_node(tree->root, -1, -1);
}

static int kd_node_intersect(kd_node_t *node, vectNd *o, vectNd *v, kd_item_t **items, int *n) {
    /* check for intersection with bb */
    if( !aabb_intersect(&node->bb, o, v) )
        return 0; /* return if not intersected */

    if( node->left==NULL && node->right==NULL ) {
        /* is a leaf, copy leaf items into items list. */
        /* TODO */

    } else {
        /* otherwise, call for both children */
        kd_node_intersect(node->left, o, v, items, n);
        kd_node_intersect(node->right, o, v, items, n);
    }

    return 0;
}

int kd_tree_intersect(kd_tree_t *tree, vectNd *o, vectNd *v, kd_item_t **items, int *n) {
    /* find all leaf nodes that ray o+x*v cross, and return items they contain */
    if( !tree )
        return 0;
    return kd_node_intersect(tree->root, o, v, items, n);
}

