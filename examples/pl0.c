#define PL0_SOURCEINFO 1
#include "pl0.h"

#include <stdio.h>

static inline void readFile(char *filePath, char **str, size_t *len) {
  long inputFileLen;
  FILE *inputFile;
  char *filestr;

  if (!(inputFile = fopen(filePath, "r")))
    fprintf(stderr, "Error: Could not open %s.\n", filePath), exit(1);
  if (fseek(inputFile, 0, SEEK_END) == -1)
    fprintf(stderr, "Error: Could not seek to end of file.\n"), exit(1);
  if ((inputFileLen = ftell(inputFile)) == -1)
    fprintf(stderr, "Error: Could not check file length.\n"), exit(1);
  if (fseek(inputFile, 0, SEEK_SET) == -1)
    fprintf(stderr, "Error: Could not rewind the file.\n"), exit(1);
  if (!(filestr = (char *)malloc(inputFileLen + 1)))
    fprintf(stderr, "Error: Could not allocate memory.\n"), exit(1);
  if (!fread(filestr, 1, inputFileLen, inputFile))
    fprintf(stderr, "Error: Could not read from the file.\n"), exit(1);
  filestr[inputFileLen] = '\0';
  fclose(inputFile);

  *str = filestr;
  *len = inputFileLen;
}

static inline void printtok(pl0_tokenizer tokenizer, pl0_token tok) {
  printf("Token: (");
  for (size_t i = 0; i < tok.len; i++) {
    codepoint_t c = tok.content[i];
    if (c == '\n')
      printf("\\n");
    else if (c == '\t')
      printf("\\t");
    else if (c == '\r')
      printf("\\r");
    else
      putchar(c);
  }

#if PL0_SOURCEINFO
  printf(") {.kind=%s, .len=%zu, .line=%zu, .col=%zu}\n",
         pl0_tokenkind_name[tok.kind], tok.len, tok.line, tok.col);
#else
  printf(") {.kind=%s, .len=%zu}\n", pl0_tokenkind_name[tok.kind], tok.len);
#endif
}

int main(void) {
  char *input_str = NULL;
  size_t input_len = 0;
  readFile("pl0.pl0", &input_str, &input_len);

  codepoint_t *cps = NULL;
  size_t cpslen = 0;
  if (!UTF8_decode(input_str, input_len, &cps, &cpslen))
    fprintf(stderr, "Could not decode to UTF32.\n"), exit(1);

  pl0_tokenizer tokenizer;
  pl0_tokenizer_init(&tokenizer, cps, cpslen);

  // Define token list
  struct {
    pl0_token *buf;
    size_t size;
    size_t cap;
  } toklist = {(pl0_token *)malloc(sizeof(pl0_token) * 4096), 0, 4096};
  if (!toklist.buf)
    fprintf(stderr, "Out of memory allocating token list.\n"), exit(1);
#define add_tok(t)                                                             \
  do {                                                                         \
    if (toklist.size == toklist.cap) {                                         \
      toklist.buf = realloc(toklist.buf, toklist.cap *= 2);                    \
      if (!toklist.buf)                                                        \
        fprintf(stderr, "Out of memory reallocating token list.\n"), exit(1);  \
    }                                                                          \
    toklist.buf[toklist.size++] = t;                                           \
  } while (0)

  // Parse Tokens
  pl0_token tok;
  do {
    tok = pl0_nextToken(&tokenizer);

    // Discard whitespace and end of stream, add other tokens to the list.
    if (!(tok.kind == PL0_TOK_SLCOM | tok.kind == PL0_TOK_MLCOM |
          tok.kind == PL0_TOK_WS | tok.kind == PL0_TOK_STREAMEND))
      add_tok(tok);

  } while (tok.kind != PL0_TOK_STREAMEND);

  // Print tokens
  /*
  for (size_t i = 0; i < toklist.size; i++)
    printtok(tokenizer, toklist.buf[i]);
  puts("");
  */

  // Init Parser
  pgen_allocator allocator = pgen_allocator_new();
  pl0_parser_ctx parser;
  pl0_parser_ctx_init(&parser, &allocator, toklist.buf, toklist.size);

  // Parse AST
  pl0_astnode_t *ast = pl0_parse_program(&parser);

  // Check for errors
  if (parser.num_errors) {
    for (size_t i = 0; i < parser.num_errors; i++) {
      pl0_parse_err error = parser.errlist[i];
      fprintf(stderr, "An error was encountered on line %zu during parsing:\n"
             "%s\n\n",
             error.line, error.msg);
    }
    exit(1);
  }

  // Print AST
  pl0_astnode_print_json(toklist.buf, ast);

  // Clean up
  pgen_allocator_destroy(&allocator);
  free(toklist.buf);
  free(cps);
  free(input_str);
}
