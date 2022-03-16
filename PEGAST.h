#ifndef PCC_AST_INCLUDED
#define PCC_AST_INCLUDED
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <limits.h>
#include <stdio.h>

/* Create a new ASTNode for this rule, with the given key. */
#define INIT(name) __ = ASTNode_new(name)
/* Adds a key/value pair to the current node. */
#define SET(type, key, value)
/* Gets a key/value pair from the current node. */
#define GET(type, key) _GET(type, key, __)
#define _GET(type, key, node)
/* Clear the children of the current node.*/
#define CLEAR() ASTNode_clearChildren(__)


/**********************/
/* AST Implementation */
/**********************/

/* This AST Node implementation is not efficient,
   simplicity is what's important. The daic implementation
   will be optimized and will use a custom memory allocator.
   This is the dumbest example possible. */
struct ASTNode;
typedef struct ASTNode ASTNode;
struct ASTNode {
    char* name;
    ASTNode* parent;
    ASTNode** children;
    size_t num_children;
};

static inline ASTNode*
ASTNode_new(char* name) {
    ASTNode* node = (ASTNode*)malloc(sizeof(ASTNode));
    node->name = name;
    node->parent = NULL;
    node->children = NULL;  // realloc() as children are added
    node->num_children = 0;
    return node;
}

static inline void
ASTNode_addChild(ASTNode* parent, ASTNode* child) {
    if (!child) return;
    if (!parent) return;

    parent->num_children++;
    parent->children =
        (ASTNode**)realloc(parent->children, sizeof(ASTNode*) * parent->num_children);
    parent->children[parent->num_children - 1] = child;
}

static inline void
ASTNode_destroy(ASTNode* self) {
    for (size_t i = 0; i < self->num_children; i++) ASTNode_destroy(self->children[i]);
    free(self->children);
    free(self);
}

static inline void
ASTNode_clearChildren(ASTNode* self) {
    for (size_t i = 0; i < self->num_children; i++) {
        ASTNode_destroy(self->children[i]);
    }
    free(self->children);
    self->children = NULL;
}

static inline void
AST_print_helper(ASTNode* current, size_t depth) {
    /* Print current node. */
    for (size_t i = 0; i < depth; i++) printf("  ");
    puts(current->name);

    /* Print children. */
    if (!current->children)
        for (size_t i = 0; i < current->num_children; i++)
            AST_print_helper(current->children[i], depth + 1);
}

static inline void
AST_print(ASTNode* root) {
    AST_print_helper(root, 0);
}


#endif /* PCC_AST_INCLUDED */
