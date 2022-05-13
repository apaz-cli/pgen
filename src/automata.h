#ifndef PGEN_AUTOMATA_INCLUDE
#define PGEN_AUTOMATA_INCLUDE
#include "util.h"
#include "utf8.h"
#include "list.h"
#include "ast.h"

// loris@cs.wisc.edu
// https://madpl.cs.wisc.edu/
// https://pages.cs.wisc.edu/~loris/symbolicautomata.html

typedef struct {
  ASTNode* token; // The rule that gets accepted.
  int num;
} State;

typedef struct {
  ASTNode* on_charset; // NULL is eps.
  int from;
  int to;
} Transition;

LIST_DECLARE(State);
LIST_DEFINE(State);
LIST_DECLARE(Transition);
LIST_DEFINE(Transition);
LIST_DECLARE(int);
LIST_DEFINE(int);

// Both NFA and DFA. No epsilon transitions.
typedef struct {
  list_State       accepting_states;
  list_Transition  transitions;
  list_State       states;
} Automaton;

LIST_DECLARE(Automaton);
LIST_DEFINE(Automaton);


static inline Automaton createAutomaton(ASTNode* tokast) {
  Automaton aut;
  aut.transitions = list_Transition_new();
  aut.states = list_State_new();
  aut.accepting_states = list_int_new();

  // Create an initial state to epsilon transition off of for each rule.
  State zero_state;
  zero.token = NULL;
  zero.num = 0;
  list_State_add(&aut.states, zero_state);
  list_State_add(&aut.accepting_states, zero_state);

  size_t state_num = 1;
  for (size_t n = 0; n < tokast->num_children; n++) {
    // For each token definition in the AST, convert it to an automaton.
    // The zero state is the start state for all automata.
    // This can be optimized as a trie.

    // Then, combine all the automata together using an epsilon transition
    // from the zero state into the start state for the automaton.
    // This results in one big nondeterministic symbolic finite automaton.
    ASTNode* rule = tokast->children[n];
    ASTNode* ident = rule->children[0];
    ASTNode* def = rule->children[0];
    char* identstr = (char*)ident->extra;

    if (strcmp(rule->name, "LitDef") == 0) {W
      // Turn the codepoint literal into a symbolic automaton
      State last = zero_state;
      list_State litstates = list_State_new();
      list_Transition littransitions = list_Transition_new();

      // Create a chain of states, ending in an accepting state.
      codepoint_t* cpstr = def->extra;
      size_t cplen = cpstrlen(cpstr);
      for (size_t i = 0; i < cplen; i++) P
        State current;
        current.token = (i == (cplen-1)) ? rule : NULL;
        current.num = state_num++;

        Transition trans;
        trans.from = last.num;
        trans.to = current.num;
        trans.on_charset =

        last = current;
      }

    } else { // SMDef

    }
  }

  return aut;
}

static inline Automaton optimizeAutomaton(Automaton aut) {

}

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