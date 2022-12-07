#ifndef PGEN_ASTVALID_INCLUDE
#define PGEN_ASTVALID_INCLUDE
#include "argparse.h"
#include "ast.h"
#include "util.h"

// TODO: Make sure that every SM transition from has
//       an SM transition to unless it's zero.

// TODO: Make sure that labeled ModExprs do not have ModExprLists in them with
// more than one child.

// TODO: Warn on token definitions not used in the parser.

static inline void validateTokast(Args args, ASTNode *tokast) {
  // Cross compare for duplicate rules.
  // Also make sure that there's no token named STREAMEND.
  for (size_t n = 0; n < tokast->num_children; n++) {
    ASTNode *rule1 = tokast->children[n];
    ASTNode *def1 = rule1->children[1];
    codepoint_t *cpstr1 = (codepoint_t *)def1->extra;
    char *identstr1 = (char *)rule1->children[0]->extra;
    // Singular comparisons
    if (strcmp(identstr1, "STREAMEND") == 0) {
      fprintf(stderr,
              "Error: Tokenizer rules cannot be named STREAMEND, "
              "because it's reserved for the end of the token stream.\n");
      exit(1);
    }
    if (strcmp(identstr1, "STREAMBEGIN") == 0) {
      fprintf(stderr,
              "Error: Tokenizer rules cannot be named STREAMBEGIN, "
              "because it's reserved for the beginning of the token stream.\n");
      exit(1);
    }
    if (strcmp(def1->name, "LitDef"))
      continue;

    for (size_t j = 0; j < tokast->num_children; j++) {
      if (j == n)
        continue;

      ASTNode *rule2 = tokast->children[j];
      ASTNode *def2 = rule2->children[1];
      codepoint_t *cpstr2 = (codepoint_t *)def2->extra;
      char *identstr2 = (char *)rule2->children[0]->extra;
      // Triangular comparisons
      if (j > n) {
        if (strcmp(def2->name, "LitDef"))
          continue;

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
typedef char *charptr;
LIST_DECLARE(charptr)
LIST_DEFINE(charptr)

// Also pulls out all the names of the rules.
static inline list_charptr resolvePrevNext(ASTNode *pegast) {
  if (!pegast)
    return list_charptr_new();

  // Filter for definitions, pull out names
  list_ASTNodePtr defs = list_ASTNodePtr_new();
  list_charptr names = list_charptr_new();
  for (size_t i = 0; i < pegast->num_children; i++) {
    ASTNode *def = pegast->children[i];
    if (strcmp(def->name, "Definition"))
      continue;
    list_ASTNodePtr_add(&defs, def);
    list_charptr_add(&names, (char *)def->children[0]->extra);
  }

  // Resolve prev and next by replacing content of LowerIdents.
  char *prev_name = NULL, *next_name = NULL;
  for (size_t i = 0; i < names.len; i++) {
    prev_name = i ? names.buf[i - 1] : NULL;
    next_name = (i == defs.len - 1) ? NULL : names.buf[i + 1];
    resolveReplace(defs.buf[i], prev_name, next_name);
  }

  list_ASTNodePtr_clear(&defs);
  return names;
}

static inline void validateVisitLabel(ASTNode *label, list_charptr *names) {
  char *lname = (char *)label->extra;
  if (!strcmp(lname, "rule") || !strcmp(lname, "ret"))
    ERROR("Labels cannot be named \"rule\" or \"ret\".");
  for (size_t i = 0; i < names->len; i++) {
    if (!strcmp(lname, names->buf[i]))
      ERROR("Cannot use %s as both a rule and a label.", lname);
  }
}

static inline void validatePegVisit(ASTNode *node, ASTNode *tokast,
                                    list_charptr *names) {
  // Pull out all the upperidents from pegast and find them in the tokast.
  if (!strcmp(node->name, "UpperIdent")) {
    char *name = (char *)node->extra;
    int tokfound = 0;
    for (size_t i = 0; i < tokast->num_children; i++) {
      char *tokname = (char *)tokast->children[i]->children[0]->extra;
      if (!strcmp(name, tokname)) {
        tokfound = 1;
        break;
      }
    }
    if (!tokfound)
      ERROR("%s appears in the parser, but does not have a token definition.",
            name);
  }

  // Ensure all rules are defined.
  if (!strcmp(node->name, "LowerIdent")) {
    char *name = (char *)node->extra;
    int rulefound = 0;
    for (size_t i = 0; i < names->len; i++) {
      if (!strcmp(name, names->buf[i])) {
        rulefound = 1;
        break;
      }
    }
    if (!rulefound)
      ERROR("%s appears as a rule, but has no definition.", name);
  }

  // Recurse to everything but labels.
  for (size_t i = 0; i < node->num_children; i++) {
    if (!strcmp(node->name, "Directive")) {
    } else if (!strcmp(node->name, "ModExpr")) {
      validatePegVisit(node->children[0], tokast, names);
      if (node->num_children > 1)
        validateVisitLabel(node->children[1], names);
    } else {
      for (size_t i = 0; i < node->num_children; i++)
        validatePegVisit(node->children[i], tokast, names);
    }
  }
}

static inline void validateDirectives(Args args, list_ASTNodePtr *directives) {
  if (!args.u)
    return;
  for (size_t i = 0; i < directives->len; i++) {
    for (size_t j = i + 1; j < directives->len; j++) {
      char *dir_name1 = (char *)directives->buf[i]->children[0]->extra;
      char *dir_name2 = (char *)directives->buf[j]->children[0]->extra;
      if (!strcmp(dir_name1, "node") && !strcmp(dir_name2, "node")) {
        // Note: invalid identifiers are taken care of in codegen.
        char *nname1 = (char *)directives->buf[i]->extra;
        char *nname2 = (char *)directives->buf[j]->extra;
        if (!strcmp(nname1, nname2)) {
          ERROR("There are two %%node directives both naming %s.", nname1);
        }
      }
    }
  }
}

static inline void validateRewritePegast(Args args, ASTNode *pegast, ASTNode *tokast) {
  if (!pegast)
    return;

  list_ASTNodePtr directives;
  if (!args.u) {
    directives = list_ASTNodePtr_new();
    for (size_t i = 0; i < pegast->num_children; i++) {
      ASTNode *node = pegast->children[i];
      if (!strcmp(node->name, "Directive"))
        list_ASTNodePtr_add(&directives, node);
    }
  }

  list_charptr names = resolvePrevNext(pegast);

  if (!args.u) {
    validatePegVisit(pegast, tokast, &names);
    validateDirectives(args, &directives);

    list_ASTNodePtr_clear(&directives);
  }

  list_charptr_clear(&names);
}

#endif /* PGEN_ASTVALID_INCLUDE */
