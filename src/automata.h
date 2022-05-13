#ifndef PGEN_AUTOMATA_INCLUDE
#define PGEN_AUTOMATA_INCLUDE
#include "util.h"
#include "utf8.h"
#include "list.h"
#include "ast.h"

// loris@cs.wisc.edu 
// https://madpl.cs.wisc.edu/
// https://pages.cs.wisc.edu/~loris/symbolicautomata.html

struct DFANode;
typedef struct DFANode DFANode;

typedef struct {
  ASTNode* on_charset; // NULL is eps.
  int from;
  int to;
} DFATransition;

LIST_DECLARE(DFATransition);
LIST_DEFINE(DFATransition);
LIST_DECLARE(int);
LIST_DEFINE(int);

// Both NFA and DFA. No epsilon transitions.
typedef struct {
  list_int           accepting_states;
  list_DFATransition transitions;
} Automaton;

LIST_DECLARE(Automaton);
LIST_DEFINE(Automaton);


static inline Automaton combineAutomata(list_Automaton automata);

static inline Automaton reverseTransitions(Automaton aut);

static inline Automaton powersetConstruction(Automaton aut);

static inline Automaton brzozowskiAlgorithm(Automaton aut) {
  aut = reverseTransitions(aut);
  aut = powersetConstruction(aut);
  aut = reverseTransitions(aut);
  aut = powersetConstruction(aut);
  return aut;
}

#endif /* PGEN_AUTOMATA_INCLUDE */
