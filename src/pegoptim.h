#ifndef PGEN_OPTIMIZATIONS
#define PGEN_OPTIMIZATIONS
#include "codegen.h"

static inline void nodecmp(ASTNode *first, ASTNode *second) {}

static inline void left_factor_rule(ASTNode *expr) {
  if (!strcmp(expr->name, "SlashExpr")) {
    for (size_t i = 0; i < expr->num_children; i++) {
      for (size_t j = i + 1; j < expr->num_children; j++) {
      }
    }
  }

  for (size_t i = 0; i < expr->num_children; i++)
    left_factor_rule(expr->children[i]);
}

static inline ASTNode *left_factor(ASTNode *pegast) {

  for (size_t i = 0; i < pegast->num_children; i++) {
    ASTNode *rule = pegast->children[i];
    if (strcmp(rule->name, "Definition"))
      continue;
    char *rulename = (char *)rule->children[0]->extra;
    left_factor_rule(rule->children[1]);
  }

  return pegast;
}

static inline ASTNode *left_recursion_eliminate(ASTNode *pegast) {
  return pegast;
}

#endif