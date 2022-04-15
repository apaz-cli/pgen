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

/*
 * Returns SIZE_MAX on error. There's no worry of that being a legitimate value.
 * Since Stilts only works on 64 bit, we know that size_t is 64 bits.
 * That's over 18 Exabytes.
 */
static inline size_t fileSize(char *filePath) {
  struct stat st;
  return (!stat(filePath, &st)) ? (size_t)st.st_size : SIZE_MAX;
}

static inline String_View readFile(char *filePath) {
  String_View err = {.str = NULL, .len = 0};

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
    return err;
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

  return (String_View){.str = buffer, .len = fsize};
}

static inline void printStringView(String_View sv) {
  fwrite(sv.str, sv.len, 1, stdout);
}

static inline void printCodepointStringView(Codepoint_String_View cpsv) {
  String_View sv = UTF8_encode(cpsv.str, cpsv.len);
  printStringView(sv);
  free(sv.str);
}

/* Open the file, converting to  that it parses as UTF8. */
static inline Codepoint_String_View readFileCodepoints(char *filePath) {
  String_View view = readFile(filePath);
  UTF8State state;
  UTF8_decode_init(&state, view.str, view.len);

  /* Over-allocate enough space. */
  codepoint_t *cps = (codepoint_t *)malloc(sizeof(codepoint_t) * view.len + 1);

  /* Parse */
  size_t cps_read = 0;
  codepoint_t cp;
  Codepoint_String_View sv;
  for (;;) {
    cp = UTF8_decodeNext(&state);

    if (cp == UTF8_ERR) {
      sv.str = NULL;
      sv.len = 0;
      return sv;
    } else if (cp == UTF8_END) {
      break;
    }

    cps[cps_read++] = cp;
  };

  /* Don't resize it at the end. The caller can if they want. */
  sv.str = cps;
  sv.len = view.len;

  free(view.str);
  return sv;
}

/* Open the file, converting to  that it parses as UTF8. */
static inline list_Codepoint_String_View
readFileCodepointLines(char *filePath) {

  // Read the file as codepoints
  Codepoint_String_View target = readFileCodepoints(filePath);

  // Split into lines.
  list_Codepoint_String_View lines = list_Codepoint_String_View_new();
  size_t last_offset = 0;
  for (size_t i = 0; i < target.len; i++) {
    if ((target.str[i] == '\n') | (i == target.len - 1)) {
      Codepoint_String_View sv = {target.str + last_offset, i - last_offset};
      list_Codepoint_String_View_add(&lines, sv);
      last_offset = i;
    }
  }

  return lines;
}

static inline void showHelp(void) {}

#endif /* PGEN_INCLUDE_UTIL */