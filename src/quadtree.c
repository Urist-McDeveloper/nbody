#include "quadtree.h"

#include <stdlib.h>

#include "err.h"
#include "body.h"
#include "v2.h"

/*
 * Dynamic array of Particles.
 */

typedef struct Particles {
    Particle *arr;  // dynamic array
    int cap, len;   // capacity and length
} Particles;

#define PARTICLES_DEFAULT_CAP 16

static void Particles_Init(Particles *ps) {
    ps->arr = malloc(PARTICLES_DEFAULT_CAP * sizeof(Particle));
    ASSERT(ps->arr != NULL);

    ps->cap = PARTICLES_DEFAULT_CAP;
    ps->len = 0;
}

static void Particles_DeInit(Particles *ps) {
    if (ps != NULL) free(ps->arr);
}

static void Particles_EnsureCap(Particles *ps, int cap) {
    if (ps->cap < cap) {
        do {
            ps->cap *= 2;
        } while (ps->cap < cap);

        ps->arr = realloc(ps->arr, ps->cap * sizeof(Particle));
        ASSERT(ps->arr != NULL);
    }
}

static void Particles_Push(Particles *ps, Particle p) {
    Particles_EnsureCap(ps, ps->len + 1);
    ps->arr[ps->len++] = p;
}

/*
 * NODE of QuadTree
 */

struct Node {
    Node *quad;         // a quad of Nodes
    V2 from, to, dims;  // (x0, y0), (x1, y1) and (width, height)
    V2 com;             // center of mass
    double mass;        // sum of members' mass
    double radius;      // sum of members' radius
    Particles members;  // cached Bodies in this Node
    bool is_leaf, end;  // whether this Node is a leaf and can it be split into quad
};

#define LEAF_MAX_BODIES 1       // how many members a leaf can have
#define NODE_END_WIDTH  1.0     // minimum width of non-leaf node
#define NODE_END_HEIGHT 1.0     // minimum height of non-leaf node

/* How far away from Node's COM a Body can be before it is considered sufficiently far away. */
#define NODE_COM_DIST_F 1.5

static Particle Node_ToParticle(const Node *n) {
    return (Particle) {
            .pos = n->com,
            .mass = n->mass,
            .radius = 0.0,
    };
}

static void Node_Init(Node *n, V2 from, V2 dims) {
    *n = (Node) {
            .quad = NULL,
            .from = from,
            .dims = dims,
            .to = V2_Add(from, dims),
            .com = V2_ZERO,
            .mass = 0.0,
            .radius = 0.0,
            .is_leaf = true,
            .end = (dims.x < NODE_END_WIDTH || dims.y < NODE_END_HEIGHT),
    };
    Particles_Init(&n->members);
}

static void Node_DeInit(Node *n) {
    if (n == NULL) return;
    if (n->quad != NULL) {
        for (int i = 0; i < 4; i++) {
            Node_DeInit(&n->quad[i]);
        }
        free(n->quad);
    }
    Particles_DeInit(&n->members);
}

static void Node_InitQuad(Node *quad, V2 parent_from, V2 parent_dims) {
    V2 dims = V2_Mul(parent_dims, 0.5);
    V2 from[] = {
            parent_from,                                // upper left quad
            V2_Add(parent_from, V2_From(dims.x, 0)),    // upper right quad
            V2_Add(parent_from, V2_From(0, dims.y)),    // lower left quad
            V2_Add(parent_from, dims),                  // lower right quad
    };

    for (int i = 0; i < 4; i++) {
        Node_Init(&quad[i], from[i], dims);
    }
}

static void Node_Update(Node *n, const Particles *ps) {
    // reset N
    V2 com = V2_ZERO;
    n->com = V2_ZERO;
    n->mass = 0.0;
    n->radius = 0.0;
    n->is_leaf = true;

    // reset members
    Particles *np = &n->members;
    np->len = 0;

    // check all particles
    for (int i = 0; i < ps->len; i++) {
        Particle p = ps->arr[i];
        V2 pp = ps->arr[i].pos;
        V2 nf = n->from;
        V2 nt = n->to;

        // if P is in this node
        if (pp.x >= nf.x && pp.x < nt.x && pp.y >= nf.y && pp.y < nt.y) {
            // add P to members
            Particles_Push(np, p);

            com = V2_Add(com, p.pos);
            n->mass += p.mass;
            n->radius += p.radius;
        }
    }

    if (np->len > 0) {
        n->com = V2_Mul(com, 1.0 / np->len);
    }

    if (!n->end && np->len > LEAF_MAX_BODIES) {
        // node should be separated into quad
        n->is_leaf = false;
        if (n->quad == NULL) {
            n->quad = malloc(4 * sizeof(Node));
            ASSERT(n->quad != NULL);

            Node_InitQuad(n->quad, n->from, n->dims);
        }

        #pragma omp parallel for firstprivate(n, np) default(none)
        for (int i = 0; i < 4; i++) {
            Node_Update(&n->quad[i], np);
        }
    }
}

static void Node_ApplyGrav(const Node *n, Body *b) {
    if (n->members.len == 0) return;
    if (n->members.len == 1) {
        Body_ApplyGrav(b, Node_ToParticle(n));
        return;
    }

    // minimal dx and dy
    V2 min = V2_Mul(n->dims, NODE_COM_DIST_F);

    double dx = fabs(b->p.pos.x - n->com.x);
    double dy = fabs(b->p.pos.y - n->com.y);

    if (dx > min.x && dy > min.y && (dx * dx + dy * dy) > (n->radius * n->radius)) {
        // B is sufficiently far away from N
        Body_ApplyGrav(b, Node_ToParticle(n));
    } else {
        // B is too close to N
        if (n->is_leaf) {
            // apply gravity of all members to B
            Particles ps = n->members;
            for (int i = 0; i < ps.len; i++) {
                Body_ApplyGrav(b, ps.arr[i]);
            }
        } else {
            // apply gravity of inner quad
            for (int i = 0; i < 4; i++) {
                Node_ApplyGrav(&n->quad[i], b);
            }
        }
    }
}

/*
 * QUADTREE
 */

struct QuadTree {
    Node quad[4];       // top-level quad
    V2 from, dims;      // (x0, y0) and (width, height)
    Particles members;  // cached Bodies in this quadtree
};

QuadTree *QuadTree_Create(V2 from, V2 to) {
    QuadTree *t = malloc(sizeof(*t));
    ASSERT(t != NULL);

    t->from = from;
    t->dims = V2_Sub(to, from);

    Node_InitQuad(t->quad, t->from, t->dims);
    Particles_Init(&t->members);

    return t;
}

void QuadTree_Destroy(QuadTree *t) {
    if (t == NULL) return;

    for (int i = 0; i < 4; i++) {
        Node_DeInit(&t->quad[i]);
    }
    Particles_DeInit(&t->members);
    free(t);
}

void QuadTree_Update(QuadTree *t, const Body *b, int n) {
    Particles *ps = &t->members;
    Particles_EnsureCap(ps, n);
    ps->len = 0;

    for (int i = 0; i < n; i++) {
        Particles_Push(ps, b[i].p);
    }

    #pragma omp parallel for firstprivate(t, ps) default(none)
    for (int i = 0; i < 4; i++) {
        Node_Update(&t->quad[i], ps);
    }
}

void QuadTree_ApplyGrav(const QuadTree *t, Body *b) {
    for (int i = 0; i < 4; i++) {
        Node_ApplyGrav(&t->quad[i], b);
    }
}

/*
 * DEBUG
 */

const Node *QuadTree_GetQuad(const QuadTree *t) {
    return (const Node *) (t->quad);
}

const Node *Node_GetQuad(const Node *n) {
    return n->is_leaf ? NULL : n->quad;
}

const Node *Node_FromQuad(const Node *quad, int n) {
    return &quad[n];
}

bool Node_IsEmpty(const Node *n) {
    return n->members.len == 0;
}

void Node_GetBox(const Node *n, V2 *from, V2 *to) {
    *from = n->from;
    *to = n->to;
}
