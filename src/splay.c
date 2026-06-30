/* ===========================================================
 * splay.c — índice autoajustável pageno -> frame_id (núcleo de ED)
 * ===========================================================
 * A árvore guarda APENAS o índice (qual frame contém cada página);
 * os bytes vivem no pool de frames do page_cache. Todo acesso com
 * acerto roda o nó até a raiz (splay), de modo que páginas quentes
 * migram para o topo. O despejo escolhe a folha mais profunda — por
 * construção, a página mais "fria".
 * =========================================================== */
#include "splay.h"

#include <stdlib.h>

typedef struct snode {
    uint64_t      pageno;
    int           frame_id;
    struct snode *left;
    struct snode *right;
    struct snode *parent;
} snode;

struct splay {
    snode *root;
};

splay_t *splay_create(void) {
    return calloc(1, sizeof(splay_t));
}

static void free_subtree(snode *n) {
    if (!n) return;
    free_subtree(n->left);
    free_subtree(n->right);
    free(n);
}

void splay_destroy(splay_t *t) {
    if (!t) return;
    free_subtree(t->root);
    free(t);
}

/* Sobe x um nível, rotacionando-o sobre o pai. Atualiza a raiz da
 * árvore quando x chega ao topo. */
static void rotate(splay_t *t, snode *x) {
    snode *p = x->parent;
    snode *g = p->parent;
    if (p->left == x) {                 /* rotação à direita */
        p->left = x->right;
        if (x->right) x->right->parent = p;
        x->right = p;
    } else {                            /* rotação à esquerda */
        p->right = x->left;
        if (x->left) x->left->parent = p;
        x->left = p;
    }
    p->parent = x;
    x->parent = g;
    if (g) {
        if (g->left == p) g->left = x;
        else              g->right = x;
    } else {
        t->root = x;
    }
}

/* Leva x até a raiz com os casos zig / zig-zig / zig-zag. */
static void splay_node(splay_t *t, snode *x) {
    while (x->parent) {
        snode *p = x->parent;
        snode *g = p->parent;
        if (!g) {
            rotate(t, x);                           /* zig */
        } else if ((g->left == p) == (p->left == x)) {
            rotate(t, p);                           /* zig-zig */
            rotate(t, x);
        } else {
            rotate(t, x);                           /* zig-zag */
            rotate(t, x);
        }
    }
    t->root = x;
}

int splay_lookup(splay_t *t, uint64_t pageno, int *out_depth) {
    if (!t) return SPLAY_NOT_FOUND;
    snode *cur   = t->root;
    int    depth = 0;
    while (cur) {
        if (pageno == cur->pageno) {
            if (out_depth) *out_depth = depth;     /* profundidade ANTES do splay */
            int fid = cur->frame_id;
            splay_node(t, cur);
            return fid;
        }
        cur = (pageno < cur->pageno) ? cur->left : cur->right;
        depth++;
    }
    if (out_depth) *out_depth = -1;
    return SPLAY_NOT_FOUND;
}

void splay_insert(splay_t *t, uint64_t pageno, int frame_id) {
    if (!t) return;
    if (!t->root) {
        snode *n = calloc(1, sizeof(snode));
        if (!n) return;
        n->pageno = pageno;
        n->frame_id = frame_id;
        t->root = n;
        return;
    }
    snode *cur    = t->root;
    snode *parent = NULL;
    while (cur) {
        parent = cur;
        if (pageno == cur->pageno) {               /* já existe: atualiza */
            cur->frame_id = frame_id;
            splay_node(t, cur);
            return;
        }
        cur = (pageno < cur->pageno) ? cur->left : cur->right;
    }
    snode *n = calloc(1, sizeof(snode));
    if (!n) return;
    n->pageno   = pageno;
    n->frame_id = frame_id;
    n->parent   = parent;
    if (pageno < parent->pageno) parent->left = n;
    else                         parent->right = n;
    splay_node(t, n);
}

static void find_deepest(snode *n, int depth, int *best, snode **out) {
    if (!n) return;
    if (depth > *best) { *best = depth; *out = n; }
    find_deepest(n->left,  depth + 1, best, out);
    find_deepest(n->right, depth + 1, best, out);
}

uint64_t splay_evict_victim(splay_t *t, int *out_frame_id) {
    if (!t || !t->root) {
        if (out_frame_id) *out_frame_id = -1;
        return 0;
    }
    int    best   = -1;
    snode *victim = NULL;
    find_deepest(t->root, 0, &best, &victim);      /* nó mais profundo = folha fria */

    uint64_t pageno = victim->pageno;
    if (out_frame_id) *out_frame_id = victim->frame_id;

    snode *p = victim->parent;                     /* é folha: basta desligar do pai */
    if (!p)                       t->root = NULL;
    else if (p->left == victim)   p->left = NULL;
    else                          p->right = NULL;
    free(victim);
    return pageno;
}
