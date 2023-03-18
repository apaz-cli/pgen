#ifndef PGEN_ASTVALID_INCLUDE
#define PGEN_ASTVALID_INCLUDE
#include "argparse.h"
#include "ast.h"
#include "codegen.h"
#include "util.h"

static inline void validateTokdefs(list_ASTNodePtr tokdefs) {

  // TODO error if two tokenizer rules contain the same content

  // Cross compare for duplicate rules.
  // Also make sure that there's no token named STREAMBEGIN or STREAMEND.
  for (size_t n = 0; n < tokdefs.len; n++) {
    ASTNode *rule1 = tokdefs.buf[n];
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

    for (size_t j = 0; j < tokdefs.len; j++) {
      if (j == n)
        continue;

      ASTNode *rule2 = tokdefs.buf[j];
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

static inline void validateVisitLabel(ASTNode *label, list_cstr *names) {
  char *lname = (char *)label->extra;
  if (!strcmp(lname, "ret"))
    ERROR("Labels cannot be named \"ret\".");
  for (size_t i = 0; i < names->len; i++) {
    if (!strcmp(lname, names->buf[i]))
      ERROR("Cannot use %s as both a rule and a label.", lname);
  }
}

static inline void validatePegVisit(ASTNode *node, list_ASTNodePtr* tokdefs,
                                    list_cstr *names) {
  // Pull out all the upperidents from pegast and find them in the tokast.
  if (!strcmp(node->name, "UpperIdent")) {
    char *name = (char *)node->extra;
    int tokfound = 0;
    for (size_t i = 0; i < tokdefs->len; i++) {
      char *tokname = (char *)tokdefs->buf[i]->children[0]->extra;
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
      validatePegVisit(node->children[0], tokdefs, names);
      ModExprOpts opts = *(ModExprOpts *)node->extra;
      if (opts.optional & opts.inverted)
        ERROR("For the sake of preventing ambiguities, a rule "
              "cannot be both optional and inverted. Please "
              "wrap your rules in parentheses like `!(&rule)` "
              "to ensure the intended behavior.");
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
        validatePegVisit(node->children[i], tokdefs, names);
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

static inline void validateDefinitions(list_cstr defnames) {
  for (size_t i = 0; i < defnames.len; i++)
    for (size_t j = i + 1; j < defnames.len; j++)
      if (!strcmp(defnames.buf[i], defnames.buf[j]))
        ERROR("More than one rule is named %s.", defnames.buf[i]);
}

static inline void validateSymtabs(Args args, Symtabs symtabs) {

  if (args.u)
    return;

  list_cstr *tks = &symtabs.tok_kind_names;
  list_cstr *pks = &symtabs.peg_kind_names;
  if (!args.u)
    for (size_t j = 0; j < pks->len; j++)
      for (size_t i = 0; i < tks->len; i++)
        if (!strcmp(tks->buf[i], pks->buf[j]))
          ERROR("%%node kind %s is already declared as a token.\n", tks->buf[i]);

  list_cstr defnames = list_cstr_new();
  for (size_t i = 0; i < symtabs.definitions.len; i++) {
    cstr defname = (char *)symtabs.definitions.buf[i]->children[0]->extra;
    list_cstr_add(&defnames, defname);
  }

  validateTokdefs(symtabs.tokendefs);
  validateDefinitions(defnames);
  validateDirectives(args, &symtabs.directives);
  for (size_t i = 0; i < symtabs.tokendefs.len; i++)
    validatePegVisit(symtabs.tokendefs.buf[i], &symtabs.tokendefs, &defnames);

  validateLeftRecursion(&symtabs.definitions, &defnames);

  list_cstr_clear(&defnames);
}

#endif /* PGEN_ASTVALID_INCLUDE */
