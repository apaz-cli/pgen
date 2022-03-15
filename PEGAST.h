#ifndef PCC_AST_INCLUDED
#define PCC_AST_INCLUDED
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <limits.h>

/* Rounds size up to the given stride for memory alignment purposes */
#define ALIGN(size) PCC_ALIGN(size, _Alignof(max_align_t))
#define PCC_ALIGN(size, stride) (((size + stride - 1) / stride) * stride)


/**********************/
/* AST Implementation */
/**********************/

/* This AST Node implementation is not efficient,
   the API is what's important. The daic implementation
   will use a custom memory allocator and handle
   fragmentation. This is the dumbest example possible. */
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
AST_print_helper(ASTNode* current, size_t depth) {
    for (size_t i = 0; i < depth; i++) printf("  ");
    puts(current->name);
}

static inline void
AST_print(ASTNode* root) {
    AST_print_helper(root, 0);
}

/* Adds a key/value pair to the current rule's node's map. */
#define SET(type, key, value)
/* Gets a key/value pair from the current rule's node's map. */
#define GET(type, key)
/* Adds a child to the current rule's AST node. */
#define ADD(child)
/* Removes a child from the current rule's AST node. */
#define REMOVE(child)


#endif /* PCC_AST_INCLUDED */
