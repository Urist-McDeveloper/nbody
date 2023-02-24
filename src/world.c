#include "world.h"

#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>

/* How velocity changes along the axis of bounce. */
#define BOUNCE_F    (-0.5)

/* How velocity changes along the other axis. */
#define FRICTION_F  0.75

/* Call perror(MSG) and abort if COND is false. */
#define ASSERT(COND, MSG) do { if (!(COND)) { perror(MSG); abort(); } } while (0)

/*
 * NODES
 */

typedef struct Node Node;

struct Node {
    Node *inner;    // nested nodes (always 4)
    V2 from;        // (x0, y0)
    V2 to;          // (x1, y1)
    V2 com;         // center of mass
    double mass;    // total mass
    int *idx;       // cached dynamic array of body indices
    int idx_cap;    // number of allocated elements of idx
    int idx_len;    // number of used elements of idx
    bool leaf;      // whether node is a leaf
    bool sgmt;      // whether node can have inner
};

typedef struct Root {
    Node nodes[4];  // nested nodes
    int bodies;     // number of expected bodies
    int idx[0];     // indices for Node_updateLeaf
} Root;

#define DEFAULT_IDX_CAP 16

#define NODE_MAX_BODIES 4
#define NODE_MIN_HEIGHT 4.0
#define NODE_MIN_WIDTH  4.0

static void Node_initLeaf(Node *node, V2 from, V2 to) {
    int *idx = malloc(DEFAULT_IDX_CAP * sizeof(*idx));
    ASSERT(idx != NULL, "Failed to malloc node->idx");

    V2 dims = V2_sub(to, from);
    *node = (Node) {
            .inner = NULL,
            .from = from,
            .to = to,
            .com = V2_ZERO,
            .mass = 0.0,
            .idx = idx,
            .idx_cap = DEFAULT_IDX_CAP,
            .idx_len = 0,
            .leaf = true,
            .sgmt = dims.x > NODE_MIN_WIDTH && dims.y > NODE_MIN_HEIGHT,
    };
}

static void Node_initInner(Node *inner, V2 parent_from, V2 parent_to) {
    V2 dims = V2_sub(parent_to, parent_from);
    V2 half_w = V2_from(0.5 * dims.x, 0.0);
    V2 half_h = V2_from(0.0, 0.5 * dims.y);

    V2 ul = parent_from;
    V2 um = V2_add(ul, half_w);

    V2 ml = V2_add(ul, half_h);
    V2 mm = V2_add(ml, half_w);
    V2 mr = V2_add(mm, half_w);

    V2 lm = V2_add(mm, half_h);
    V2 lr = V2_add(lm, half_w);

    Node_initLeaf(&inner[0], ul, mm);
    Node_initLeaf(&inner[1], um, mr);
    Node_initLeaf(&inner[2], ml, lm);
    Node_initLeaf(&inner[3], mm, lr);
}

static void Node_destroyLeaf(Node *node) {
    if (node == NULL) return;

    if (node->inner != NULL) {
        for (int i = 0; i < 4; i++) {
            Node_destroyLeaf(&node->inner[i]);
        }
    }
    free(node->idx);
    free(node);
}

static Root *Node_createRoot(int width, int height, int bodies) {
    Root *root = malloc(sizeof(Root) + bodies * sizeof(int));
    ASSERT(root != NULL, "Failed to malloc root");

    root->bodies = bodies;
    for (int i = 0; i < bodies; i++) {
        root->idx[i] = i;
    }

    Node_initInner(root->nodes, V2_ZERO, V2_from(width, height));
    return root;
}

static void Node_destroyRoot(Root *root) {
    if (root != NULL) {
        for (int i = 0; i < 4; i++) {
            Node_destroyLeaf(&root->nodes[i]);
        }
        free(root);
    }
}

static void Node_reset(Node *node) {
    node->com = V2_ZERO;
    node->mass = 0;
    node->idx_len = 0;
    node->leaf = true;
}

static void Node_ensureIdxCap(Node *node, int cap) {
    if (node->idx_cap < cap) {
        while (node->idx_cap < cap) {
            node->idx_cap *= 2;
        }
        node->idx = realloc(node->idx, node->idx_cap * sizeof(*(node->idx)));
        ASSERT(node->idx != NULL, "Failed to realloc node->idx");
    }
}

static void Node_addIdx(Node *node, int i) {
    Node_ensureIdxCap(node, node->idx_len + 1);
    node->idx[node->idx_len++] = i;
}

static void Node_updateLeaf(Node *node, const Body *bodies, const int *idx, int idx_len) {
    // prepare for update
    Node_reset(node);

    // accumulators
    double mass = 0;
    V2 com = V2_ZERO;

    for (int i = 0; i < idx_len; i++) {
        const Body *b = &bodies[idx[i]];

        V2 bp = b->pos;
        V2 nf = node->from;
        V2 nt = node->to;

        if (bp.x >= nf.x && bp.x < nt.x && bp.y >= nf.y && bp.y < nt.y) {
            Node_addIdx(node, idx[i]);
            mass += b->m;
            com = V2_add(com, b->pos);
        }
    }

    if (node->idx_len > 0) {
        // update node
        node->com = V2_scale(com, 1.0 / node->idx_len);
        node->mass = mass / node->idx_len;

        if (node->sgmt && node->idx_len > NODE_MAX_BODIES) {
            // update leafs
            if (node->inner == NULL) {
                // allocate leafs
                node->inner = malloc(4 * sizeof(Node));
                ASSERT(node->inner != NULL, "Failed to malloc node->inner");

                Node_initInner(node->inner, node->from, node->to);
            }

            #pragma omp parallel for shared(node, bodies) default(none)
            for (int i = 0; i < 4; i++) {
                Node_updateLeaf(&node->inner[i], bodies, node->idx, node->idx_len);
            }
            node->leaf = false;
        }
    }
}

static void Node_updateRoot(Root *root, const Body *bodies) {
    #pragma omp parallel for shared(root, bodies) default(none)
    for (int i = 0; i < 4; i++) {
        Node_updateLeaf(&root->nodes[i], bodies, root->idx, root->bodies);
    }
}

#define MIN_DIST_F  8.0

static void Node_checkLeaf(Node *node, Body *target, const Body *bodies) {
    if (node->idx_len == 0) return;
    if (node->leaf) {
        for (int i = 0; i < node->idx_len; i++) {
            const Body *other = &bodies[node->idx[i]];
            Body_applyGrav(target, other);
        }
    } else {
        V2 radv = V2_sub(target->pos, node->com);
        V2 dims = V2_sub(node->to, node->from);
        double maxd = dims.x > dims.y ? dims.x : dims.y;

        double dist_f = V2_sqLength(radv) / maxd;
        if (dist_f > MIN_DIST_F) {
            Body_applyGravV2(target, node->com, node->mass);
        } else {
            #pragma omp parallel for shared(node, target, bodies) default(none)
            for (int i = 0; i < 4; i++) {
                Node_checkLeaf(&node->inner[i], target, bodies);
            }
        }
    }
}

static void Node_checkRoot(Root *root, Body *target, const Body *bodies) {
    #pragma omp parallel for shared(root, target, bodies) default(none)
    for (int i = 0; i < 4; i++) {
        Node_checkLeaf(&root->nodes[i], target, bodies);
    }
}

/*
 * WORLD
 */

struct World {
    Body *bodies;
    Root *root;
    int size;
    int width;
    int height;
};

World *World_create(int size, int width, int height) {
    World *world = malloc(sizeof(*world));
    Body *bodies = malloc(sizeof(*bodies) * size);
    Root *root = Node_createRoot(width, height, size);

    ASSERT(world && bodies && root, "Failed to malloc World");

    for (int i = 0; i < size; i++) {
        Body_init(&bodies[i], width, height);
    }

    *world = (World) {
            .bodies = bodies,
            .root = root,
            .size = size,
            .width = width,
            .height = height,
    };
    return world;
}

void World_destroy(World *world) {
    if (world != NULL) {
        Node_destroyRoot(world->root);
        free(world->bodies);
        free(world);
    }
}

void World_getBodies(World *world, Body **bodies, int *size) {
    *bodies = world->bodies;
    *size = world->size;
}

void World_update(World *world, const double t) {
    Body *bodies = world->bodies;
    Root *root = world->root;
    int size = world->size;

    Node_updateRoot(root, bodies);

    #pragma omp parallel for shared(bodies, root) firstprivate(size) default(none)
    for (int i = 0; i < size; i++) {
//        for (int j = 0; j < size; j++) {
//            if (i == j) continue;
//            Body_applyGrav(&bodies[i], &bodies[j]);
//        }
        Node_checkRoot(root, &bodies[i], bodies);
    }

    int width = world->width;
    int height = world->height;

    #pragma omp parallel for shared(bodies) firstprivate(size, t, width, height) default(none)
    for (int i = 0; i < size; i++) {
        Body *b = &bodies[i];
        Body_move(b, t);

        double min_x = b->r;
        double min_y = b->r;
        double max_x = width - min_x;
        double max_y = height - min_y;

        if (b->pos.x < min_x || b->pos.x > max_x) {
            b->pos.x = (b->pos.x < min_x) ? min_x : max_x;
            b->vel.x *= BOUNCE_F;
            b->vel.y *= FRICTION_F;
        }
        if (b->pos.y < min_y || b->pos.y > max_y) {
            b->pos.y = (b->pos.y < min_y) ? min_y : max_y;
            b->vel.y *= BOUNCE_F;
            b->vel.x *= FRICTION_F;
        }
    }
}
