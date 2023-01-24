#ifndef PGEN_ASTVALID_INCLUDE
#define PGEN_ASTVALID_INCLUDE
#include "argparse.h"
#include "ast.h"
#include "codegen.h"
#include "util.h"

static inline void validateTokast(Args args, ASTNode *tokast) {
  (void)args;

  // TODO error if two tokenizer rules contain the same content

  // Cross compare for duplicate rules.
  // Also make sure that there's no token named STREAMBEGIN or STREAMEND.
  for (size_t n = 0; n < tokast->num_children; n++) {
    ASTNode *rule1 = tokast->children[n];
    ASTNode *def1 = rule1->children[1];
    codepoint_t *cpstr1 = (codepoint_t *)def1->extra;
    char *identstr1 = (char *)rule1->children[0]->extra;
    // Singular comparisons
    if (!strcmp(identstr1, "STREAMEND")) {
      fprintf(stderr,
              "Error: Tokenizer rules cannot be named STREAMEND, "
              "because it's reserved for the end of the token stream.\n");
      exit(1);
    }
    if (!strcmp(identstr1, "STREAMBEGIN")) {
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

        if (!strcmp(identstr1, identstr2)) {
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

// Also pulls out all the names of the rules.
static inline void resolvePrevNext(list_ASTNodePtr *defs) {
  // Resolve prev and next by replacing content of LowerIdents.
  char *prev_name = NULL, *next_name = NULL;
  for (size_t i = 0; i < defs->len; i++) {
    int f = !!i;
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

static inline void validateVisitLabel(ASTNode *label, list_cstr *names) {
  char *lname = (char *)label->extra;
  if (!strcmp(lname, "ret"))
    ERROR("Labels cannot be named \"ret\".");
  for (size_t i = 0; i < names->len; i++) {
    if (!strcmp(lname, names->buf[i]))
      ERROR("Cannot use %s as both a rule and a label.", lname);
  }
}

static inline void validatePegVisit(ASTNode *node, ASTNode *tokast,
                                    list_cstr *names) {
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
      // Also recurse to error handler
      if (node->num_children > 1)
        for (size_t z = 1; z < node->num_children; z++) {
          if (strcmp(node->children[z]->name, "BaseExpr")) {
            validateVisitLabel(node->children[z], names);
            break;
          }
        }
    } else {
      for (size_t i = 0; i < node->num_children; i++)
        validatePegVisit(node->children[i], tokast, names);
    }
  }
}

static inline void validateDirectives(Args args, list_ASTNodePtr *directives) {
  if (args.u)
    return;

  for (size_t i = 0; i < directives->len; i++) {
    ASTNode *node = directives->buf[i];

    // Compare current
    if (!strcmp((char *)node->children[0]->extra, "node")) {
      char *dir_content = (char *)node->extra;
      for (size_t i = 0; i < strlen(dir_content); i++) {
        char c = dir_content[i];
        if (((c < 'A') | (c > 'Z')) & (c != '_') & ((c < '0') | (c > '9'))) {
          ERROR("Node kind %s would not create a valid (uppercase) identifier.",
                dir_content);
        }
      }
    }

    // Cross-compare
    for (size_t j = i + 1; j < directives->len; j++) {
      char *dir_name1 = (char *)directives->buf[i]->children[0]->extra;
      char *dir_name2 = (char *)directives->buf[j]->children[0]->extra;
      if (!strcmp(dir_name1, "node") && !strcmp(dir_name2, "node")) {
        // Note: Undeclared directives are taken care of in codegen.
        char *nname1 = (char *)directives->buf[i]->extra;
        char *nname2 = (char *)directives->buf[j]->extra;
        if (!strcmp(nname1, nname2)) {
          ERROR("There are two %%node directives for %s.", nname1);
        }
      }
    }
  }
}

static inline int is_left_recursive(list_ASTNodePtr *definitions,
                                    list_cstr *defnames, ASTNode *rule,
                                    char *name, list_cstr *trace) {
  // Find the first-executed rule.
  // If it's not a LowerExpr, return 0.
  // If it is, recurse and check that rule, keeping track of the trace.
  while (1) {
    if (!strcmp(rule->name, "Definition")) {
      rule = rule->children[1];
    } else if (!strcmp(rule->name, "SlashExpr")) {
      rule = rule->children[0];
    } else if (!strcmp(rule->name, "ModExprList")) {
      if (!rule->num_children) {
        list_cstr_clear(trace);
        fprintf(stderr, "PGEN Warning: Empty rule %s.\n", name);
        return 0;
      }
      rule = rule->children[0];
    } else if (!strcmp(rule->name, "ModExpr")) {
      rule = rule->children[0];
    } else if (!strcmp(rule->name, "BaseExpr")) {
      rule = rule->children[0];
    } else if (!strcmp(rule->name, "CodeExpr")) {
      return list_cstr_clear(trace), 0;
    } else if (!strcmp(rule->name, "UpperIdent")) {
      return list_cstr_clear(trace), 0;
    } else if (!strcmp(rule->name, "LowerIdent")) {
      break;
    } else
      ERROR("Unexpected astnode kind in left recursion check: %s", rule->name);
  }
  list_cstr_add(trace, name);
  char *nextrulename = (char *)rule->extra;

  // Is directly left recursive (bad).
  if (!strcmp(nextrulename, name)) {
    list_cstr_add(trace, nextrulename);
    return 2;
  }

  // Not directly left recursive. Check if indirectly left recursive.
  else {
    for (size_t i = 0; i < trace->len; i++) {
      if (!strcmp(nextrulename, trace->buf[i])) {
        list_cstr_add(trace, nextrulename);
        return 1;
      }
    }

    ASTNode *nextrule = NULL;
    for (size_t i = 0; i < defnames->len; i++) {
      if (!strcmp(nextrulename, defnames->buf[i])) {
        nextrule = definitions->buf[i];
        break;
      }
    }
    if (!nextrule)
      ERROR("While checking for left recurison, "
            "could not find next rule %s.\n",
            nextrulename);
    return is_left_recursive(definitions, defnames, nextrule, nextrulename,
                             trace);
  }
}

static inline void validateLeftRecursion(list_ASTNodePtr *definitions,
                                         list_cstr *defnames) {
  for (size_t i = 0; i < definitions->len; i++) {
    list_cstr trace = list_cstr_new();
    int lr = is_left_recursive(definitions, defnames, definitions->buf[i],
                               defnames->buf[i], &trace);
    if (lr) {
      fprintf(stderr, "%s left recursion is not allowed. Trace:\n",
              lr == 2 ? "Direct (or indirect)" : "Indirect (or direct)");
      for (size_t j = 0; j < trace.len - 1; j++)
        fprintf(stderr, "%s <- %s\n", trace.buf[j], trace.buf[j + 1]);
      exit(1);
    }
  }
}

static inline void validateDefinitions(list_ASTNodePtr *definitions,
                                       list_cstr *defnames) {
  for (size_t i = 0; i < defnames->len; i++) {
    for (size_t j = i + 1; j < defnames->len; j++) {
      if (!strcmp(defnames->buf[i], defnames->buf[j])) {
        ERROR("More than one rule is named %s.", defnames->buf[i]);
      }
    }
  }
}

static inline void validateRewritePegast(Args args, ASTNode *pegast,
                                         ASTNode *tokast) {
  if (!pegast)
    return;

  // Grab all the directives, and make sure their contents are reasonable.
  list_ASTNodePtr directives = list_ASTNodePtr_new();
  list_ASTNodePtr definitions = list_ASTNodePtr_new();
  list_cstr defnames = list_cstr_new();
  for (size_t i = 0; i < pegast->num_children; i++) {
    ASTNode *node = pegast->children[i];
    if (!strcmp(node->name, "Directive")) {
      if (!args.u)
        list_ASTNodePtr_add(&directives, node);
    } else {
      list_ASTNodePtr_add(&definitions, node);
      if (!args.u)
        list_cstr_add(&defnames, (char *)node->children[0]->extra);
    }
  }

  // replace prev, next
  resolvePrevNext(&definitions);

  if (!args.u) {
    validateDefinitions(&definitions, &defnames);
    validatePegVisit(pegast, tokast, &defnames);
    validateDirectives(args, &directives);
    validateLeftRecursion(&definitions, &defnames);
  }
  list_ASTNodePtr_clear(&directives);
  list_ASTNodePtr_clear(&definitions);
  list_cstr_clear(&defnames);
}

#endif /* PGEN_ASTVALID_INCLUDE */
