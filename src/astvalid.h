#ifndef PGEN_ASTVALID_INCLUDE
#define PGEN_ASTVALID_INCLUDE
#include "ast.h"
#include "util.h"

// TODO: Make sure that every SM transition from has
//       an SM transition to unless it's zero.

// TODO: Make sure that every upperident in the peg corresponds to a token
// TODO: Make sure that every lowerident in the peg corresponds to a rule
// TODO: Make sure that no names of labels and rules overlap
// TODO: Make sure that no labels are named rule or ret.
// TODO: Make sure that labeled ModExprs do not have ModExprLists in them with
// more than one child.

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

static inline void resolveReplace(ASTNode *node, char *prev_name,
                                  char *next_name) {

  // Replace prev or next with the appropriate
  if (!strcmp(node->name, "LowerIdent")) {
    char *id = (char *)node->extra;
    int prev = !strcmp(id, "prev");
    int next = !strcmp(id, "next");
    if (prev | next) {
      char *buf;
      char *replace = prev ? prev_name : next_name;
      if (!replace) {
        ERROR("Cannot replace %s when there is no %s rule.", id,
              prev ? "previous" : "next");
      }
      size_t cpylen = strlen(replace);
      node->extra = buf = (char *)realloc(node->extra, cpylen + 1);
      if (!node->extra)
        OOM();
      for (size_t i = 0; i < cpylen; i++)
        buf[i] = replace[i];
      buf[cpylen] = '\0';
    }
  }

  if (!strcmp(node->name, "ModExpr")) {
    resolveReplace(node->children[0], prev_name, next_name);
  } else {
    for (size_t i = 0; i < node->num_children; i++)
      resolveReplace(node->children[i], prev_name, next_name);
  }
}

typedef ASTNode *ASTNodePtr;
LIST_DECLARE(ASTNodePtr)
LIST_DEFINE(ASTNodePtr)
static inline void resolvePrevNext(ASTNode *pegast) {

  // Filter for definitions
  list_ASTNodePtr defs = list_ASTNodePtr_new();
  for (size_t i = 0; i < pegast->num_children; i++) {
    ASTNode *def = pegast->children[i];
    if (strcmp(def->name, "Definition"))
      continue;
    list_ASTNodePtr_add(&defs, def);
  }

  // Resolve prev and next by replacing content of LowerIdents.
  char *prev_name = NULL, *next_name = NULL;
  for (size_t i = 0; i < defs.len; i++) {
    prev_name = i ? (char *)defs.buf[i - 1]->children[0]->extra : NULL;
    next_name = (i == defs.len - 1)
                    ? NULL
                    : (char *)defs.buf[i + 1]->children[0]->extra;
    resolveReplace(defs.buf[i], prev_name, next_name);
  }

  list_ASTNodePtr_clear(&defs);
}

static inline void validatePegast(ASTNode *pegast) { resolvePrevNext(pegast); }

#endif /* PGEN_ASTVALID_INCLUDE */
