#ifndef PGEN_AST_INCLUDED
#define PGEN_AST_INCLUDED
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "list.h"

/**********************/
/* AST Implementation */
/**********************/

struct ASTNode;
typedef struct ASTNode ASTNode;
struct ASTNode {
  const char *name;
  // ASTNode *parent;
  ASTNode **children;
  size_t num_children;
  void *extra;
};

typedef ASTNode *ASTNodePtr;
LIST_DECLARE(ASTNodePtr)
LIST_DEFINE(ASTNodePtr)

#define PGENAST_OOM()                                                           \
  do {                                                                         \
    fprintf(stderr, "Out of memory.\n");                                       \
    exit(1);                                                                   \
  } while (0)

static inline ASTNode *ASTNode_new(const char *name) {
  ASTNode *node = (ASTNode *)malloc(sizeof(ASTNode));
  node->name = name;
  //node->parent = NULL;
  node->children = NULL; // realloc() as children are added
  node->num_children = 0;
  node->extra = NULL;
  return node;
}

static inline void ASTNode_addChild(ASTNode *parent, ASTNode *child) {
  if (!child)
    return;
  if (!parent)
    return;
  parent->num_children++;
  parent->children = (ASTNode **)realloc(
      parent->children, sizeof(ASTNode *) * parent->num_children);
  parent->children[parent->num_children - 1] = child;
  if (!parent->children)
    PGENAST_OOM();
}

static inline void ASTNode_destroy(ASTNode *self) {
  for (size_t i = 0; i < self->num_children; i++)
    ASTNode_destroy(self->children[i]);
  free(self->children);
  free(self->extra);
  free(self);
}

static inline void ASTNode_clearChildren(ASTNode *self) {
  for (size_t i = 0; i < self->num_children; i++)
    ASTNode_destroy(self->children[i]);
  free(self->children);
  free(self->extra);
  self->children = NULL;
  self->extra = NULL;
}

static inline void AST_print_helper(ASTNode *current, size_t depth) {
  /* Print current node. */
  for (size_t i = 0; i < depth; i++)
    printf("  ");
  puts(current->name);

  /* Print children. */
  for (size_t i = 0; i < current->num_children; i++)
    AST_print_helper(current->children[i], depth + 1);
}

static inline void AST_print(ASTNode *root) { AST_print_helper(root, 0); }

#endif /* PGEN_AST_INCLUDED */
