
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
  if (fread(filestr, 1, inputFileLen, inputFile) == -1)
    fprintf(stderr, "Error: Could not read from the file.\n"), exit(1);
  filestr[inputFileLen] = '\0';
  fclose(inputFile);

  *str = filestr;
  *len = inputFileLen;
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
  pl0_tokenizer_init(&tokenizer, "pl0.pl0", cps, cpslen);

  pl0_token tok;
  while ((tok = pl0_nextToken(&tokenizer)).lexeme != PL0_TOK_STREAMEND) {
#if PL0_TOKENIZER_SOURCEINFO
    printf("Token: {.lexeme=%s, .start=%zu, .len=%zu, "
           ".line=%zu, .col=%zu, .sourceFile=\"%s\"}\n",
           pl0_lexeme_name[tok.lexeme], tok.start, tok.len, tok.line, tok.col,
           tok.sourceFile);
#else
    printf("Token: {%s, %zu, %zu}\n", pl0_lexeme_name[tok.lexeme], tok.start,
           tok.len);
#endif
  }

  // Clean up
}
