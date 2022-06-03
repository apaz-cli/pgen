#ifndef PGEN_ASTVALID_INCLUDE
#define PGEN_ASTVALID_INCLUDE
#include "ast.h"
#include "util.h"

// TODO: Make sure that every SM from has an SM to unless it's zero.

static inline void validateTokast(ASTNode *tokast) {
  // Cross compare for duplicate rules.
  // Also make sure that there's no token named STREAMEND.
  for (size_t n = 0; n < tokast->num_children; n++) {
    ASTNode *rule1 = tokast->children[n];
    ASTNode *def1 = rule1->children[1];
    codepoint_t *cpstr1 = (codepoint_t *)def1->extra;
    char *identstr1 = (char *)rule1->children[0]->extra;
    if (strcmp(identstr1, "STREAMEND") == 0) {
      fprintf(stderr, "Error: Tokenizer rules cannot be named STREAMEND, "
                      "because it's reserved for end of token stream.\n");
      exit(1);
    }
    if (strcmp(def1->name, "LitDef") != 0)
      continue;

    for (size_t j = 0; j < tokast->num_children; j++) {
      if (j == n)
        continue;

      ASTNode *rule2 = tokast->children[j];
      ASTNode *def2 = rule2->children[1];
      codepoint_t *cpstr2 = (codepoint_t *)def2->extra;
      char *identstr2 = (char *)rule2->children[0]->extra;
      if (strcmp(def2->name, "LitDef") != 0)
        continue;

      {
        if (strcmp(identstr1, identstr2) == 0) {
          fprintf(stderr, "Error: There are two or more rules named %s.\n",
                  identstr1);
          exit(1);
        }

        if (cpstr_equals(cpstr1, cpstr2)) {
          fprintf(stderr, "Error: Tokenizer literals %s and %s are equal.\n",
                  identstr1, identstr2);
          Codepoint_String_View cpsv;
          cpsv.str = cpstr1;
          cpsv.len = cpstrlen(cpstr1);
          String_View sv = UTF8_encode_view(cpsv);
          fprintf(stderr, "str: %s\n", sv.str);
          exit(1);
        }
      }
    }
  }
}

static inline void validatePegast(ASTNode *pegast) {}

#endif /* PGEN_ASTVALID_INCLUDE */
