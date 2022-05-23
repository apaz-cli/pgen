#ifndef PGEN_ASTMOD_INCLUDE
#define PGEN_ASTMOD_INCLUDE
#include "ast.h"
#include "util.h"

static inline ASTNode *simplifyAST(ASTNode *node) {

  if (strcmp(node->name, "GrammarFile") == 0) {
    for (size_t i = 0; i < node->num_children; i++)
      simplifyAST(node->children[i]);
  } else if (strcmp(node->name, "Definition") == 0) {
    simplifyAST(node->children[1]);
  } else if (strcmp(node->name, "SlashExpr") == 0) {
    
  } else if (strcmp(node->name, "ModExprList") == 0) {
    
  } else if (strcmp(node->name, "") == 0) {
    
  } else if (strcmp(node->name, "") == 0) {
    
  } else if (strcmp(node->name, "") == 0) {
    
  }

  return node;
}

#endif /* PGEN_ASTMOD_INCLUDE */