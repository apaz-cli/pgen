#ifndef NTUI_LIST_INCLUDE
#define NTUI_LIST_INCLUDE
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

/**********/
/* Macros */
/**********/

#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define MAX(a, b) ((a) < (b) ? (b) : (a))
#define OOM()                                                                  \
  do {                                                                         \
    fprintf(stderr, "pgen has run out of memory.\n");                          \
    exit(1);                                                                   \
  } while (0)
#define ERROR(...)                                                             \
  do {                                                                         \
    fprintf(stderr, "Error:\n");                                               \
    fprintf(stderr, __VA_ARGS__);                                              \
    fprintf(stderr, "\n");                                                     \
    exit(1);                                                                   \
  } while (0)

/********/
/* List */
/********/

#define LIST_DECLARE(type)                                                     \
  typedef struct {                                                             \
    type *buf;                                                                 \
    size_t len;                                                                \
    size_t cap;                                                                \
  } list_##type;

#define LIST_DEFINE(type)                                                      \
  static inline list_##type list_##type##_new() {                              \
    list_##type nl;                                                            \
    nl.buf = NULL;                                                             \
    nl.len = 0;                                                                \
    nl.cap = 0;                                                                \
    return nl;                                                                 \
  }                                                                            \
  static inline int list_##type##_add(list_##type *self, type item) {          \
    size_t next_len = self->len + 1;                                           \
    /* Grow the buffer if there's not enough space. */                         \
    if (self->cap <= next_len) {                                               \
      size_t next_cap = next_len * 2 + 8;                                      \
      type *reall = (type *)realloc(self->buf, sizeof(type) * next_cap);       \
      if (!reall)                                                              \
        return 1;                                                              \
      self->cap = next_cap;                                                    \
      self->buf = reall;                                                       \
    }                                                                          \
    /* Insert */                                                               \
    self->buf[self->len] = item;                                               \
    self->len = next_len;                                                      \
    return 0;                                                                  \
  }                                                                            \
  static inline type list_##type##_get(list_##type *self, size_t idx) {        \
    if (!self)                                                                 \
      ERROR("List is null.");                                                  \
    if ((idx >= self->len) | (idx < 0))                                        \
      ERROR("List index out of range.");                                       \
    return self->buf[idx];                                                     \
  }                                                                            \
  static inline type list_##type##_remove(list_##type *self, size_t idx) {     \
    if (!self)                                                                 \
      ERROR("List is null.");                                                  \
    if ((idx >= self->len) | (idx < 0))                                        \
      ERROR("List index out of range.");                                       \
    type ret = self->buf[idx];                                                 \
    size_t nlen = self->len ? self->len - 1 : 0;                               \
    for (size_t i = idx; i < nlen; i++)                                        \
      self->buf[i] = self->buf[i + 1];                                         \
    self->len = nlen;                                                          \
    return ret;                                                                \
  }                                                                            \
  static inline bool list_##type##_isEmpty(list_##type *self) {                \
    return self->len == 0;                                                     \
  }                                                                            \
  static inline void list_##type##_clear(list_##type *self) {                  \
    if (self->buf)                                                             \
      free(self->buf);                                                         \
    self->buf = NULL;                                                          \
    self->len = 0;                                                             \
    self->cap = 0;                                                             \
  }

#endif /* NTUI_LIST_INCLUDE */
