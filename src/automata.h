#ifndef PGEN_AUTOMATA_INCLUDE
#define PGEN_AUTOMATA_INCLUDE
#include "ast.h"
#include "list.h"
#include "utf8.h"
#include "util.h"

#define AUT_PRINT 0
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

typedef struct {
  int f;
  int s;
} StateRange;

typedef struct {
  codepoint_t f;
  codepoint_t s;
} CharRange;

LIST_DECLARE(int)
LIST_DEFINE(int)
LIST_DECLARE(State)
LIST_DEFINE(State)
LIST_DECLARE(TrieTransition)
LIST_DEFINE(TrieTransition)
LIST_DECLARE(StateRange)
LIST_DEFINE(StateRange)
LIST_DECLARE(CharRange)
LIST_DEFINE(CharRange)

typedef struct {
  list_CharRange on;
  list_StateRange from;
  int to;
  bool inverted;
} SMTransition;

LIST_DECLARE(SMTransition)
LIST_DEFINE(SMTransition)

typedef struct {
  list_State accepting;
  list_TrieTransition trans;
} TrieAutomaton;

typedef struct {
  char *ident;
  list_StateRange accepting;
  list_SMTransition trans;
} SMAutomaton;

LIST_DECLARE(TrieAutomaton)
LIST_DEFINE(TrieAutomaton)
LIST_DECLARE(SMAutomaton)
LIST_DEFINE(SMAutomaton)

static inline void print_stateranges(list_StateRange l) {
  printf("[");
  for (size_t i = 0; i < l.len; i++) {
    if (i)
      printf(", ");
    printf("[%i, %i]", l.buf[i].f, l.buf[i].s);
  }
  printf("]\n");
}

static const char numset_failstr[] =
    "Error: -1 cannot be used as a state in .tok files. "
    "It's the failure state.";

static inline StateRange get_num_staterange(ASTNode *num) {
  int n = *(int *)num->extra;
  if (n == -1)
    ERROR(numset_failstr);
  return (StateRange){n, n};
}

static inline StateRange get_numrange_staterange(ASTNode *numrange) {
  int *iptr = (int *)numrange->extra;
  int first = *iptr, second = *(iptr + 1);
  if (first == -1 || second == -1)
    ERROR(numset_failstr);
  return (StateRange){first, second};
}

static inline void get_numsetlist_stateranges(ASTNode *numsetlist,
                                              list_StateRange *l) {
  for (size_t i = 0; i < numsetlist->num_children; i++) {
    ASTNode *cld = numsetlist->children[i];
    if (!strcmp(cld->name, "Num")) {
      StateRange r = get_num_staterange(cld);
      list_StateRange_add(l, r);
    } else if (!strcmp(cld->name, "NumRange")) {
      StateRange r = get_numrange_staterange(cld);
      list_StateRange_add(l, r);
    } else if (!strcmp(cld->name, "NumSetList")) {
      get_numsetlist_stateranges(cld, l);
    } else
      ERROR("%s is not a numset.", cld->name);
  }
}

static inline list_StateRange numset_to_stateranges(ASTNode *numset) {
  list_StateRange l = list_StateRange_new();
  if (strcmp(numset->name, "Num") == 0) {
    list_StateRange_add(&l, get_num_staterange(numset));
  } else if (strcmp(numset->name, "NumRange") == 0) {
    list_StateRange_add(&l, get_numrange_staterange(numset));
  } else if (strcmp(numset->name, "NumSetList") == 0) {
    get_numsetlist_stateranges(numset, &l);
  } else
    ERROR("%s is not a numset.", numset->name);
  return l;
}

static inline list_StateRange compressStateRanges(list_StateRange srs) {

  if (srs.len <= 1)
    return srs;

  // Shellsort ranges by the first entry.
  StateRange temp;
  for (size_t interval = srs.len / 2; interval > 0; interval /= 2) {
    for (size_t i = interval; i < srs.len; i += 1) {
      temp = srs.buf[i];
      size_t j = i;
      for (; j >= interval && srs.buf[j - interval].f > temp.f; j -= interval)
        srs.buf[j] = srs.buf[j - interval];
      srs.buf[j] = temp;
    }
  }

  // Compress
  size_t ins = 0;
  int min = srs.buf[0].f;
  int max = srs.buf[0].s;
  for (size_t i = 1; i < srs.len; i++) {
    if (srs.buf[i].f > max + 1) {
      srs.buf[ins++] = (StateRange){min, max};
      min = srs.buf[i].f;
      max = srs.buf[i].s;
    } else {
      max = MAX(max, srs.buf[i].s);
    }
  }
  srs.buf[ins++] = (StateRange){min, max};
  srs.len = ins;

  return srs;
}

static inline void print_charranges(list_CharRange l) {
  printf("[");
  for (size_t i = 0; i < l.len; i++) {
    CharRange range = l.buf[i];
    i ? printf(", ") : 1;
    printf("[%" PRI_CODEPOINT ", %" PRI_CODEPOINT "]", range.f, range.s);
  }
  printf("]\n");
}

static inline list_CharRange compressCharRanges(list_CharRange crs) {

  if (crs.len <= 1)
    return crs;

  // Shellsort ranges by the first entry.
  CharRange temp;
  for (size_t interval = crs.len / 2; interval > 0; interval /= 2) {
    for (size_t i = interval; i < crs.len; i += 1) {
      temp = crs.buf[i];
      size_t j = i;
      for (; j >= interval && crs.buf[j - interval].f > temp.f; j -= interval)
        crs.buf[j] = crs.buf[j - interval];
      crs.buf[j] = temp;
    }
  }

  // Compress
  size_t ins = 0;
  codepoint_t min = crs.buf[0].f;
  codepoint_t max = crs.buf[0].s;
  for (size_t i = 1; i < crs.len; i++) {
    if (crs.buf[i].f > max + 1) {
      crs.buf[ins++] = (CharRange){min, max};
      min = crs.buf[i].f;
      max = crs.buf[i].s;
    } else {
      max = MAX(max, crs.buf[i].s);
    }
  }
  crs.buf[ins++] = (CharRange){min, max};
  crs.len = ins;

  return crs;
}

static inline list_CharRange charset_to_charranges(ASTNode *charset,
                                                   bool *inverted) {
  // If the charset is a single quoted char, then char->num_children is 0 and
  // char->extra is that character's codepoint. If the charset is of
  // the form [^? ...] then charset->extra is a bool* of whether
  // the ^ is present, and charset->children has the range's contents.
  // The children are of the form char->extra = codepoint or
  // charrange->extra = codepoint[2]. A charset of the
  // form [^? ...] may have no children.

  list_CharRange l = list_CharRange_new();

  if (!strcmp(charset->name, "Char")) {
    codepoint_t c = *(codepoint_t *)charset->extra;
    list_CharRange_add(&l, (CharRange){c, c});
    return *inverted = false, l;
  }

  if (!strcmp(charset->name, "CharSet")) {
    for (size_t i = 0; i < charset->num_children; i++) {
      ASTNode *cld = charset->children[i];
      if (!strcmp(cld->name, "CharRange")) {
        codepoint_t *cpptr = (codepoint_t *)cld->extra;
        list_CharRange_add(&l, (CharRange){cpptr[0], cpptr[1]});
      } else {
        codepoint_t cp = *(codepoint_t *)cld->extra;
        list_CharRange_add(&l, (CharRange){cp, cp});
      }
    }
    return *inverted = *(bool *)charset->extra ? true : false, l;
  }

  ERROR("UNREACHABLE CODE!");
  return l;
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

static inline TrieAutomaton createTrieAutomaton(list_ASTNodePtr tokdefs) {

  // Build the trie automaton.
  if (AUT_DEBUG)
    puts("Building the Trie automaton.");

  TrieAutomaton trie;
  trie.trans = list_TrieTransition_new();
  trie.accepting = list_State_new();
  // For each token literal in the AST, add it as a path to the trie.
  int state_num = 1; // 0 is the trie root.
  for (size_t n = 0; n < tokdefs.len; n++) {

    ASTNode *rule = tokdefs.buf[n];
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
    printf("Finished building trie.\n");
    fflush(stdout);
  }

  return trie;
}

static inline list_SMAutomaton createSMAutomata(list_ASTNodePtr tokdefs) {
  list_SMAutomaton auts = list_SMAutomaton_new();

  // Now, build SFAs for the state machine definitions.
  for (size_t n = 0; n < tokdefs.len; n++) {
    SMAutomaton aut;
    aut.accepting = list_StateRange_new();
    aut.trans = list_SMTransition_new();

    ASTNode *r = tokdefs.buf[n];
    ASTNode *ident = r->children[0];
    ASTNode *def = r->children[1];
    char *identstr = (char *)ident->extra;
    aut.ident = identstr;

    if (strcmp(def->name, "SMDef") != 0)
      continue;

    if (AUT_DEBUG)
      printf("Building automaton for %s.\n", identstr);

    // Grab the accepting states.
    ASTNode *accepting = def->children[0];
    aut.accepting = numset_to_stateranges(accepting);

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
        printf("Parsing transition rule %zu\n", k);

      // For each starting state, make a transition to the end state on the
      // charset.
      SMTransition t;
      bool inv;
      t.from = compressStateRanges(numset_to_stateranges(startingStates));
      t.to = toState;
      t.on = charset_to_charranges(onCharset, &inv);
      t.inverted = inv;
      list_SMTransition_add(&aut.trans, t);

      if (AUT_DEBUG) {
        printf("  from: \n");
        printf("  on:   \n");
        printf("  to:   \n");
      }
    } // rule in smdef
    list_SMAutomaton_add(&auts, aut);
  } // automaton for smdef in tokast

  if (AUT_DEBUG)
    printf("Finished building the SM automata.\n\n");

  if (AUT_PRINT) {
    for (size_t k = 0; k < auts.len; k++) {
      SMAutomaton aut = list_SMAutomaton_get(&auts, k);

      printf("Accepting states for smaut %zu: ", k);
      // list_int_print(stdout, aut.accepting);
      list_StateRange l = aut.accepting;
      printf("((%i, %i)", l.buf[0].f, l.buf[0].s);
      for (size_t i = 1; i < l.len; i++)
        printf(", (%i, %i)", l.buf[i].f, l.buf[i].s);
      fputc(')', stdout);

      puts("");
      for (size_t i = 0; i < aut.trans.len; i++) {
        SMTransition t = list_SMTransition_get(&aut.trans, i);
        printf("Transition: ");
        for (size_t z = 0; z < t.from.len; z++) {
          if (!z)
            printf("(");
          else
            printf(", ");

          for (size_t i = 1; i < t.from.len; i++) {
            int f = t.from.buf[i].f, s = t.from.buf[i].s;
            if (f != s)
              printf("%i", f);
            else
              printf("(%i, %i)", f, s);
          }

          if (z == t.from.len - 1)
            printf(")");
          printf(" -> (%i)\n", t.to);
        }
        fflush(stdout);
      }
    }
  }

  return auts;
}

static inline void destroyTrieAutomaton(TrieAutomaton trie) {
  list_TrieTransition_clear(&trie.trans);
  list_State_clear(&trie.accepting);
}

static inline void destroySMAutomata(list_SMAutomaton smauts) {
  for (size_t i = 0; i < smauts.len; i++) {
    list_StateRange_clear(&(smauts.buf[i].accepting));
    list_SMTransition trans = smauts.buf[i].trans;
    for (size_t j = 0; j < trans.len; j++) {
      list_StateRange_clear(&trans.buf[j].from);
      list_CharRange_clear(&trans.buf[j].on);
    }
    list_SMTransition_clear(&trans);
  }
  list_SMAutomaton_clear(&smauts);
}

#endif /* PGEN_AUTOMATA_INCLUDE */
