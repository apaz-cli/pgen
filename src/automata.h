#ifndef PGEN_AUTOMATA_INCLUDE
#define PGEN_AUTOMATA_INCLUDE
#include "ast.h"
#include "list.h"
#include "utf8.h"
#include "util.h"

// loris@cs.wisc.edu
// https://madpl.cs.wisc.edu/
// https://pages.cs.wisc.edu/~loris/symbolicautomata.html

typedef struct {
  ASTNode *token; // The rule that gets accepted, or NULL.
  int num;
} State;

typedef struct {
  union {
    char c;
    ASTNode *act; // NULL is eps.
  } on;
  int from;
  int to;
  char is_act;
} Transition;

LIST_DECLARE(int);
LIST_DEFINE(int);
LIST_DECLARE(State);
LIST_DEFINE(State);
LIST_DECLARE(Transition);
LIST_DEFINE(Transition);

typedef struct {
  list_State accepting;
  list_Transition trans;
} Automaton;

LIST_DECLARE(Automaton);
LIST_DEFINE(Automaton);

// TODO make sure no duplicate LitDefs.

static inline list_Automaton createAutomata(ASTNode *tokast) {
  list_Automaton auts = list_Automaton_new();

  // For each token literal in the AST, convert it to a trie SFT,
  size_t state_num = 1; // 0 is the trie root.
  for (size_t n = 0; n < tokast->num_children; n++) {
    ASTNode *rule = tokast->children[n];
    if (strcmp(rule->name, "LitDef") != 0)
      continue;

    ASTNode *ident = rule->children[0];
    ASTNode *def = rule->children[1];
    char *identstr = (char *)ident->extra;

    Automaton aut;
    aut.trans = list_Transition_new();
    aut.accepting = list_State_new();

    // Add the literal to the trie.
    int str_state = 0; // trie root
    codepoint_t *cpstr = (codepoint_t *)def->extra;
    size_t cplen = cpstrlen(cpstr);
    for (size_t i = 0; i < cplen; i++) {
      codepoint_t c = cpstr[i];

      // TODO try to find where we are in the trie.

      Transition trans;
      trans.from = str_state;
      trans.to = current.num;
      trans.is_act = (i == (cplen - 1)); // last one
      trans.on.act = trans.is_act ? rule : NULL;

    }
  }

  return auts;
}

#endif /* PGEN_AUTOMATA_INCLUDE */
