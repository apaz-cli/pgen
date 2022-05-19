#ifndef PL0_TOKENIZER_INCLUDE
#define PL0_TOKENIZER_INCLUDE
#include "../src/utf8.h"

#ifndef PL0_TOKENIZER_SOURCEINFO
#define PL0_TOKENIZER_SOURCEINFO 1
#endif

#define PL0_TOKENS PL0_TOK_STREAMEND, PL0_TOK_EQ, PL0_TOK_CEQ, PL0_TOK_SEMI, PL0_TOK_DOT, PL0_TOK_COMMA, PL0_TOK_OPEN, PL0_TOK_CLOSE, PL0_TOK_HASH, PL0_TOK_LT, PL0_TOK_LEQ, PL0_TOK_GT, PL0_TOK_GEQ, PL0_TOK_PLUS, PL0_TOK_MINUS, PL0_TOK_STAR, PL0_TOK_DIV, PL0_TOK_VAR, PL0_TOK_PROC, PL0_TOK_CONST, PL0_TOK_BEGIN, PL0_TOK_END, PL0_TOK_IF, PL0_TOK_THEN, PL0_TOK_WHILE, PL0_TOK_DO, PL0_TOK_ODD, PL0_TOK_CALL, PL0_TOK_IDENT, PL0_TOK_NUM, 

typedef enum { PL0_TOKENS } pl0_token_id;
static pl0_token_id _pl0_num_tokids[] = { PL0_TOKENS };
static size_t pl0_num_tokens = (sizeof(_pl0_num_tokids) / sizeof(pl0_token_id)) - 1;

typedef struct {
  pl0_token_id lexeme;
  codepoint_t* start;
  size_t len;
#if PL0_TOKENIZER_SOURCEINFO
  size_t line;
  size_t col;
  char* sourceFile;
#endif
} pl0_token;

typedef struct {
  codepoint_t* start;
  size_t len;
  size_t pos;
#if PL0_TOKENIZER_SOURCEINFO
  size_t pos_line;
  size_t pos_col;
  char* pos_sourceFile;
#endif
} pl0_tokenizer;

static inline void pl0_tokenizer_init(pl0_tokenizer* tokenizer, char* sourceFile, codepoint_t* start, size_t len) {
  tokenizer->start = start;
  tokenizer->len = len;
  tokenizer->pos = 0;
#if PL0_TOKENIZER_SOURCEINFO
  tokenizer->pos_line = 0;
  tokenizer->pos_col = 0;
  tokenizer->pos_sourceFile = sourceFile;
#else
  (void)sourceFile;
#endif
}

static inline pl0_token pl0_nextToken(pl0_tokenizer* tokenizer) {
  codepoint_t* current = tokenizer->start + tokenizer->pos;
  size_t remaining = tokenizer->len - tokenizer->pos;
  pl0_token ret;
#if PL0_TOKENIZER_SOURCEINFO
  ret.line = tokenizer->pos_line;
  ret.col = tokenizer->pos_col;
  ret.sourceFile = tokenizer->pos_sourceFile;
#endif

  int trie_current_state = 0;
  size_t trie_last_accept = 0;
  int smaut_0_current_state = 0;
  size_t smaut_0_last_accept = 0;
  int smaut_1_current_state = 0;
  size_t smaut_1_last_accept = 0;


  for (size_t iidx = 0; iidx < remaining; iidx++) {
    codepoint_t c = current[iidx];

    int all_dead = 1;

    // Trie
    if (trie_current_state != -1) {
      all_dead = 0;

      if (trie_current_state == 0) {
        if (c == 35 /*'#'*/) trie_current_state = 9;
        else if (c == 40 /*'('*/) trie_current_state = 7;
        else if (c == 41 /*')'*/) trie_current_state = 8;
        else if (c == 42 /*'*'*/) trie_current_state = 16;
        else if (c == 43 /*'+'*/) trie_current_state = 14;
        else if (c == 44 /*','*/) trie_current_state = 6;
        else if (c == 45 /*'-'*/) trie_current_state = 15;
        else if (c == 46 /*'.'*/) trie_current_state = 5;
        else if (c == 47 /*'/'*/) trie_current_state = 17;
        else if (c == 58 /*':'*/) trie_current_state = 2;
        else if (c == 59 /*';'*/) trie_current_state = 4;
        else if (c == 60 /*'<'*/) trie_current_state = 10;
        else if (c == 61 /*'='*/) trie_current_state = 1;
        else if (c == 62 /*'>'*/) trie_current_state = 12;
        else if (c == 98 /*'b'*/) trie_current_state = 35;
        else if (c == 99 /*'c'*/) trie_current_state = 30;
        else if (c == 100 /*'d'*/) trie_current_state = 54;
        else if (c == 101 /*'e'*/) trie_current_state = 40;
        else if (c == 105 /*'i'*/) trie_current_state = 43;
        else if (c == 111 /*'o'*/) trie_current_state = 56;
        else if (c == 112 /*'p'*/) trie_current_state = 21;
        else if (c == 116 /*'t'*/) trie_current_state = 45;
        else if (c == 118 /*'v'*/) trie_current_state = 18;
        else if (c == 119 /*'w'*/) trie_current_state = 49;
        else trie_current_state = -1;
      }
      if (trie_current_state == 2) {
        if (c == 61 /*'='*/) trie_current_state = 3;
        else trie_current_state = -1;
      }
      if (trie_current_state == 10) {
        if (c == 61 /*'='*/) trie_current_state = 11;
        else trie_current_state = -1;
      }
      if (trie_current_state == 12) {
        if (c == 61 /*'='*/) trie_current_state = 13;
        else trie_current_state = -1;
      }
      if (trie_current_state == 18) {
        if (c == 97 /*'a'*/) trie_current_state = 19;
        else trie_current_state = -1;
      }
      if (trie_current_state == 19) {
        if (c == 114 /*'r'*/) trie_current_state = 20;
        else trie_current_state = -1;
      }
      if (trie_current_state == 21) {
        if (c == 114 /*'r'*/) trie_current_state = 22;
        else trie_current_state = -1;
      }
      if (trie_current_state == 22) {
        if (c == 111 /*'o'*/) trie_current_state = 23;
        else trie_current_state = -1;
      }
      if (trie_current_state == 23) {
        if (c == 99 /*'c'*/) trie_current_state = 24;
        else trie_current_state = -1;
      }
      if (trie_current_state == 24) {
        if (c == 101 /*'e'*/) trie_current_state = 25;
        else trie_current_state = -1;
      }
      if (trie_current_state == 25) {
        if (c == 100 /*'d'*/) trie_current_state = 26;
        else trie_current_state = -1;
      }
      if (trie_current_state == 26) {
        if (c == 117 /*'u'*/) trie_current_state = 27;
        else trie_current_state = -1;
      }
      if (trie_current_state == 27) {
        if (c == 114 /*'r'*/) trie_current_state = 28;
        else trie_current_state = -1;
      }
      if (trie_current_state == 28) {
        if (c == 101 /*'e'*/) trie_current_state = 29;
        else trie_current_state = -1;
      }
      if (trie_current_state == 30) {
        if (c == 97 /*'a'*/) trie_current_state = 59;
        else if (c == 111 /*'o'*/) trie_current_state = 31;
        else trie_current_state = -1;
      }
      if (trie_current_state == 31) {
        if (c == 110 /*'n'*/) trie_current_state = 32;
        else trie_current_state = -1;
      }
      if (trie_current_state == 32) {
        if (c == 115 /*'s'*/) trie_current_state = 33;
        else trie_current_state = -1;
      }
      if (trie_current_state == 33) {
        if (c == 116 /*'t'*/) trie_current_state = 34;
        else trie_current_state = -1;
      }
      if (trie_current_state == 35) {
        if (c == 101 /*'e'*/) trie_current_state = 36;
        else trie_current_state = -1;
      }
      if (trie_current_state == 36) {
        if (c == 103 /*'g'*/) trie_current_state = 37;
        else trie_current_state = -1;
      }
      if (trie_current_state == 37) {
        if (c == 105 /*'i'*/) trie_current_state = 38;
        else trie_current_state = -1;
      }
      if (trie_current_state == 38) {
        if (c == 110 /*'n'*/) trie_current_state = 39;
        else trie_current_state = -1;
      }
      if (trie_current_state == 40) {
        if (c == 110 /*'n'*/) trie_current_state = 41;
        else trie_current_state = -1;
      }
      if (trie_current_state == 41) {
        if (c == 100 /*'d'*/) trie_current_state = 42;
        else trie_current_state = -1;
      }
      if (trie_current_state == 43) {
        if (c == 102 /*'f'*/) trie_current_state = 44;
        else trie_current_state = -1;
      }
      if (trie_current_state == 45) {
        if (c == 104 /*'h'*/) trie_current_state = 46;
        else trie_current_state = -1;
      }
      if (trie_current_state == 46) {
        if (c == 101 /*'e'*/) trie_current_state = 47;
        else trie_current_state = -1;
      }
      if (trie_current_state == 47) {
        if (c == 110 /*'n'*/) trie_current_state = 48;
        else trie_current_state = -1;
      }
      if (trie_current_state == 49) {
        if (c == 104 /*'h'*/) trie_current_state = 50;
        else trie_current_state = -1;
      }
      if (trie_current_state == 50) {
        if (c == 105 /*'i'*/) trie_current_state = 51;
        else trie_current_state = -1;
      }
      if (trie_current_state == 51) {
        if (c == 108 /*'l'*/) trie_current_state = 52;
        else trie_current_state = -1;
      }
      if (trie_current_state == 52) {
        if (c == 101 /*'e'*/) trie_current_state = 53;
        else trie_current_state = -1;
      }
      if (trie_current_state == 54) {
        if (c == 111 /*'o'*/) trie_current_state = 55;
        else trie_current_state = -1;
      }
      if (trie_current_state == 56) {
        if (c == 100 /*'d'*/) trie_current_state = 57;
        else trie_current_state = -1;
      }
      if (trie_current_state == 57) {
        if (c == 100 /*'d'*/) trie_current_state = 58;
        else trie_current_state = -1;
      }
      if (trie_current_state == 59) {
        if (c == 108 /*'l'*/) trie_current_state = 60;
        else trie_current_state = -1;
      }
      if (trie_current_state == 60) {
        if (c == 108 /*'l'*/) trie_current_state = 61;
        else trie_current_state = -1;
      }
    }

    // State Machines
    if (smaut_0_current_state != -1) {
      all_dead = 0;
      
    }

    if (smaut_1_current_state != -1) {
      all_dead = 0;
      
    }

    if (all_dead)
      break;
  }

  return ret;
}

#endif /* PL0_TOKENIZER_INCLUDE */
