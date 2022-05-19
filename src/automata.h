#ifndef PGEN_AUTOMATA_INCLUDE
#define PGEN_AUTOMATA_INCLUDE
#include "ast.h"
#include "list.h"
#include "utf8.h"
#include "util.h"

#define AUT_PRINT 1
#define AUT_DEBUG 0

// loris@cs.wisc.edu
// https://madpl.cs.wisc.edu/
// https://pages.cs.wisc.edu/~loris/symbolicautomata.html

typedef struct {
  ASTNode *rule; // The rule that gets accepted, or NULL.
  int num;
} State;

typedef struct {
  codepoint_t c;
  int from;
  int to;
} TrieTransition;

LIST_DECLARE(int);
LIST_DEFINE(int);
LIST_DECLARE(State);
LIST_DEFINE(State);
LIST_DECLARE(TrieTransition);
LIST_DEFINE(TrieTransition);

typedef struct {
  ASTNode *act;
  list_int from;
  int to;
} SMTransition;

LIST_DECLARE(SMTransition);
LIST_DEFINE(SMTransition);

typedef struct {
  list_State accepting;
  list_TrieTransition trans;
} TrieAutomaton;

typedef struct {
  list_State accepting;
  list_SMTransition trans;
} SMAutomaton;

LIST_DECLARE(TrieAutomaton);
LIST_DEFINE(TrieAutomaton);
LIST_DECLARE(SMAutomaton);
LIST_DEFINE(SMAutomaton);

// TODO What if there is no trie?
// TODO How will the tokenizer consume whitespace?

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

static inline list_int list_int_sort(list_int l) {
  int temp;
  size_t interval, i, j;
  for (interval = l.len / 2; interval > 0; interval /= 2) {
    for (i = interval; i < l.len; i += 1) {
      temp = l.buf[i];
      j = i;
      for (; j >= interval && l.buf[j - interval] > temp; j -= interval) {
        l.buf[j] = l.buf[j - interval];
      }
      l.buf[j] = temp;
    }
  }
  return l;
}

static inline void list_int_print(FILE *stream, list_int l) {
  if (!l.len)
    return;

  fprintf(stream, "(%i", l.buf[0]);
  for (size_t i = 1; i < l.len; i++)
    fprintf(stream, ", %i", l.buf[i]);
  fputc(')', stream);
}

static inline int trieTransition_compare(const void *trans1,
                                         const void *trans2) {
  TrieTransition t1 = *(TrieTransition *)trans1, t2 = *(TrieTransition *)trans2;

  if (t1.from < t2.from)
    return -1;
  if (t1.from > t2.from)
    return 1;

  if (t1.c < t2.c)
    return -1;
  if (t1.c > t2.c)
    return 1;

  if (t1.to < t2.to)
    return -1;
  if (t1.to > t2.to)
    return 1;

  return 0;
}

static inline TrieAutomaton createTrieAutomaton(ASTNode *tokast) {

  // Build the trie automaton.
  if (AUT_DEBUG)
    puts("Building the Trie automaton.");

  TrieAutomaton trie;
  trie.trans = list_TrieTransition_new();
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
      if (AUT_DEBUG)
        printf("Current: %c\n", (char)c);

      // Try to find the next state after the transition of c.
      // See if we've already created a transition from where we are in the trie
      // to the next character.
      TrieTransition trans;
      int found = 0;
      for (size_t j = 0; j < trie.trans.len; j++) {
        TrieTransition t = list_TrieTransition_get(&trie.trans, j);
        if ((t.from == prev_state) & (t.c == c)) {
          found = true;
          trans = t;
          if (AUT_DEBUG)
            printf("Found a transition from %i to %i on '%c'.\n", trans.from,
                   trans.to, (char)trans.c);
          break;
        }
      }

      // If the transition hasn't been created, create the transition and the
      // state that it goes to.
      if (!found) {
        trans.from = prev_state;
        trans.to = state_num++;
        trans.c = c;
        list_TrieTransition_add(&trie.trans, trans);
        if (AUT_DEBUG)
          printf("Created a transition from %i to %i on '%c'.\n", trans.from,
                 trans.to, (char)trans.c);
      }

      // If the state we just created is the last character in the string,
      // it's accepting.
      if (i == (cplen - 1)) {
        State s;
        s.rule = rule;
        s.num = trans.to;
        list_State_add(&trie.accepting, s);
        if (AUT_DEBUG)
          printf("Marked %i as an accepting state for rule %s.\n", s.num,
                 identstr);
      }

      // Traverse over the transition.
      prev_state = trans.to;
    } // c in cpstr
  }   // litdef in tokast

  // Sort the trie transitions.
  // First by from, then by to, then by char.
  qsort(trie.trans.buf, trie.trans.len, sizeof(TrieTransition),
        trieTransition_compare);

  if (AUT_DEBUG)
    printf("Finished building the Trie automaton.\n");

  if (AUT_PRINT) {
    TrieAutomaton aut = trie;
    for (size_t i = 0; i < aut.accepting.len; i++) {
      State s = list_State_get(&aut.accepting, i);
      printf("Accepting state: (%i, %s)\n", s.num,
             (char *)s.rule->children[0]->extra);
    }
    for (size_t i = 0; i < aut.trans.len; i++) {
      TrieTransition t = list_TrieTransition_get(&aut.trans, i);
      printf("Transition: (%i, %c) -> (%i)\n", t.from, t.c, t.to);
    }
    fflush(stdout);
  }

  return trie;
}

static inline list_SMAutomaton createSMAutomata(ASTNode *tokast) {
  list_SMAutomaton auts = list_SMAutomaton_new();

  // Now, build SFAs for the state machine definitions.
  for (size_t n = 0; n < tokast->num_children; n++) {
    SMAutomaton aut;
    aut.accepting = list_State_new();
    aut.trans = list_SMTransition_new();

    ASTNode *r = tokast->children[n];
    ASTNode *ident = r->children[0];
    ASTNode *def = r->children[1];
    char *identstr = (char *)ident->extra;

    if (strcmp(def->name, "SMDef") != 0)
      continue;

    if (AUT_DEBUG)
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
      if (AUT_DEBUG)
        printf("Parsing rule %zu.\n", k);

      // For each starting state, make a transition to the end state on the
      // charset.
      SMTransition t;
      t.from = list_int_sort(numset_to_list(startingStates));
      t.to = toState;
      t.act = onCharset;
      list_SMTransition_add(&aut.trans, t);

    } // rule in smdef
    list_SMAutomaton_add(&auts, aut);
  } // automaton for smdef in tokast

  if (AUT_DEBUG)
    printf("Finished building the SM automata.\n");

  if (AUT_PRINT) {
    for (size_t k = 0; k < auts.len; k++) {
      SMAutomaton aut = list_SMAutomaton_get(&auts, k);
      for (size_t i = 0; i < aut.accepting.len; i++) {
        State s = list_State_get(&aut.accepting, i);
        printf("Accepting state: (%i, %s)\n", s.num,
               (char *)s.rule->children[0]->extra);
      }
      for (size_t i = 0; i < aut.trans.len; i++) {
        SMTransition t = list_SMTransition_get(&aut.trans, i);
        printf("Transition: (");
        list_int_print(stdout, t.from);
        printf(", %p) -> (%i)\n", t.act, t.to);
      }
      fflush(stdout);
    }
  }

  return auts;
}

#endif /* PGEN_AUTOMATA_INCLUDE */
