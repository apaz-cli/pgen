#ifndef PGEN_INCLUDE_UTIL
#define PGEN_INCLUDE_UTIL
#include <fcntl.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>

typedef struct {
  char *str;
  size_t len;
} String_View;

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

#endif /* PGEN_INCLUDE_UTIL */