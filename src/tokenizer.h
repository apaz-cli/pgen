#ifndef TOKENIZER_INCLUDED
#define TOKENIZER_INCLUDED
#include "util.h"

#define PGEN_TOKENS                                                            \
  TOK_END, TOK_DOT, TOK_OPEN, TOK_CLOSE, TOK_LBRACK, TOK_RBRACK, TOK_PLUS,     \
      TOK_STAR, TOK_QUESTION, TOK_NOT, TOK_AND, TOK_SLASH, TOK_LEFTARROW,      \
      TOK_STRLIT, TOK_CHARCLASS, TOK_TOKIDENT, TOK_RULEIDENT,

typedef enum { PGEN_TOKENS } pgen_token_id;

static pgen_token_id _pgen_num_toktypes[] = {PGEN_TOKENS};
static size_t pgen_num_toktypes =
    sizeof(_pgen_num_toktypes) / sizeof(pgen_token_id) - 1;

#undef PGEN_TOKENS

typedef struct {
  pgen_token_id lexeme;
  Codepoint_String_View view;
  size_t line;
  size_t col;
  char *sourceFile;
} pgen_token;

LIST_DECLARE(pgen_token);
LIST_DEFINE(pgen_token);

typedef struct {
  
} pgen_tokenizer_dfa;

typedef struct {
  char *filePath;
  Codepoint_String_View input;
  size_t resume_at;
} pgen_tokenizer;

static inline pgen_tokenizer *pgen_tokenizer_init(pgen_tokenizer *tokenizer,
                                                  char *filePath,
                                                  Codepoint_String_View input) {
  if (!input.str && input.len == 0)
    ERROR("Error opening file: %s. Does it exist?", filePath);
  else if (!input.str && input.len == 1)
    ERROR("Error opening file: %s. Out of memory.", filePath);

  tokenizer->filePath = filePath;
  tokenizer->input = input;
  tokenizer->resume_at = 0;

  return tokenizer;
}

static inline pgen_tokenizer *
pgen_tokenizer_read_init(pgen_tokenizer *tokenizer, char *filePath) {
  return pgen_tokenizer_init(tokenizer, filePath, readFileCodepoints(filePath));
}

static inline pgen_token pgen_tokenizer_next(pgen_tokenizer *tokenizer) {
  // Create a view of the current codepoint forward.
  Codepoint_String_View view;
  view.str = tokenizer->input.str + tokenizer->resume_at;
  view.len = tokenizer->input.len - tokenizer->resume_at;
  
  // Exec DFA
  pgen_token tok;

  // TODO remove
  tok.lexeme = TOK_END;
  tok.view.str = NULL;
  tok.view.len = 0;
  tok.sourceFile = NULL;
  tok.line = 0;
  tok.col = 0;
  return tok;
}

static inline list_pgen_token pgen_tokenize_all(char *filePath) {
  pgen_tokenizer tokenizer;
  pgen_tokenizer_init(&tokenizer, filePath, readFileCodepoints(filePath));

  list_pgen_token output = list_pgen_token_new();

  return output;
}

#endif /* TOKENIZER_INCLUDED */
