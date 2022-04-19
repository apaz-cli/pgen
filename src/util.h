#ifndef PGEN_INCLUDE_UTIL
#define PGEN_INCLUDE_UTIL
#include <fcntl.h>
#include <limits.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "list.h"
#include "utf8.h"

LIST_DECLARE(String_View);
LIST_DEFINE(String_View);
LIST_DECLARE(Codepoint_String_View);
LIST_DEFINE(Codepoint_String_View);
LIST_DECLARE(size_t);
LIST_DEFINE(size_t);

/*
 * Returns SIZE_MAX on error. There's no worry of that being a legitimate value.
 * Since Stilts only works on 64 bit, we know that size_t is 64 bits.
 * That's over 18 Exabytes.
 */
static inline size_t fileSize(char *filePath) {
  struct stat st;
  return (!stat(filePath, &st)) ? (size_t)st.st_size : SIZE_MAX;
}

/* Open the file and returns its contents in a string. */
/* Returns {.str = NULL, .len = 0} on UTF8 parsing error. */
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
  size_t fsize = fileSize(filePath);
  if (fsize == SIZE_MAX)
    return err;

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
  if (!view.str) {
    Codepoint_String_View cpsv;
    cpsv.str = NULL;
    cpsv.len = view.len;
    return cpsv;
  } else {
    return UTF8_decode_view(view);
  }
}

/* Open the file as utf8, converting to utf32 and returning each line in a list. */
/* Returns an empty list on UTF8 parsing error or OOM. */
/* The list must be destroyed, and list.get(0).str must be freed. */
static inline list_Codepoint_String_View
readFileCodepointLines(char *filePath) {

  // Read the file as codepoints
  Codepoint_String_View target = readFileCodepoints(filePath);

  list_Codepoint_String_View lines = list_Codepoint_String_View_new();
  if (!target.str) return lines; // return empty list on error

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

static inline list_size_t
find_sv_newlines(String_View sv) {
  list_size_t l = list_size_t_new();

  for (size_t i = 0; i < sv.len; i++) {
    if (sv.str[i] == '\n')
      list_size_t_add(&l, i);
  }

  return l;
}

static inline list_size_t
find_cpsv_newlines(Codepoint_String_View cpsv) {
  list_size_t l = list_size_t_new();

  for (size_t i = 0; i < cpsv.len; i++) {
    if (cpsv.str[i] == '\n')
      list_size_t_add(&l, i);
  }

  return l;
}

#endif /* PGEN_INCLUDE_UTIL */
