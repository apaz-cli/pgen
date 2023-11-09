#ifndef PGEN_SYMTAB_INCLUDE
#define PGEN_SYMTAB_INCLUDE
#include "ast.h"
#include "util.h"

typedef struct {
  list_ASTNodePtr directives;
  list_ASTNodePtr definitions;
  list_ASTNodePtr tokendefs;
  list_cstr tok_kind_names;
  list_cstr peg_kind_names;
} Symtabs;

static inline void resolvePrevNext(list_ASTNodePtr *defs);

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

// Also pulls out all the names of the rules.
static inline void resolvePrevNext(list_ASTNodePtr *defs) {
  // Resolve prev and next by replacing content of LowerIdents.
  char *prev_name = NULL, *next_name = NULL;
  for (size_t i = 0; i < defs->len; i++) {
    int l = (i != defs->len - 1);
    prev_name = i ? (char *)defs->buf[i - 1]->children[0]->extra : NULL;
    next_name = l ? (char *)defs->buf[i + 1]->children[0]->extra : NULL;
    ASTNode *def = defs->buf[i];
    char *rulename = (char *)def->children[0]->extra;
    if ((!strcmp(rulename, "prev")) | (!strcmp(rulename, "next")))
      ERROR("No rule can be named \"prev\" or \"next\".");
    resolveReplace(def->children[1], prev_name, next_name);
  }
}

// Creates the symbol tables
static inline Symtabs gen_symtabs(ASTNode *ast) {

  Symtabs s;
  s.directives = list_ASTNodePtr_new();
  s.definitions = list_ASTNodePtr_new();
  s.tokendefs = list_ASTNodePtr_new();
  s.tok_kind_names = list_cstr_new();
  s.peg_kind_names = list_cstr_new();

  // Grab all the directives, and make sure their contents are reasonable.
  for (size_t i = 0; i < ast->num_children; i++) {
    ASTNode *node = ast->children[i];
    if (!strcmp(node->name, "Directive")) {
      list_ASTNodePtr_add(&s.directives, node);
      if (!strcmp((char *)node->children[0]->extra, "node"))
        list_cstr_add(&s.peg_kind_names, (char *)node->extra);
      if (!strcmp((char *)node->children[0]->extra, "token"))
        list_cstr_add(&s.tok_kind_names, (char *)node->extra);
    } else if (!strcmp(node->name, "TokenDef")) {
      list_ASTNodePtr_add(&s.tokendefs, node);
      list_cstr_add(&s.tok_kind_names, (char *)node->children[0]->extra);
    } else if (!strcmp(node->name, "Definition")) {
      list_ASTNodePtr_add(&s.definitions, node);
      // list_cstr_add(&s.peg_kind_names, (char *)node->extra);
    } else
      ERROR("Unknown top level node: %s", node->name);
  }

  // replace prev/next with reference to the rules above and below.
  resolvePrevNext(&s.definitions);

  return s;
}

#endif /* PGEN_SYMTAB_INCLUDE */
