
#define PL0_TOKENIZER_SOURCEINFO 1
#include "pl0.h"

#include <fcntl.h>
#include <stdio.h>
#include <sys/stat.h>
#include <unistd.h>

typedef struct {
  char *str;
  size_t len;
} String_View;

static inline String_View readFile(char *filePath) {
  String_View err;
  err.str = NULL;
  err.len = 0;
  String_View oom;
  oom.str = NULL;
  oom.len = 1;

  /* Ask for the length of the file */
  struct stat st;
  if (stat(filePath, &st) == -1)
    return err;
  size_t fsize = (size_t)st.st_size;

  /* Open the file */
  int fd = open(filePath, O_RDONLY);
  if (fd == -1)
    return err;

  /* Allocate exactly enough memory. */
  char *buffer = (char *)malloc(fsize + 1);
  if (!buffer)
    return close(fd), oom;

  /* Read the file into a buffer and close it. */
  ssize_t bytes_read = read(fd, buffer, fsize);
  int close_err = close(fd);
  if ((bytes_read != (ssize_t)fsize) | close_err)
    return free(buffer), err;

  /* Write null terminator */
  buffer[fsize] = '\0';

  String_View ret;
  ret.str = buffer;
  ret.len = fsize;
  return ret;
}

int main(void) {
  String_View sv = readFile("pl0.pl0");
  if (!sv.str)
    perror("Could not open example file.");

  codepoint_t *cps = NULL;
  size_t cpslen = 0;
  UTF8_decode(sv.str, sv.len, &cps, &cpslen);
  if ((!cps) | (!cpslen))
    perror("Could not decode any text to parse.");

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
}
