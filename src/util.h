#ifndef PGEN_INCLUDE_UTIL
#define PGEN_INCLUDE_UTIL
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sys/stat.h>
#include <unistd.h>

#include "list.h"
#include "utf8.h"

LIST_DECLARE(String_View)
LIST_DEFINE(String_View)
LIST_DECLARE(Codepoint_String_View)
LIST_DEFINE(Codepoint_String_View)
LIST_DECLARE(size_t)
LIST_DEFINE(size_t)

/* Open the file and returns its contents in a string. */
/* Returns {.str = NULL, .len = 0} on error, and errno is set to indicate the
 * error. */
/* Returns {.str = NULL, .len = 1} on OOM. */
/* {.str} must be freed. */
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
  size_t fsize = st.st_size;

  /* Open the file */
  int fd = open(filePath, O_RDONLY);
  if (fd == -1)
    return err;

  /* Allocate exactly enough memory. */
  char *buffer = (char *)malloc(fsize + 1);
  if (!buffer) {
    close(fd);
    return oom;
  }

  /* Read the file into a buffer and close it. */
  size_t bytes_read = read(fd, buffer, fsize);
  int close_err = close(fd);
  if ((bytes_read != fsize) | close_err) {
    free(buffer);
    return err;
  }

  /* Write null terminator */
  buffer[fsize] = '\0';

  String_View ret;
  ret.str = buffer;
  ret.len = fsize;
  return ret;
}

static inline size_t cpstrlen(codepoint_t *cpstr) {
  size_t cnt = 0;
  while (*cpstr != '\0') {
    cnt++;
    cpstr++;
  }
  return cnt;
}

static inline bool cpstr_equals(codepoint_t *str1, codepoint_t *str2) {
  while (*str1 && (*str1 == *str2))
    str1++, str2++;
  return *str1 == *str2;
}

static inline void printStringView(String_View sv) {
  fwrite(sv.str, sv.len, 1, stdout);
}

static inline void printCodepointStringView(Codepoint_String_View cpsv) {
  String_View sv = UTF8_encode(cpsv.str, cpsv.len);
  printStringView(sv);
  free(sv.str);
}

/* Open the file as utf8, returning a string of utf32 codepoints. */
/* Returns {.str = NULL, .len = 0} on UTF8 parsing error. */
/* Returns {.str = NULL, .len = 1} on OOM. */
/* {.str} must be freed. */
static inline Codepoint_String_View readFileCodepoints(char *filePath) {
  /* Read the file. */
  String_View view = readFile(filePath);
  Codepoint_String_View cpsv;
  if (!view.str) {
    cpsv.str = NULL;
    cpsv.len = view.len;
    return cpsv;
  } else {
    cpsv = UTF8_decode_view(view);
    free(view.str);
    return cpsv;
  }
}

/* Open the file as utf8, converting to utf32 and returning each line in a list.
 */
/* Returns an empty list on UTF8 parsing error or OOM. */
/* The list must be destroyed, and list.get(0).str must be freed. */
static inline list_Codepoint_String_View
readFileCodepointLines(char *filePath) {

  // Read the file as codepoints
  Codepoint_String_View target = readFileCodepoints(filePath);

  list_Codepoint_String_View lines = list_Codepoint_String_View_new();
  if (!target.str)
    return lines; // return empty list on error

  // Split into lines.
  for (size_t i = 0, last = 0; i < target.len; i++) {
    if ((target.str[i] == '\n') | (i == target.len - 1)) {
      Codepoint_String_View sv;
      sv.str = target.str + last;
      sv.len = i - last;
      list_Codepoint_String_View_add(&lines, sv);

      last = i;
    }
  }

  return lines;
}

static inline list_size_t find_sv_newlines(String_View sv) {
  list_size_t l = list_size_t_new();

  for (size_t i = 0; i < sv.len; i++) {
    if (sv.str[i] == '\n')
      list_size_t_add(&l, i);
  }

  return l;
}

static inline list_size_t find_cpsv_newlines(Codepoint_String_View cpsv) {
  list_size_t l = list_size_t_new();

  for (size_t i = 0; i < cpsv.len; i++) {
    if (cpsv.str[i] == '\n')
      list_size_t_add(&l, i);
  }

  return l;
}

static inline unsigned long long
codepoint_atoull_nosigns(const codepoint_t *a, size_t len, size_t *read) {

  unsigned long long parsing = 0;
  size_t chars = 0;
  for (size_t i = 0; i < len; i++) {
    codepoint_t c = a[i];
    if (!((c >= 48) && (c <= 57)))
      break;
    parsing *= 10; // TODO overflow/underflow checks.
    parsing += (c - 48);
    chars++;
  }

  *read = chars;
  return parsing;
}

static inline int codepoint_atoi(const codepoint_t *a, size_t len,
                                 size_t *read) {

  int neg = a[0] == '-';
  int consumed = (neg | (a[0] == '+'));
  a += consumed;
  len -= consumed;

  size_t ullread;
  unsigned long long ull = codepoint_atoull_nosigns(a, len, &ullread);
  if (neg ? ull > (unsigned long long)-(long long)INT_MIN
          : ull > (unsigned long long)INT_MAX)
    return *read = 0, 0;
  int l = (int)(neg ? -ull : ull);

  *read = ullread + consumed;
  return l;
}

#endif /* PGEN_INCLUDE_UTIL */
