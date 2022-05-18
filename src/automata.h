#ifndef PGEN_AUTOMATA_INCLUDE
#define PGEN_AUTOMATA_INCLUDE
#include "ast.h"
#include "list.h"
#include "utf8.h"
#include "util.h"

#define AUT_PRINT 0

// loris@cs.wisc.edu
// https://madpl.cs.wisc.edu/
// https://pages.cs.wisc.edu/~loris/symbolicautomata.html

typedef struct {
  ASTNode *rule; // The rule that gets accepted, or NULL.
  int num;
} State;

typedef struct {
  union {
    codepoint_t c;
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

static inline list_int numset_to_list(ASTNode *numset) {
  list_int l = list_int_new();
  if (strcmp(numset->name, "Num") == 0) {
    list_int_add(&l, *(int *)numset->extra);
  } else if (strcmp(numset->name, "NumRange") == 0) {
    int *iptr = (int *)numset->extra;
    int first = *iptr, second = *(iptr + 1);
    for (int i = first; i <= second; i++)
      list_int_add(&l, i);
  } else if (strcmp(numset->name, "NumSetList") == 0) {
    // recurse, combine.
    for (size_t i = 0; i < numset->num_children; i++) {
      list_int _l = numset_to_list(numset->children[i]);
      for (size_t j = 0; j < _l.len; j++)
        list_int_add(&l, list_int_get(&_l, j));
      list_int_clear(&_l);
    }
  } else
    ERROR("%s is not a numset.", numset->name);

  return l;
}

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

static inline list_Automaton createAutomata(ASTNode *tokast) {
  validateTokast(tokast);

  list_Automaton auts = list_Automaton_new();

  // Build the trie automaton.
  if (AUT_PRINT)
    puts("Building the Trie automaton.");
  Automaton trie;
  trie.trans = list_Transition_new();
  trie.accepting = list_State_new();
  // For each token literal in the AST, add it as a path to the trie.
  int state_num = 1; // 0 is the trie root.
  for (size_t n = 0; n < tokast->num_children; n++) {

    ASTNode *rule = tokast->children[n];
    ASTNode *ident = rule->children[0];
    ASTNode *def = rule->children[1];
    char *identstr = (char *)ident->extra;

    if (strcmp(def->name, "LitDef") != 0)
      continue;

    // Add the literal to the trie.
    int prev_state = 0; // trie root
    codepoint_t *cpstr = (codepoint_t *)def->extra;
    size_t cplen = cpstrlen(cpstr);
    for (size_t i = 0; i < cplen; i++) {
      codepoint_t c = cpstr[i];
      if (AUT_PRINT)
        printf("Current: %c\n", (char)c);

      // Try to find the next state after the transition of c.
      // See if we've already created a transition from where we are in the trie
      // to the next character.
      Transition trans;
      int found = 0;
      for (size_t j = 0; j < trie.trans.len; j++) {
        Transition t = list_Transition_get(&trie.trans, j);
        if ((t.from == prev_state) & (t.on.c == c)) {
          found = true;
          trans = t;
          if (AUT_PRINT)
            printf("Found a transition from %i to %i on '%c'.\n", trans.from,
                   trans.to, (char)trans.on.c);
          break;
        }
      }

      // If the transition hasn't been created, create the transition and the
      // state that it goes to.
      if (!found) {
        trans.from = prev_state;
        trans.to = state_num++;
        trans.is_act = 0;
        trans.on.c = c;
        list_Transition_add(&trie.trans, trans);
        if (AUT_PRINT)
          printf("Created a transition from %i to %i on '%c'.\n", trans.from,
                 trans.to, (char)trans.on.c);
      }

      // If the state we just created is the last character in the string,
      // it's accepting.
      if (i == (cplen - 1)) {
        State s;
        s.rule = rule;
        s.num = trans.to;
        list_State_add(&trie.accepting, s);
        if (AUT_PRINT)
          printf("Marked %i as an accepting state for rule %s.\n", s.num,
                 identstr);
      }

      // Traverse over the transition.
      prev_state = trans.to;
    } // c in cpstr
  }   // litdef in tokast

  // Add the trie automaton to the list.
  list_Automaton_add(&auts, trie);

  // Now, build SFAs for the state machine definitions.
  for (size_t n = 0; n < tokast->num_children; n++) {
    Automaton aut;
    aut.accepting = list_State_new();
    aut.trans = list_Transition_new();

    ASTNode *r = tokast->children[n];
    ASTNode *ident = r->children[0];
    ASTNode *def = r->children[1];
    char *identstr = (char *)ident->extra;

    if (strcmp(def->name, "SMDef") != 0)
      continue;

    if (AUT_PRINT)
      printf("Building automaton %zu.\n", auts.len);

    // Grab the accepting states.
    ASTNode *accepting = def->children[0];
    list_int accepting_states = numset_to_list(accepting);
    for (size_t i = 0; i < accepting_states.len; i++) {
      State s;
      s.num = list_int_get(&accepting_states, i);
      s.rule = r;
      list_State_add(&aut.accepting, s);
    }
    list_int_clear(&accepting_states);

    // Process each rule
    ASTNode **rules = def->children + 1;
    size_t num_rules = def->num_children - 1;
    for (size_t k = 0; k < num_rules; k++) {
      ASTNode *rule = rules[k];
      ASTNode *pair = rule->children[0];
      ASTNode *startingStates = pair->children[0];
      ASTNode *onCharset = pair->children[1];
      int toState = *(int *)rule->extra;
      if (AUT_PRINT)
        printf("Parsing rule %zu.\n", k);

      // For each starting state, make a transition to the end state on the
      // charset.
      list_int starting = numset_to_list(startingStates);
      for (size_t m = 0; m < starting.len; m++) {
        Transition t;
        t.from = list_int_get(&starting, m);
        t.to = toState;
        t.is_act = 1;
        t.on.act = onCharset;
        list_Transition_add(&aut.trans, t);
      } // starting state in rule
    }   // rule in smdef
    list_Automaton_add(&auts, aut);
  } // automaton for smdef in tokast

  if (AUT_PRINT) {
    for (size_t k = 0; k < auts.len; k++) {
      Automaton aut = list_Automaton_get(&auts, k);
      for (size_t i = 0; i < aut.accepting.len; i++) {
        State s = list_State_get(&aut.accepting, i);
        printf("Accepting state: (%i, %s)\n", s.num,
               (char *)s.rule->children[0]->extra);
      }
      for (size_t i = 0; i < aut.trans.len; i++) {
        Transition t = list_Transition_get(&aut.trans, i);
        if (t.is_act)
          printf("Transition: (%i, %p) -> (%i)\n", t.from, t.on.act, t.to);
        else
          printf("Transition: (%i, '%c') -> (%i)\n", t.from, t.on.c, t.to);
      }
      fflush(stdout);
    }
  }

  return auts;
}

#endif /* PGEN_AUTOMATA_INCLUDE */
