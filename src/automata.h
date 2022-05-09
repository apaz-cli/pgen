#ifndef PGEN_AUTOMATA_INCLUDE
#define PGEN_AUTOMATA_INCLUDE
#include "util.h"
#include "utf8.h"
#include "list.h"
#include "ast.h"

struct DFANode;
typedef struct DFANode DFANode;

typedef struct {
  ASTNode* charset;
  int from;
  int to;
} DFATransition;
LIST_DECLARE(DFATransition);
LIST_DEFINE(DFATransition);

LIST_DECLARE(int);
LIST_DEFINE(int);

typedef struct {
  list_int accepting_states;
  list_DFATransition transitions;
} NFA;

typedef struct {

} DFA


#endif /* PGEN_AUTOMATA_INCLUDE */
