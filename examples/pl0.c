
#define PL0_TOKENIZER_SOURCEINFO 1
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
  if (!(filestr = malloc(inputFileLen + 1)))
    fprintf(stderr, "Error: Could not allocate memory.\n"), exit(1);
  if (!fread(filestr, 1, inputFileLen, inputFile))
    fprintf(stderr, "Error: Could not read from the file.\n"), exit(1);
  filestr[inputFileLen] = '\0';
  fclose(inputFile);

  *str = filestr;
  *len = inputFileLen;
}

static inline void skipWhitespace(pl0_tokenizer *tokenizer) {

  // Eat all the whitespace you can.
  while (1) {
    // Consumed the end of the file
    if (tokenizer->pos >= tokenizer->len) break;

    codepoint_t c1 = tokenizer->start[tokenizer->pos];
    size_t remaining = tokenizer->len - tokenizer->pos;
    codepoint_t c2 = remaining ? tokenizer->start[tokenizer->pos + 1] : '\0';

    // Whitespace
    if ((c1 == ' ') | (c1 == '\t') | (c1 == '\r') | (c1 == '\n')) {
      tokenizer->pos++;
    }
    // Single line comment
    else if ((c1 == '/') & (c2 == '/')) {
      tokenizer->pos += 2;
      while (tokenizer->pos >= tokenizer->len && c1 != '\n')
        tokenizer->pos++;
    }
    // Multi line comment
    else if ((c1 == '/') & (c2 == '*')) {
      // Consume characters until */ or EOF
      while (!((c1 == '*') & (c2 == '/'))) {
        tokenizer->pos++;
        c1 = c2;
        if (tokenizer->pos < tokenizer->len)
          c2 = tokenizer->start[tokenizer->pos];
        else
          break;
      }
    } else // No more whitespace to munch
      break;
  }
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
  pl0_tokenizer_init(&tokenizer, cps, cpslen, "pl0.pl0");

  pl0_token tok;
  do {
    // skipWhitespace(&tokenizer);
    tok = pl0_nextToken(&tokenizer);

#if PL0_TOKENIZER_SOURCEINFO
    printf("Token: {.lexeme=%s, .start=%zu, .len=%zu, "
           ".line=%zu, .col=%zu, .sourceFile=\"%s\"}\n",
           pl0_lexeme_name[tok.lexeme], tok.start, tok.len, tok.line, tok.col,
           tok.sourceFile);
#else
    printf("Token: {%s, %zu, %zu}\n", pl0_lexeme_name[tok.lexeme], tok.start,
           tok.len);
#endif
    for (size_t i = 0; i < tok.len; i++) {
      putchar(*(tokenizer.start + tok.start + i));
    }puts("");
  } while (tok.lexeme != PL0_TOK_STREAMEND);

  // Clean up
}
