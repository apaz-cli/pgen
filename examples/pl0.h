#ifndef PGEN_PL0_PARSER_H
#define PGEN_PL0_PARSER_H


/* START OF UTF8 LIBRARY */

#ifndef UTF8_INCLUDED
#define UTF8_INCLUDED
#include <inttypes.h>
#include <stdint.h>
#include <stdlib.h>
#include <limits.h>

#define UTF8_END (char)(CHAR_MIN ? CHAR_MIN     : CHAR_MAX    ) /* 1111 1111 */
#define UTF8_ERR (char)(CHAR_MIN ? CHAR_MIN + 1 : CHAR_MAX - 1) /* 1111 1110 */

#ifndef UTF8_MALLOC
#define UTF8_MALLOC malloc
#endif

#ifndef UTF8_FREE
#define UTF8_FREE free
#endif

typedef int32_t codepoint_t;
#define PRI_CODEPOINT PRIu32

typedef struct {
  char *start;
  size_t pos;
  size_t len;
} UTF8Decoder;

static inline void UTF8_decoder_init(UTF8Decoder *state, char *str,
                                     size_t len) {
  state->start = str;
  state->pos = 0;
  state->len = len;
}

static inline char UTF8_nextByte(UTF8Decoder *state) {
  char c;
  if (state->pos >= state->len)
    return UTF8_END;
  c = state->start[state->pos++];
  return c;
}

static inline char UTF8_contByte(UTF8Decoder *state) {
  char c;
  c = UTF8_nextByte(state);
  return ((c & 0xC0) == 0x80) ? (c & 0x3F) : UTF8_ERR;
}

static inline int UTF8_validByte(char c) {
  return (c != UTF8_ERR) & (c != UTF8_END);
}

/* Extract the next unicode code point. Returns the codepoint, UTF8_END, or
 * UTF8_ERR. */
static inline codepoint_t UTF8_decodeNext(UTF8Decoder *state) {
  codepoint_t c;
  char c0, c1, c2, c3;

  if (state->pos >= state->len)
    return state->pos == state->len ? UTF8_END : UTF8_ERR;

  c0 = UTF8_nextByte(state);

  if ((c0 & 0x80) == 0) {
    return (codepoint_t)c0;
  } else if ((c0 & 0xE0) == 0xC0) {
    c1 = UTF8_contByte(state);
    if (UTF8_validByte(c1)) {
      c = ((c0 & 0x1F) << 6) | c1;
      if (c >= 128)
        return c;
    }
  } else if ((c0 & 0xF0) == 0xE0) {
    c1 = UTF8_contByte(state);
    c2 = UTF8_contByte(state);
    if (UTF8_validByte(c1) & UTF8_validByte(c2)) {
      c = ((c0 & 0x0F) << 12) | (c1 << 6) | c2;
      if ((c >= 2048) & ((c < 55296) | (c > 57343)))
        return c;
    }
  } else if ((c0 & 0xF8) == 0xF0) {
    c1 = UTF8_contByte(state);
    c2 = UTF8_contByte(state);
    c3 = UTF8_contByte(state);
    if (UTF8_validByte(c1) & UTF8_validByte(c2) & UTF8_validByte(c3)) {
      c = ((c0 & 0x07) << 18) | (c1 << 12) | (c2 << 6) | c3;
      if ((c >= 65536) & (c <= 1114111))
        return c;
    }
  }
  return UTF8_ERR;
}

/*
 * Encodes the codepoint as utf8 into the buffer, and returns the number of
 * characters written. If the codepoint is invalid, nothing is written and zero
 * is returned.
 */
static inline size_t UTF8_encodeNext(codepoint_t codepoint, char *buf4) {
  if (codepoint <= 0x7F) {
    buf4[0] = (char)codepoint;
    return 1;
  } else if (codepoint <= 0x07FF) {
    buf4[0] = (char)(((codepoint >> 6) & 0x1F) | 0xC0);
    buf4[1] = (char)(((codepoint >> 0) & 0x3F) | 0x80);
    return 2;
  } else if (codepoint <= 0xFFFF) {
    buf4[0] = (char)(((codepoint >> 12) & 0x0F) | 0xE0);
    buf4[1] = (char)(((codepoint >> 6) & 0x3F) | 0x80);
    buf4[2] = (char)(((codepoint >> 0) & 0x3F) | 0x80);
    return 3;
  } else if (codepoint <= 0x10FFFF) {
    buf4[0] = (char)(((codepoint >> 18) & 0x07) | 0xF0);
    buf4[1] = (char)(((codepoint >> 12) & 0x3F) | 0x80);
    buf4[2] = (char)(((codepoint >> 6) & 0x3F) | 0x80);
    buf4[3] = (char)(((codepoint >> 0) & 0x3F) | 0x80);
    return 4;
  }
  return 0;
}

/*
 * Convert UTF32 codepoints to a utf8 string.
 * This will UTF8_MALLOC() a buffer large enough, and store it to retstr and its
 * length to retlen. The result is not null terminated.
 * Returns 1 on success, 0 on failure. Cleans up the buffer and does not store
 * to retstr or retlen on failure.
 */
static inline int UTF8_encode(codepoint_t *codepoints, size_t len,
                              char **retstr, size_t *retlen) {
  char buf4[4];
  size_t characters_used, used, i, j;
  char *out_buf, *new_obuf;

  if ((!codepoints) | (!len))
    return 0;
  if (!(out_buf = (char *)UTF8_MALLOC(len * sizeof(codepoint_t) + 1)))
    return 0;

  characters_used = 0;
  for (i = 0; i < len; i++) {
    if (!(used = UTF8_encodeNext(codepoints[i], buf4)))
      return UTF8_FREE(out_buf), 0;
    for (j = 0; j < used; j++)
      out_buf[characters_used++] = buf4[j];
  }

  out_buf[characters_used] = '\0';
  *retstr = out_buf;
  *retlen = characters_used;
  return 1;
}

/*
 * Convert a UTF8 string to UTF32 codepoints.
 * This will UTF8_MALLOC() a buffer large enough, and store it to retstr and its
 * length to retcps. The result is not null terminated.
 * Returns 1 on success, 0 on failure. Cleans up the buffer and does not store
 * to retcps or retlen on failure.
 * Also, if map is not null, UTF8_MALLOC()s and a pointer to a list of the
 * position of the beginning of each utf8 codepoint in str to map. There are
 * retlen many of them. Cleans up and does not store the list to map on failure.
 */
static inline int UTF8_decode_map(char *str, size_t len, codepoint_t **retcps,
                                  size_t *retlen, size_t **map) {

  UTF8Decoder state;
  codepoint_t *cpbuf, cp;
  size_t cps_read = 0;

  if ((!str) | (!len))
    return 0;
  if (!(cpbuf = (codepoint_t *)UTF8_MALLOC(sizeof(codepoint_t) * len)))
    return 0;

  size_t *mapbuf = NULL;
  if (map) {
    mapbuf = (size_t *)UTF8_MALLOC(sizeof(size_t) * len);
    if (!mapbuf) {
      free(cpbuf);
      return 0;
    }
  }

  UTF8_decoder_init(&state, str, len);
  for (;;) {
    size_t prepos = state.pos;
    cp = UTF8_decodeNext(&state);
    if ((cp == UTF8_ERR) | (cp == UTF8_END))
      break;
    if (mapbuf)
      mapbuf[cps_read] = prepos;
    cpbuf[cps_read] = cp;
    cps_read++;
  }

  if (cp == UTF8_ERR) {
    UTF8_FREE(cpbuf);
    if (mapbuf)
      UTF8_FREE(mapbuf);
    return 0;
  }

  if (mapbuf)
    *map = mapbuf;
  *retcps = cpbuf;
  *retlen = cps_read;
  return 1;
}

/*
 * Convert a UTF8 string to UTF32 codepoints.
 * This will UTF8_MALLOC() a buffer large enough, and store it to retstr and its
 * length to retcps. The result is not null terminated.
 * Returns 1 on success, 0 on failure. Cleans up the buffer and does not store
 * to retcps or retlen on failure.
 */
static inline int UTF8_decode(char *str, size_t len, codepoint_t **retcps,
                              size_t *retlen) {
  return UTF8_decode_map(str, len, retcps, retlen, NULL);
}

#endif /* UTF8_INCLUDED */

/* END OF UTF8 LIBRARY */


#ifndef PGEN_INTERACTIVE
#define PGEN_INTERACTIVE 0

#define PGEN_ALLOCATOR_DEBUG 0

#endif /* PGEN_INTERACTIVE */


/* START OF AST ALLOCATOR */

#ifndef PGEN_ARENA_INCLUDED
#define PGEN_ARENA_INCLUDED
#include <stdbool.h>
#include <limits.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define PGEN_ALIGNMENT _Alignof(max_align_t)
#define PGEN_BUFFER_SIZE (PGEN_PAGESIZE * 1024)
#define PGEN_NUM_ARENAS 256
#define PGEN_NUM_FREELIST 256

#ifndef PGEN_PAGESIZE
#define PGEN_PAGESIZE 4096
#endif

#ifndef PGEN_MALLOC
#define PGEN_MALLOC malloc
#endif

#ifndef PGEN_FREE
#define PGEN_FREE free
#endif

#ifndef PGEN_OOM
#define PGEN_OOM()                                                             \
  do {                                                                         \
    fprintf(stderr, "Parser out of memory on line %i in %s in %s.\n",          \
            __LINE__, __func__, __FILE__);                                     \
    exit(1);                                                                   \
  } while (0);
#endif

#ifndef PGEN_DEBUG
#define PGEN_DEBUG 0
#endif

#ifndef PGEN_ALLOCATOR_DEBUG
#define PGEN_ALLOCATOR_DEBUG 0
#endif

#if SIZE_MAX < UINT32_MAX
#define PGEN_SIZE_RANGE_CHECK
#endif

#if __STDC_VERSION__ >= 201112L
_Static_assert((PGEN_ALIGNMENT % 2) == 0,
               "Why would alignof(max_align_t) be odd? WTF?");
_Static_assert(PGEN_BUFFER_SIZE <= UINT32_MAX,
               "The arena buffer size must fit in uint32_t.");
#endif

static inline size_t pgen_align(size_t n, size_t align) {
  if (align == 1)
    return n;
  return (n + align - 1) & -align;
}

typedef struct {
  void (*freefn)(void *ptr);
  char *buf;
  uint32_t cap;
} pgen_arena_t;

typedef struct {
  uint32_t arena_idx;
  uint32_t filled;
} pgen_allocator_rewind_t;

typedef struct {
  pgen_allocator_rewind_t arew;
  size_t prew;
} pgen_parser_rewind_t;

#define PGEN_REWIND_START ((pgen_allocator_rewind_t){{0, 0}, 0})

typedef struct {
  void (*freefn)(void *);
  void *ptr;
  pgen_allocator_rewind_t rew;
} pgen_freelist_entry_t;

typedef struct {
  uint32_t len;
  uint32_t cap;
  pgen_freelist_entry_t *entries;
} pgen_freelist_t;

typedef struct {
  pgen_allocator_rewind_t rew;
  pgen_arena_t arenas[PGEN_NUM_ARENAS];
  pgen_freelist_t freelist;
} pgen_allocator;

static inline pgen_allocator pgen_allocator_new(void) {
  pgen_allocator alloc;

  alloc.rew.arena_idx = 0;
  alloc.rew.filled = 0;

  for (size_t i = 0; i < PGEN_NUM_ARENAS; i++) {
    alloc.arenas[i].freefn = NULL;
    alloc.arenas[i].buf = NULL;
    alloc.arenas[i].cap = 0;
  }

  alloc.freelist.entries = (pgen_freelist_entry_t *)PGEN_MALLOC(
      sizeof(pgen_freelist_entry_t) * PGEN_NUM_FREELIST);
  if (alloc.freelist.entries) {
    alloc.freelist.cap = PGEN_NUM_FREELIST;
    alloc.freelist.len = 0;
  } else {
    alloc.freelist.cap = 0;
    alloc.freelist.len = 0;
    PGEN_OOM();
  }

  return alloc;
}

static inline int pgen_allocator_launder(pgen_allocator *allocator,
                                         pgen_arena_t arena) {
  for (size_t i = 0; i < PGEN_NUM_ARENAS; i++) {
    if (!allocator->arenas[i].buf) {
      allocator->arenas[i] = arena;
      return 1;
    }
  }
  return 0;
}

static inline void pgen_allocator_destroy(pgen_allocator *allocator) {
  // Free all the buffers
  for (size_t i = 0; i < PGEN_NUM_ARENAS; i++) {
    pgen_arena_t a = allocator->arenas[i];
    if (a.freefn)
      a.freefn(a.buf);
  }

  // Free everything in the freelist
  for (size_t i = 0; i < allocator->freelist.len; i++) {
    void (*fn)(void *) = allocator->freelist.entries[i].freefn;
    void *ptr = allocator->freelist.entries[i].ptr;
    fn(ptr);
  }

  // Free the freelist itself
  free(allocator->freelist.entries);
}

#if PGEN_ALLOCATOR_DEBUG
static inline void pgen_allocator_print_freelist(pgen_allocator *allocator) {

  if (allocator->freelist.len) {
    puts("Freelist:");
    for (size_t i = 0; i < allocator->freelist.len; i++) {
      printf("  {.freefn=%p, .ptr=%p, {.arena_idx=%u, .filled=%u}}\n",
             allocator->freelist.entries->freefn,
             allocator->freelist.entries->ptr,
             allocator->freelist.entries->rew.arena_idx,
             allocator->freelist.entries->rew.filled);
    }
  }
  puts("");
}
#endif

#define PGEN_ALLOC(allocator, type)                                         \
  (type *)pgen_alloc(allocator, sizeof(type), _Alignof(type))
static void *_aa_last;
#define PGEN_ALLOC_ASSIGN(allocator, type, value)                              \
  (_aa_last = PGEN_ALLOC(allocator, type), (*(type *)_aa_last = value), *(type *)_aa_last)
static inline char *pgen_alloc(pgen_allocator *allocator, size_t n,
                               size_t alignment) {
#if PGEN_ALLOCATOR_DEBUG
  printf("alloc({.arena_idx=%u, .filled=%u, .freelist_len=%u}, "
         "{.n=%zu, .alignment=%zu})\n",
         allocator->rew.arena_idx, allocator->rew.filled,
         allocator->freelist.len, n, alignment);
#endif

  char *ret = NULL;

#if PGEN_SIZE_RANGE_CHECK
  if (allocator->rew.filled > SIZE_MAX)
    PGEN_OOM();
#endif

  // Find the arena to allocate on and where we are inside it.
  size_t bufcurrent = pgen_align((size_t)allocator->rew.filled, alignment);
  size_t bufnext = bufcurrent + n;

  // Check for overflow
  if (bufnext < allocator->rew.filled)
    PGEN_OOM();

  while (1) {
    // If we need a new arena
    if (bufnext > allocator->arenas[allocator->rew.arena_idx].cap) {
      bufcurrent = 0;
      bufnext = n;

      // Make sure there's a spot for it
      if (allocator->rew.arena_idx + 1 >= PGEN_NUM_ARENAS)
        PGEN_OOM();

      // Allocate a new arena if necessary
      if (allocator->arenas[allocator->rew.arena_idx].buf)
        allocator->rew.arena_idx++;
      if (!allocator->arenas[allocator->rew.arena_idx].buf) {
        char *nb = (char *)PGEN_MALLOC(PGEN_BUFFER_SIZE);
        if (!nb)
          PGEN_OOM();
        pgen_arena_t new_arena;
        new_arena.freefn = free;
        new_arena.buf = nb;
        new_arena.cap = PGEN_BUFFER_SIZE;
        allocator->arenas[allocator->rew.arena_idx] = new_arena;
      }
    } else {
      break;
    }
  }

  ret = allocator->arenas[allocator->rew.arena_idx].buf + bufcurrent;
  allocator->rew.filled = (uint32_t)bufnext;

#if PGEN_ALLOCATOR_DEBUG
  printf("New allocator state: {.arena_idx=%u, .filled=%u, .freelist_len=%u}"
         "\n\n",
         allocator->freelist.entries->rew.arena_idx,
         allocator->freelist.entries->rew.filled, allocator->freelist.len);
#endif

  return ret;
}

// Does not take a pgen_allocator_rewind_t, does not rebind the
// lifetime of the reallocated object.
static inline void pgen_allocator_realloced(pgen_allocator *allocator,
                                            void *old_ptr, void *new_ptr,
                                            void (*new_free_fn)(void *)) {

#if PGEN_ALLOCATOR_DEBUG
  printf("realloc({.arena_idx=%u, .filled=%u, .freelist_len=%u}, "
         "{.old_ptr=%p, .new_ptr=%p, new_free_fn=%p})\n",
         allocator->rew.arena_idx, allocator->rew.filled,
         allocator->freelist.len, old_ptr, new_ptr, new_free_fn);
  pgen_allocator_print_freelist(allocator);
#endif

  for (size_t i = 0; i < allocator->freelist.len; i++) {
    void *ptr = allocator->freelist.entries[i].ptr;
    if (ptr == old_ptr) {
      allocator->freelist.entries[i].ptr = new_ptr;
      allocator->freelist.entries[i].freefn = new_free_fn;
      return;
    }
  }

#if PGEN_ALLOCATOR_DEBUG
  puts("Realloced.");
  pgen_allocator_print_freelist(allocator);
#endif
}

static inline void pgen_defer(pgen_allocator *allocator, void (*freefn)(void *),
                              void *ptr, pgen_allocator_rewind_t rew) {
#if PGEN_ALLOCATOR_DEBUG
  printf("defer({.arena_idx=%u, .filled=%u, .freelist_len=%u}, "
         "{.freefn=%p, ptr=%p, {.arena_idx=%u, .filled=%u}})\n",
         allocator->rew.arena_idx, allocator->rew.filled,
         allocator->freelist.len, ptr, rew.arena_idx, rew.filled);
  pgen_allocator_print_freelist(allocator);
#endif

  if (!freefn | !ptr)
    return;

  // Grow list by factor of 2 if too small
  size_t next_len = allocator->freelist.len + 1;
  if (next_len >= allocator->freelist.cap) {
    uint32_t new_size = allocator->freelist.len * 2;

#if PGEN_SIZE_RANGE_CHECK
    if (new_size > SIZE_MAX)
      PGEN_OOM();
#endif

    pgen_freelist_entry_t *new_entries = (pgen_freelist_entry_t *)realloc(
        allocator->freelist.entries,
        sizeof(pgen_freelist_entry_t) * (size_t)new_size);
    if (!new_entries)
      PGEN_OOM();
    allocator->freelist.entries = new_entries;
    allocator->freelist.cap = allocator->freelist.len * 2;
  }

  // Append the new entry
  pgen_freelist_entry_t entry;
  entry.freefn = freefn;
  entry.ptr = ptr;
  entry.rew = rew;
  allocator->freelist.entries[allocator->freelist.len] = entry;
  allocator->freelist.len = (uint32_t)next_len;

#if PGEN_ALLOCATOR_DEBUG
  puts("Deferred.");
  pgen_allocator_print_freelist(allocator);
#endif
}

static inline void pgen_allocator_rewind(pgen_allocator *allocator,
                                         pgen_allocator_rewind_t rew) {

#if PGEN_ALLOCATOR_DEBUG
  printf("rewind({.arena_idx=%u, .filled=%u, .freelist_len=%u}, "
         "{.arena_idx=%u, .filled=%u})\n",
         allocator->freelist.entries->rew.arena_idx,
         allocator->freelist.entries->rew.filled, allocator->freelist.len,
         rew.arena_idx, rew.filled);
  pgen_allocator_print_freelist(allocator);
#endif

  // Free all the objects associated with nodes implicitly destroyed.
  // These are the ones located beyond the rew we're rewinding back to.
  int freed_any = 0;
  size_t i = allocator->freelist.len;
  while (i--) {

    pgen_freelist_entry_t entry = allocator->freelist.entries[i];
    uint32_t arena_idx = entry.rew.arena_idx;
    uint32_t filled = entry.rew.filled;

    if ((rew.arena_idx <= arena_idx) | (rew.filled <= filled))
      break;

    freed_any = 1;
    entry.freefn(entry.ptr);
  }
  if (freed_any)
    allocator->freelist.len = (uint32_t)i;
  allocator->rew = rew;

#if PGEN_ALLOCATOR_DEBUG
  printf("rewound to: {.arena_idx=%u, .filled=%u, .freelist_len=%u}\n",
         allocator->freelist.entries->rew.arena_idx,
         allocator->freelist.entries->rew.filled, allocator->freelist.len);
  pgen_allocator_print_freelist(allocator);
#endif
}

#endif /* PGEN_ARENA_INCLUDED */


struct pl0_astnode_t;
typedef struct pl0_astnode_t pl0_astnode_t;


#ifndef PGEN_PARSER_MACROS_INCLUDED
#define PGEN_PARSER_MACROS_INCLUDED

#if defined(__STDC_VERSION__) && (__STDC_VERSION__ >= 199901L) && !defined(__cplusplus)
#  define PGEN_RESTRICT restrict
#elif defined(__clang__) || \
     (defined(__GNUC__) && (__GNUC__ >= 4)) || \
     (defined(_MSC_VER) && (_MSC_VER >= 1900))
#  define PGEN_RESTRICT __restrict
#else
#  define PGEN_RESTRICT
#endif

#define PGEN_CAT_(x, y) x##y
#define PGEN_CAT(x, y) PGEN_CAT_(x, y)
#define PGEN_NARG(...) PGEN_NARG_(__VA_ARGS__, PGEN_RSEQ_N())
#define PGEN_NARG_(...) PGEN_128TH_ARG(__VA_ARGS__)
#define PGEN_128TH_ARG(                                                        \
    _1, _2, _3, _4, _5, _6, _7, _8, _9, _10, _11, _12, _13, _14, _15, _16,     \
    _17, _18, _19, _20, _21, _22, _23, _24, _25, _26, _27, _28, _29, _30, _31, \
    _32, _33, _34, _35, _36, _37, _38, _39, _40, _41, _42, _43, _44, _45, _46, \
    _47, _48, _49, _50, _51, _52, _53, _54, _55, _56, _57, _58, _59, _60, _61, \
    _62, _63, _64, _65, _66, _67, _68, _69, _70, _71, _72, _73, _74, _75, _76, \
    _77, _78, _79, _80, _81, _82, _83, _84, _85, _86, _87, _88, _89, _90, _91, \
    _92, _93, _94, _95, _96, _97, _98, _99, _100, _101, _102, _103, _104,      \
    _105, _106, _107, _108, _109, _110, _111, _112, _113, _114, _115, _116,    \
    _117, _118, _119, _120, _121, _122, _123, _124, _125, _126, _127, N, ...)  \
  N
#define PGEN_RSEQ_N()                                                          \
  127, 126, 125, 124, 123, 122, 121, 120, 119, 118, 117, 116, 115, 114, 113,   \
      112, 111, 110, 109, 108, 107, 106, 105, 104, 103, 102, 101, 100, 99, 98, \
      97, 96, 95, 94, 93, 92, 91, 90, 89, 88, 87, 86, 85, 84, 83, 82, 81, 80,  \
      79, 78, 77, 76, 75, 74, 73, 72, 71, 70, 69, 68, 67, 66, 65, 64, 63, 62,  \
      61, 60, 59, 58, 57, 56, 55, 54, 53, 52, 51, 50, 49, 48, 47, 46, 45, 44,  \
      43, 42, 41, 40, 39, 38, 37, 36, 35, 34, 33, 32, 31, 30, 29, 28, 27, 26,  \
      25, 24, 23, 22, 21, 20, 19, 18, 17, 16, 15, 14, 13, 12, 11, 10, 9, 8, 7, \
      6, 5, 4, 3, 2, 1, 0
#endif /* PGEN_PARSER_MACROS_INCLUDED */

#ifndef PL0_TOKENIZER_INCLUDE
#define PL0_TOKENIZER_INCLUDE

typedef enum {
  PL0_TOK_STREAMBEGIN,
  PL0_TOK_STREAMEND,
  PL0_TOK_EQ,
  PL0_TOK_CEQ,
  PL0_TOK_SEMI,
  PL0_TOK_DOT,
  PL0_TOK_COMMA,
  PL0_TOK_OPEN,
  PL0_TOK_CLOSE,
  PL0_TOK_HASH,
  PL0_TOK_LT,
  PL0_TOK_LEQ,
  PL0_TOK_GT,
  PL0_TOK_GEQ,
  PL0_TOK_PLUS,
  PL0_TOK_MINUS,
  PL0_TOK_STAR,
  PL0_TOK_DIV,
  PL0_TOK_VAR,
  PL0_TOK_PROC,
  PL0_TOK_WRITE,
  PL0_TOK_CONST,
  PL0_TOK_BEGIN,
  PL0_TOK_END,
  PL0_TOK_IF,
  PL0_TOK_THEN,
  PL0_TOK_WHILE,
  PL0_TOK_DO,
  PL0_TOK_ODD,
  PL0_TOK_CALL,
  PL0_TOK_IDENT,
  PL0_TOK_NUM,
  PL0_TOK_WS,
  PL0_TOK_MLCOM,
  PL0_TOK_SLCOM,
} pl0_token_kind;

// The 0th token is beginning of stream.
// The 1st token isend of stream.
// Tokens 1 through 33 are the ones you defined.
// This totals 35 kinds of tokens.
#define PL0_NUM_TOKENKINDS 35
static const char* pl0_tokenkind_name[PL0_NUM_TOKENKINDS] = {
  "STREAMBEGIN",
  "STREAMEND",
  "EQ",
  "CEQ",
  "SEMI",
  "DOT",
  "COMMA",
  "OPEN",
  "CLOSE",
  "HASH",
  "LT",
  "LEQ",
  "GT",
  "GEQ",
  "PLUS",
  "MINUS",
  "STAR",
  "DIV",
  "VAR",
  "PROC",
  "WRITE",
  "CONST",
  "BEGIN",
  "END",
  "IF",
  "THEN",
  "WHILE",
  "DO",
  "ODD",
  "CALL",
  "IDENT",
  "NUM",
  "WS",
  "MLCOM",
  "SLCOM",
};

typedef struct {
  pl0_token_kind kind;
  codepoint_t* content; // The token begins at tokenizer->start[token->start].
  size_t len;
  size_t line;
  size_t col;
} pl0_token;

typedef struct {
  codepoint_t* start;
  size_t len;
  size_t pos;
  size_t pos_line;
  size_t pos_col;
} pl0_tokenizer;

static inline void pl0_tokenizer_init(pl0_tokenizer* tokenizer, codepoint_t* start, size_t len) {
  tokenizer->start = start;
  tokenizer->len = len;
  tokenizer->pos = 0;
  tokenizer->pos_line = 1;
  tokenizer->pos_col = 0;
}

static inline pl0_token pl0_nextToken(pl0_tokenizer* tokenizer) {
  codepoint_t* current = tokenizer->start + tokenizer->pos;
  size_t remaining = tokenizer->len - tokenizer->pos;

  int trie_state = 0;
  int smaut_state_0 = 0;
  int smaut_state_1 = 0;
  int smaut_state_2 = 0;
  int smaut_state_3 = 0;
  int smaut_state_4 = 0;
  size_t trie_munch_size = 0;
  size_t smaut_munch_size_0 = 0;
  size_t smaut_munch_size_1 = 0;
  size_t smaut_munch_size_2 = 0;
  size_t smaut_munch_size_3 = 0;
  size_t smaut_munch_size_4 = 0;
  pl0_token_kind trie_tokenkind = PL0_TOK_STREAMEND;

  for (size_t iidx = 0; iidx < remaining; iidx++) {
    codepoint_t c = current[iidx];
    int all_dead = 1;

    // Trie
    if (trie_state != -1) {
      all_dead = 0;
      if (trie_state == 0) {
        if (c == 35 /*'#'*/) trie_state = 9;
        else if (c == 40 /*'('*/) trie_state = 7;
        else if (c == 41 /*')'*/) trie_state = 8;
        else if (c == 42 /*'*'*/) trie_state = 16;
        else if (c == 43 /*'+'*/) trie_state = 14;
        else if (c == 44 /*','*/) trie_state = 6;
        else if (c == 45 /*'-'*/) trie_state = 15;
        else if (c == 46 /*'.'*/) trie_state = 5;
        else if (c == 47 /*'/'*/) trie_state = 17;
        else if (c == 58 /*':'*/) trie_state = 2;
        else if (c == 59 /*';'*/) trie_state = 4;
        else if (c == 60 /*'<'*/) trie_state = 10;
        else if (c == 61 /*'='*/) trie_state = 1;
        else if (c == 62 /*'>'*/) trie_state = 12;
        else if (c == 98 /*'b'*/) trie_state = 40;
        else if (c == 99 /*'c'*/) trie_state = 35;
        else if (c == 100 /*'d'*/) trie_state = 58;
        else if (c == 101 /*'e'*/) trie_state = 45;
        else if (c == 105 /*'i'*/) trie_state = 48;
        else if (c == 111 /*'o'*/) trie_state = 60;
        else if (c == 112 /*'p'*/) trie_state = 21;
        else if (c == 116 /*'t'*/) trie_state = 50;
        else if (c == 118 /*'v'*/) trie_state = 18;
        else if (c == 119 /*'w'*/) trie_state = 30;
        else trie_state = -1;
      }
      else if (trie_state == 2) {
        if (c == 61 /*'='*/) trie_state = 3;
        else trie_state = -1;
      }
      else if (trie_state == 10) {
        if (c == 61 /*'='*/) trie_state = 11;
        else trie_state = -1;
      }
      else if (trie_state == 12) {
        if (c == 61 /*'='*/) trie_state = 13;
        else trie_state = -1;
      }
      else if (trie_state == 18) {
        if (c == 97 /*'a'*/) trie_state = 19;
        else trie_state = -1;
      }
      else if (trie_state == 19) {
        if (c == 114 /*'r'*/) trie_state = 20;
        else trie_state = -1;
      }
      else if (trie_state == 21) {
        if (c == 114 /*'r'*/) trie_state = 22;
        else trie_state = -1;
      }
      else if (trie_state == 22) {
        if (c == 111 /*'o'*/) trie_state = 23;
        else trie_state = -1;
      }
      else if (trie_state == 23) {
        if (c == 99 /*'c'*/) trie_state = 24;
        else trie_state = -1;
      }
      else if (trie_state == 24) {
        if (c == 101 /*'e'*/) trie_state = 25;
        else trie_state = -1;
      }
      else if (trie_state == 25) {
        if (c == 100 /*'d'*/) trie_state = 26;
        else trie_state = -1;
      }
      else if (trie_state == 26) {
        if (c == 117 /*'u'*/) trie_state = 27;
        else trie_state = -1;
      }
      else if (trie_state == 27) {
        if (c == 114 /*'r'*/) trie_state = 28;
        else trie_state = -1;
      }
      else if (trie_state == 28) {
        if (c == 101 /*'e'*/) trie_state = 29;
        else trie_state = -1;
      }
      else if (trie_state == 30) {
        if (c == 104 /*'h'*/) trie_state = 54;
        else if (c == 114 /*'r'*/) trie_state = 31;
        else trie_state = -1;
      }
      else if (trie_state == 31) {
        if (c == 105 /*'i'*/) trie_state = 32;
        else trie_state = -1;
      }
      else if (trie_state == 32) {
        if (c == 116 /*'t'*/) trie_state = 33;
        else trie_state = -1;
      }
      else if (trie_state == 33) {
        if (c == 101 /*'e'*/) trie_state = 34;
        else trie_state = -1;
      }
      else if (trie_state == 35) {
        if (c == 97 /*'a'*/) trie_state = 63;
        else if (c == 111 /*'o'*/) trie_state = 36;
        else trie_state = -1;
      }
      else if (trie_state == 36) {
        if (c == 110 /*'n'*/) trie_state = 37;
        else trie_state = -1;
      }
      else if (trie_state == 37) {
        if (c == 115 /*'s'*/) trie_state = 38;
        else trie_state = -1;
      }
      else if (trie_state == 38) {
        if (c == 116 /*'t'*/) trie_state = 39;
        else trie_state = -1;
      }
      else if (trie_state == 40) {
        if (c == 101 /*'e'*/) trie_state = 41;
        else trie_state = -1;
      }
      else if (trie_state == 41) {
        if (c == 103 /*'g'*/) trie_state = 42;
        else trie_state = -1;
      }
      else if (trie_state == 42) {
        if (c == 105 /*'i'*/) trie_state = 43;
        else trie_state = -1;
      }
      else if (trie_state == 43) {
        if (c == 110 /*'n'*/) trie_state = 44;
        else trie_state = -1;
      }
      else if (trie_state == 45) {
        if (c == 110 /*'n'*/) trie_state = 46;
        else trie_state = -1;
      }
      else if (trie_state == 46) {
        if (c == 100 /*'d'*/) trie_state = 47;
        else trie_state = -1;
      }
      else if (trie_state == 48) {
        if (c == 102 /*'f'*/) trie_state = 49;
        else trie_state = -1;
      }
      else if (trie_state == 50) {
        if (c == 104 /*'h'*/) trie_state = 51;
        else trie_state = -1;
      }
      else if (trie_state == 51) {
        if (c == 101 /*'e'*/) trie_state = 52;
        else trie_state = -1;
      }
      else if (trie_state == 52) {
        if (c == 110 /*'n'*/) trie_state = 53;
        else trie_state = -1;
      }
      else if (trie_state == 54) {
        if (c == 105 /*'i'*/) trie_state = 55;
        else trie_state = -1;
      }
      else if (trie_state == 55) {
        if (c == 108 /*'l'*/) trie_state = 56;
        else trie_state = -1;
      }
      else if (trie_state == 56) {
        if (c == 101 /*'e'*/) trie_state = 57;
        else trie_state = -1;
      }
      else if (trie_state == 58) {
        if (c == 111 /*'o'*/) trie_state = 59;
        else trie_state = -1;
      }
      else if (trie_state == 60) {
        if (c == 100 /*'d'*/) trie_state = 61;
        else trie_state = -1;
      }
      else if (trie_state == 61) {
        if (c == 100 /*'d'*/) trie_state = 62;
        else trie_state = -1;
      }
      else if (trie_state == 63) {
        if (c == 108 /*'l'*/) trie_state = 64;
        else trie_state = -1;
      }
      else if (trie_state == 64) {
        if (c == 108 /*'l'*/) trie_state = 65;
        else trie_state = -1;
      }
      else {
        trie_state = -1;
      }

      // Check accept
      if (trie_state == 1) {
        trie_tokenkind =  PL0_TOK_EQ;
        trie_munch_size = iidx + 1;
      }
      else if (trie_state == 3) {
        trie_tokenkind =  PL0_TOK_CEQ;
        trie_munch_size = iidx + 1;
      }
      else if (trie_state == 4) {
        trie_tokenkind =  PL0_TOK_SEMI;
        trie_munch_size = iidx + 1;
      }
      else if (trie_state == 5) {
        trie_tokenkind =  PL0_TOK_DOT;
        trie_munch_size = iidx + 1;
      }
      else if (trie_state == 6) {
        trie_tokenkind =  PL0_TOK_COMMA;
        trie_munch_size = iidx + 1;
      }
      else if (trie_state == 7) {
        trie_tokenkind =  PL0_TOK_OPEN;
        trie_munch_size = iidx + 1;
      }
      else if (trie_state == 8) {
        trie_tokenkind =  PL0_TOK_CLOSE;
        trie_munch_size = iidx + 1;
      }
      else if (trie_state == 9) {
        trie_tokenkind =  PL0_TOK_HASH;
        trie_munch_size = iidx + 1;
      }
      else if (trie_state == 10) {
        trie_tokenkind =  PL0_TOK_LT;
        trie_munch_size = iidx + 1;
      }
      else if (trie_state == 11) {
        trie_tokenkind =  PL0_TOK_LEQ;
        trie_munch_size = iidx + 1;
      }
      else if (trie_state == 12) {
        trie_tokenkind =  PL0_TOK_GT;
        trie_munch_size = iidx + 1;
      }
      else if (trie_state == 13) {
        trie_tokenkind =  PL0_TOK_GEQ;
        trie_munch_size = iidx + 1;
      }
      else if (trie_state == 14) {
        trie_tokenkind =  PL0_TOK_PLUS;
        trie_munch_size = iidx + 1;
      }
      else if (trie_state == 15) {
        trie_tokenkind =  PL0_TOK_MINUS;
        trie_munch_size = iidx + 1;
      }
      else if (trie_state == 16) {
        trie_tokenkind =  PL0_TOK_STAR;
        trie_munch_size = iidx + 1;
      }
      else if (trie_state == 17) {
        trie_tokenkind =  PL0_TOK_DIV;
        trie_munch_size = iidx + 1;
      }
      else if (trie_state == 20) {
        trie_tokenkind =  PL0_TOK_VAR;
        trie_munch_size = iidx + 1;
      }
      else if (trie_state == 29) {
        trie_tokenkind =  PL0_TOK_PROC;
        trie_munch_size = iidx + 1;
      }
      else if (trie_state == 34) {
        trie_tokenkind =  PL0_TOK_WRITE;
        trie_munch_size = iidx + 1;
      }
      else if (trie_state == 39) {
        trie_tokenkind =  PL0_TOK_CONST;
        trie_munch_size = iidx + 1;
      }
      else if (trie_state == 44) {
        trie_tokenkind =  PL0_TOK_BEGIN;
        trie_munch_size = iidx + 1;
      }
      else if (trie_state == 47) {
        trie_tokenkind =  PL0_TOK_END;
        trie_munch_size = iidx + 1;
      }
      else if (trie_state == 49) {
        trie_tokenkind =  PL0_TOK_IF;
        trie_munch_size = iidx + 1;
      }
      else if (trie_state == 53) {
        trie_tokenkind =  PL0_TOK_THEN;
        trie_munch_size = iidx + 1;
      }
      else if (trie_state == 57) {
        trie_tokenkind =  PL0_TOK_WHILE;
        trie_munch_size = iidx + 1;
      }
      else if (trie_state == 59) {
        trie_tokenkind =  PL0_TOK_DO;
        trie_munch_size = iidx + 1;
      }
      else if (trie_state == 62) {
        trie_tokenkind =  PL0_TOK_ODD;
        trie_munch_size = iidx + 1;
      }
      else if (trie_state == 65) {
        trie_tokenkind =  PL0_TOK_CALL;
        trie_munch_size = iidx + 1;
      }
    }

    // Transition IDENT State Machine
    if (smaut_state_0 != -1) {
      all_dead = 0;

      if ((smaut_state_0 == 0) &
         ((c == '_') | ((c >= 'a') & (c <= 'z')) | ((c >= 'A') & (c <= 'Z')))) {
          smaut_state_0 = 1;
      }
      else if (((smaut_state_0 == 1) | (smaut_state_0 == 2)) &
         ((c == '_') | ((c >= 'a') & (c <= 'z')) | ((c >= 'A') & (c <= 'Z')) | ((c >= '0') & (c <= '9')))) {
          smaut_state_0 = 2;
      }
      else {
        smaut_state_0 = -1;
      }

      // Check accept
      if ((smaut_state_0 == 1) | (smaut_state_0 == 2)) {
        smaut_munch_size_0 = iidx + 1;
      }
    }

    // Transition NUM State Machine
    if (smaut_state_1 != -1) {
      all_dead = 0;

      if ((smaut_state_1 == 0) &
         ((c == '-') | (c == '+'))) {
          smaut_state_1 = 1;
      }
      else if (((smaut_state_1 >= 0) & (smaut_state_1 <= 2)) &
         ((c >= '0') & (c <= '9'))) {
          smaut_state_1 = 2;
      }
      else {
        smaut_state_1 = -1;
      }

      // Check accept
      if (smaut_state_1 == 2) {
        smaut_munch_size_1 = iidx + 1;
      }
    }

    // Transition WS State Machine
    if (smaut_state_2 != -1) {
      all_dead = 0;

      if (((smaut_state_2 == 0) | (smaut_state_2 == 1)) &
         ((c == 32) | (c == '\n') | (c == 13) | (c == 9))) {
          smaut_state_2 = 1;
      }
      else {
        smaut_state_2 = -1;
      }

      // Check accept
      if (smaut_state_2 == 1) {
        smaut_munch_size_2 = iidx + 1;
      }
    }

    // Transition MLCOM State Machine
    if (smaut_state_3 != -1) {
      all_dead = 0;

      if ((smaut_state_3 == 0) &
         (c == '/')) {
          smaut_state_3 = 1;
      }
      else if ((smaut_state_3 == 1) &
         (c == '*')) {
          smaut_state_3 = 2;
      }
      else if ((smaut_state_3 == 2) &
         (c == '*')) {
          smaut_state_3 = 3;
      }
      else if ((smaut_state_3 == 2) &
         (1)) {
          smaut_state_3 = 2;
      }
      else if ((smaut_state_3 == 3) &
         (c == '*')) {
          smaut_state_3 = 3;
      }
      else if ((smaut_state_3 == 3) &
         (c == '/')) {
          smaut_state_3 = 4;
      }
      else if ((smaut_state_3 == 3) &
         (1)) {
          smaut_state_3 = 2;
      }
      else {
        smaut_state_3 = -1;
      }

      // Check accept
      if (smaut_state_3 == 4) {
        smaut_munch_size_3 = iidx + 1;
      }
    }

    // Transition SLCOM State Machine
    if (smaut_state_4 != -1) {
      all_dead = 0;

      if ((smaut_state_4 == 0) &
         (c == '/')) {
          smaut_state_4 = 1;
      }
      else if ((smaut_state_4 == 1) &
         (c == '/')) {
          smaut_state_4 = 2;
      }
      else if ((smaut_state_4 == 2) &
         (!(c == '\n'))) {
          smaut_state_4 = 2;
      }
      else if ((smaut_state_4 == 2) &
         (c == '\n')) {
          smaut_state_4 = 3;
      }
      else {
        smaut_state_4 = -1;
      }

      // Check accept
      if ((smaut_state_4 == 2) | (smaut_state_4 == 3)) {
        smaut_munch_size_4 = iidx + 1;
      }
    }

    if (all_dead)
      break;
  }

  // Determine what token was accepted, if any.
  pl0_token_kind kind = PL0_TOK_STREAMEND;
  size_t max_munch = 0;
  if (smaut_munch_size_4 >= max_munch) {
    kind = PL0_TOK_SLCOM;
    max_munch = smaut_munch_size_4;
  }
  if (smaut_munch_size_3 >= max_munch) {
    kind = PL0_TOK_MLCOM;
    max_munch = smaut_munch_size_3;
  }
  if (smaut_munch_size_2 >= max_munch) {
    kind = PL0_TOK_WS;
    max_munch = smaut_munch_size_2;
  }
  if (smaut_munch_size_1 >= max_munch) {
    kind = PL0_TOK_NUM;
    max_munch = smaut_munch_size_1;
  }
  if (smaut_munch_size_0 >= max_munch) {
    kind = PL0_TOK_IDENT;
    max_munch = smaut_munch_size_0;
  }
  if (trie_munch_size >= max_munch) {
    kind = trie_tokenkind;
    max_munch = trie_munch_size;
  }

  pl0_token tok;
  tok.kind = kind;
  tok.content = tokenizer->start + tokenizer->pos;
  tok.len = max_munch;

  tok.line = tokenizer->pos_line;
  tok.col = tokenizer->pos_col;

  for (size_t i = 0; i < tok.len; i++) {
    if (current[i] == '\n') {
      tokenizer->pos_line++;
      tokenizer->pos_col = 0;
    } else {
      tokenizer->pos_col++;
    }
  }

  tokenizer->pos += max_munch;
  return tok;
}

#endif /* PL0_TOKENIZER_INCLUDE */

#ifndef PGEN_PL0_ASTNODE_INCLUDE
#define PGEN_PL0_ASTNODE_INCLUDE

struct pl0_parse_err;
typedef struct pl0_parse_err pl0_parse_err;
struct pl0_parse_err {
  const char* msg;
  int severity;
  size_t line;
  size_t col;
};

#ifndef PL0_MAX_PARSER_ERRORS
#define PL0_MAX_PARSER_ERRORS 20
#endif
typedef struct {
  pl0_token* tokens;
  size_t len;
  size_t pos;
  int exit;
  pgen_allocator *alloc;
  size_t num_errors;
  pl0_parse_err errlist[PL0_MAX_PARSER_ERRORS];
} pl0_parser_ctx;

static inline void pl0_parser_ctx_init(pl0_parser_ctx* parser,
                                       pgen_allocator* allocator,
                                       pl0_token* tokens, size_t num_tokens) {
  parser->tokens = tokens;
  parser->len = num_tokens;
  parser->pos = 0;
  parser->exit = 0;
  parser->alloc = allocator;
  parser->num_errors = 0;
  size_t to_zero = sizeof(pl0_parse_err) * PL0_MAX_PARSER_ERRORS;
  memset(&parser->errlist, 0, to_zero);
}
static inline pl0_parse_err* pl0_report_parse_error(pl0_parser_ctx* ctx, const char* msg, int severity) {
  if (ctx->num_errors >= PL0_MAX_PARSER_ERRORS) {
    ctx->exit = 1;
    return NULL;
  }
  pl0_parse_err* err = &ctx->errlist[ctx->num_errors++];
  err->msg = (const char*)msg;
  err->severity = severity;
  size_t toknum = ctx->pos + (ctx->pos != ctx->len - 1);
  pl0_token tok = ctx->tokens[toknum];
  err->line = tok.line;
  err->col = tok.col;

  if (severity == 3)
    ctx->exit = 1;
  return err;
}

typedef enum {
  PL0_NODE_EQ,
  PL0_NODE_CEQ,
  PL0_NODE_SEMI,
  PL0_NODE_DOT,
  PL0_NODE_COMMA,
  PL0_NODE_OPEN,
  PL0_NODE_CLOSE,
  PL0_NODE_HASH,
  PL0_NODE_LT,
  PL0_NODE_LEQ,
  PL0_NODE_GT,
  PL0_NODE_GEQ,
  PL0_NODE_PLUS,
  PL0_NODE_MINUS,
  PL0_NODE_STAR,
  PL0_NODE_DIV,
  PL0_NODE_VAR,
  PL0_NODE_PROC,
  PL0_NODE_WRITE,
  PL0_NODE_CONST,
  PL0_NODE_BEGIN,
  PL0_NODE_END,
  PL0_NODE_IF,
  PL0_NODE_THEN,
  PL0_NODE_WHILE,
  PL0_NODE_DO,
  PL0_NODE_ODD,
  PL0_NODE_CALL,
  PL0_NODE_IDENT,
  PL0_NODE_NUM,
  PL0_NODE_WS,
  PL0_NODE_MLCOM,
  PL0_NODE_SLCOM,
  PL0_NODE_PROGRAM,
  PL0_NODE_VARLIST,
  PL0_NODE_CONSTLIST,
  PL0_NODE_PROCLIST,
  PL0_NODE_STATEMENT,
  PL0_NODE_EXPRS,
  PL0_NODE_UNEXPR,
  PL0_NODE_BINEXPR,
  PL0_NODE_TERM,
} pl0_astnode_kind;

#define PL0_NUM_NODEKINDS 42
static const char* pl0_nodekind_name[PL0_NUM_NODEKINDS] = {
  "EQ",
  "CEQ",
  "SEMI",
  "DOT",
  "COMMA",
  "OPEN",
  "CLOSE",
  "HASH",
  "LT",
  "LEQ",
  "GT",
  "GEQ",
  "PLUS",
  "MINUS",
  "STAR",
  "DIV",
  "VAR",
  "PROC",
  "WRITE",
  "CONST",
  "BEGIN",
  "END",
  "IF",
  "THEN",
  "WHILE",
  "DO",
  "ODD",
  "CALL",
  "IDENT",
  "NUM",
  "WS",
  "MLCOM",
  "SLCOM",
  "PROGRAM",
  "VARLIST",
  "CONSTLIST",
  "PROCLIST",
  "STATEMENT",
  "EXPRS",
  "UNEXPR",
  "BINEXPR",
  "TERM",
};

struct pl0_astnode_t {
  pl0_astnode_t* parent;
  uint16_t num_children;
  uint16_t max_children;
  pl0_astnode_kind kind;

  codepoint_t* tok_repr;
  size_t repr_len;
  // No %extra directives.
  pl0_astnode_t** children;
};

#define PGEN_MIN1(a) a
#define PGEN_MIN2(a, b) PGEN_MIN(a, PGEN_MIN1(b))
#define PGEN_MIN3(a, b, c) PGEN_MIN(a, PGEN_MIN2(b, c))
#define PGEN_MIN4(a, b, c, d) PGEN_MIN(a, PGEN_MIN3(b, c, d))
#define PGEN_MIN5(a, b, c, d, e) PGEN_MIN(a, PGEN_MIN4(b, c, d, e))
#define PGEN_MAX1(a) a
#define PGEN_MAX2(a, b) PGEN_MAX(a, PGEN_MAX1(b))
#define PGEN_MAX3(a, b, c) PGEN_MAX(a, PGEN_MAX2(b, c))
#define PGEN_MAX4(a, b, c, d) PGEN_MAX(a, PGEN_MAX3(b, c, d))
#define PGEN_MAX5(a, b, c, d, e) PGEN_MAX(a, PGEN_MAX4(b, c, d, e))
#define PGEN_MAX(a, b) ((a) > (b) ? (a) : (b))
#define PGEN_MIN(a, b) ((a) ? ((a) > (b) ? (b) : (a)) : (b))


static inline pl0_astnode_t* pl0_astnode_list(
                             pgen_allocator* alloc,
                             pl0_astnode_kind kind,
                             size_t initial_size) {
  char* ret = pgen_alloc(alloc,
                         sizeof(pl0_astnode_t),
                         _Alignof(pl0_astnode_t));
  pl0_astnode_t *node = (pl0_astnode_t*)ret;

  pl0_astnode_t **children;
  if (initial_size) {
    children = (pl0_astnode_t**)PGEN_MALLOC(sizeof(pl0_astnode_t*) * initial_size);
    if (!children) PGEN_OOM();
    pgen_defer(alloc, PGEN_FREE, children, alloc->rew);
  } else {
    children = NULL;
  }

  node->kind = kind;
  node->parent = NULL;
  node->max_children = (uint16_t)initial_size;
  node->num_children = 0;
  node->children = children;
  node->tok_repr = NULL;
  node->repr_len = 0;
  return node;
}

static inline pl0_astnode_t* pl0_astnode_leaf(
                             pgen_allocator* alloc,
                             pl0_astnode_kind kind) {
  char* ret = pgen_alloc(alloc,
                         sizeof(pl0_astnode_t),
                         _Alignof(pl0_astnode_t));
  pl0_astnode_t *node = (pl0_astnode_t *)ret;
  pl0_astnode_t *children = NULL;
  node->kind = kind;
  node->parent = NULL;
  node->max_children = 0;
  node->num_children = 0;
  node->children = NULL;
  node->tok_repr = NULL;
  node->repr_len = 0;
  return node;
}

static inline pl0_astnode_t* pl0_astnode_fixed_1(
                             pgen_allocator* alloc,
                             pl0_astnode_kind kind,
                             pl0_astnode_t* PGEN_RESTRICT n0) {
  char* ret = pgen_alloc(alloc,
                         sizeof(pl0_astnode_t) +
                         sizeof(pl0_astnode_t *) * 1,
                         _Alignof(pl0_astnode_t));
  pl0_astnode_t *node = (pl0_astnode_t *)ret;
  pl0_astnode_t **children = (pl0_astnode_t **)(node + 1);
  node->kind = kind;
  node->parent = NULL;
  node->max_children = 0;
  node->num_children = 1;
  node->children = children;
  node->tok_repr = NULL;
  node->repr_len = 0;
  children[0] = n0;
  n0->parent = node;
  return node;
}

static inline pl0_astnode_t* pl0_astnode_fixed_2(
                             pgen_allocator* alloc,
                             pl0_astnode_kind kind,
                             pl0_astnode_t* PGEN_RESTRICT n0,
                             pl0_astnode_t* PGEN_RESTRICT n1) {
  char* ret = pgen_alloc(alloc,
                         sizeof(pl0_astnode_t) +
                         sizeof(pl0_astnode_t *) * 2,
                         _Alignof(pl0_astnode_t));
  pl0_astnode_t *node = (pl0_astnode_t *)ret;
  pl0_astnode_t **children = (pl0_astnode_t **)(node + 1);
  node->kind = kind;
  node->parent = NULL;
  node->max_children = 0;
  node->num_children = 2;
  node->children = children;
  node->tok_repr = NULL;
  node->repr_len = 0;
  children[0] = n0;
  n0->parent = node;
  children[1] = n1;
  n1->parent = node;
  return node;
}

static inline pl0_astnode_t* pl0_astnode_fixed_3(
                             pgen_allocator* alloc,
                             pl0_astnode_kind kind,
                             pl0_astnode_t* PGEN_RESTRICT n0,
                             pl0_astnode_t* PGEN_RESTRICT n1,
                             pl0_astnode_t* PGEN_RESTRICT n2) {
  char* ret = pgen_alloc(alloc,
                         sizeof(pl0_astnode_t) +
                         sizeof(pl0_astnode_t *) * 3,
                         _Alignof(pl0_astnode_t));
  pl0_astnode_t *node = (pl0_astnode_t *)ret;
  pl0_astnode_t **children = (pl0_astnode_t **)(node + 1);
  node->kind = kind;
  node->parent = NULL;
  node->max_children = 0;
  node->num_children = 3;
  node->children = children;
  node->tok_repr = NULL;
  node->repr_len = 0;
  children[0] = n0;
  n0->parent = node;
  children[1] = n1;
  n1->parent = node;
  children[2] = n2;
  n2->parent = node;
  return node;
}

static inline pl0_astnode_t* pl0_astnode_fixed_4(
                             pgen_allocator* alloc,
                             pl0_astnode_kind kind,
                             pl0_astnode_t* PGEN_RESTRICT n0,
                             pl0_astnode_t* PGEN_RESTRICT n1,
                             pl0_astnode_t* PGEN_RESTRICT n2,
                             pl0_astnode_t* PGEN_RESTRICT n3) {
  char* ret = pgen_alloc(alloc,
                         sizeof(pl0_astnode_t) +
                         sizeof(pl0_astnode_t *) * 4,
                         _Alignof(pl0_astnode_t));
  pl0_astnode_t *node = (pl0_astnode_t *)ret;
  pl0_astnode_t **children = (pl0_astnode_t **)(node + 1);
  node->kind = kind;
  node->parent = NULL;
  node->max_children = 0;
  node->num_children = 4;
  node->children = children;
  node->tok_repr = NULL;
  node->repr_len = 0;
  children[0] = n0;
  n0->parent = node;
  children[1] = n1;
  n1->parent = node;
  children[2] = n2;
  n2->parent = node;
  children[3] = n3;
  n3->parent = node;
  return node;
}

static inline pl0_astnode_t* pl0_astnode_fixed_5(
                             pgen_allocator* alloc,
                             pl0_astnode_kind kind,
                             pl0_astnode_t* PGEN_RESTRICT n0,
                             pl0_astnode_t* PGEN_RESTRICT n1,
                             pl0_astnode_t* PGEN_RESTRICT n2,
                             pl0_astnode_t* PGEN_RESTRICT n3,
                             pl0_astnode_t* PGEN_RESTRICT n4) {
  char* ret = pgen_alloc(alloc,
                         sizeof(pl0_astnode_t) +
                         sizeof(pl0_astnode_t *) * 5,
                         _Alignof(pl0_astnode_t));
  pl0_astnode_t *node = (pl0_astnode_t *)ret;
  pl0_astnode_t **children = (pl0_astnode_t **)(node + 1);
  node->kind = kind;
  node->parent = NULL;
  node->max_children = 0;
  node->num_children = 5;
  node->children = children;
  node->tok_repr = NULL;
  node->repr_len = 0;
  children[0] = n0;
  n0->parent = node;
  children[1] = n1;
  n1->parent = node;
  children[2] = n2;
  n2->parent = node;
  children[3] = n3;
  n3->parent = node;
  children[4] = n4;
  n4->parent = node;
  return node;
}

static inline void pl0_astnode_add(pgen_allocator* alloc, pl0_astnode_t *list, pl0_astnode_t *node) {
  if (list->max_children == list->num_children) {
    // Figure out the new size. Check for overflow where applicable.
    uint64_t new_max = (uint64_t)list->max_children * 2;
    if (new_max > UINT16_MAX || new_max > SIZE_MAX) PGEN_OOM();
    if (SIZE_MAX < UINT16_MAX && (size_t)new_max > SIZE_MAX / sizeof(pl0_astnode_t)) PGEN_OOM();
    size_t new_bytes = (size_t)new_max * sizeof(pl0_astnode_t);

    // Reallocate the list, and inform the allocator.
    void* old_ptr = list->children;
    void* new_ptr = realloc(list->children, new_bytes);
    if (!new_ptr) PGEN_OOM();
    list->children = (pl0_astnode_t **)new_ptr;
    list->max_children = (uint16_t)new_max;
    pgen_allocator_realloced(alloc, old_ptr, new_ptr, free);
  }
  node->parent = list;
  list->children[list->num_children++] = node;
}

static inline void pl0_parser_rewind(pl0_parser_ctx *ctx, pgen_parser_rewind_t rew) {
  pgen_allocator_rewind(ctx->alloc, rew.arew);
  ctx->pos = rew.prew;
}

static inline pl0_astnode_t* pl0_astnode_repr(pl0_astnode_t* node, pl0_astnode_t* t) {
  node->tok_repr = t->tok_repr;
  node->repr_len = t->repr_len;
  return node;
}

static inline pl0_astnode_t* pl0_astnode_cprepr(pl0_astnode_t* node, codepoint_t* cps, size_t repr_len) {
  node->tok_repr = cps;
  node->repr_len = repr_len;
  return node;
}

static inline pl0_astnode_t* pl0_astnode_srepr(pgen_allocator* allocator, pl0_astnode_t* node, char* s) {
  size_t cpslen = strlen(s);
  codepoint_t* cps = (codepoint_t*)pgen_alloc(allocator, (cpslen + 1) * sizeof(codepoint_t), _Alignof(codepoint_t));
  for (size_t i = 0; i < cpslen; i++) cps[i] = (codepoint_t)s[i];
  cps[cpslen] = 0;
  node->tok_repr = cps;
  node->repr_len = cpslen;
  return node;
}

static inline int pl0_node_print_content(pl0_astnode_t* node, pl0_token* tokens) {
  int found = 0;
  codepoint_t* utf32 = NULL; size_t utf32len = 0;
  char* utf8 = NULL; size_t utf8len = 0;
  if (node->tok_repr && node->repr_len) {
    utf32 = node->tok_repr;
    utf32len = node->repr_len;
    int success = UTF8_encode(node->tok_repr, node->repr_len, &utf8, &utf8len);
    if (success) {
      for (size_t i = 0; i < utf8len; i++)
        if (utf8[i] == '\n') fputc('\\', stdout), fputc('n', stdout);
        else if (utf8[i] == '"') fputc('\\', stdout), fputc(utf8[i], stdout);
        else fputc(utf8[i], stdout);
      return PGEN_FREE(utf8), 1;
    }
  }
  return 0;
}

static inline int pl0_astnode_print_h(pl0_token* tokens, pl0_astnode_t *node, size_t depth, int fl) {
  #define indent() for (size_t i = 0; i < depth; i++) printf("  ")
  if (!node)
    return 0;
  else if (node == (pl0_astnode_t*)(void*)(uintptr_t)_Alignof(pl0_astnode_t))
    puts("ERROR, CAPTURED SUCC."), exit(1);

  indent(); puts("{");
  depth++;
  indent(); printf("\"kind\": "); printf("\"%s\",\n", pl0_nodekind_name[node->kind]);
  if (!(!node->tok_repr & !node->repr_len)) {
    indent();
    printf("\"content\": \"");
    pl0_node_print_content(node, tokens);
    printf("\",\n");
  }
  size_t cnum = node->num_children;
  if (cnum) {
    indent(); printf("\"num_children\": %zu,\n", cnum);
    indent(); printf("\"children\": [");
    putchar('\n');
    for (size_t i = 0; i < cnum; i++)
      pl0_astnode_print_h(tokens, node->children[i], depth + 1, i == cnum - 1);
    indent();
    printf("]\n");
  }
  depth--;
  indent(); putchar('}'); if (fl != 1) putchar(','); putchar('\n');
  return 0;
#undef indent
}

static inline void pl0_astnode_print_json(pl0_token* tokens, pl0_astnode_t *node) {
  if (node)    pl0_astnode_print_h(tokens, node, 0, 1);
  else    puts("The AST is null.");}

#define SUCC                     (pl0_astnode_t*)(void*)(uintptr_t)_Alignof(pl0_astnode_t)

#define rec(label)               pgen_parser_rewind_t _rew_##label = (pgen_parser_rewind_t){ctx->alloc->rew, ctx->pos};
#define rew(label)               pl0_parser_rewind(ctx, _rew_##label)
#define node(kindname, ...)      PGEN_CAT(pl0_astnode_fixed_, PGEN_NARG(__VA_ARGS__))(ctx->alloc, kind(kindname), __VA_ARGS__)
#define kind(name)               PL0_NODE_##name
#define list(kind)               pl0_astnode_list(ctx->alloc, PL0_NODE_##kind, 16)
#define leaf(kind)               pl0_astnode_leaf(ctx->alloc, PL0_NODE_##kind)
#define add(list, node)          pl0_astnode_add(ctx->alloc, list, node)
#define has(node)                (((uintptr_t)node <= (uintptr_t)SUCC) ? 0 : 1)
#define repr(node, t)            pl0_astnode_repr(node, t)
#define srepr(node, s)           pl0_astnode_srepr(ctx->alloc, node, (char*)s)
#define cprepr(node, cps, len)   pl0_astnode_cprepr(node, cps, len)
#define expect(kind, cap)        ((ctx->pos < ctx->len && ctx->tokens[ctx->pos].kind == pl0_TOK_##kind) ? ctx->pos++, (cap ? cprepr(leaf(kind), NULL, ctx->pos-1) : SUCC) : NULL)

#define LB {
#define RB }

#define INFO(msg)                pl0_report_parse_error(ctx, (const char*)msg, 0)
#define WARNING(msg)             pl0_report_parse_error(ctx, (const char*)msg, 1)
#define ERROR(msg)               pl0_report_parse_error(ctx, (const char*)msg, 2)
#define FATAL(msg)               pl0_report_parse_error(ctx, (const char*)msg, 3)

/******************/
/* Mid Directives */
/******************/
#define recover(kind) (ERROR("Missing token: " #kind), ret = leaf(kind))

static inline pl0_astnode_t* pl0_parse_program(pl0_parser_ctx* ctx);
static inline pl0_astnode_t* pl0_parse_vdef(pl0_parser_ctx* ctx);
static inline pl0_astnode_t* pl0_parse_block(pl0_parser_ctx* ctx);
static inline pl0_astnode_t* pl0_parse_statement(pl0_parser_ctx* ctx);
static inline pl0_astnode_t* pl0_parse_condition(pl0_parser_ctx* ctx);
static inline pl0_astnode_t* pl0_parse_expression(pl0_parser_ctx* ctx);
static inline pl0_astnode_t* pl0_parse_term(pl0_parser_ctx* ctx);
static inline pl0_astnode_t* pl0_parse_factor(pl0_parser_ctx* ctx);


static inline pl0_astnode_t* pl0_parse_program(pl0_parser_ctx* ctx) {
  pl0_astnode_t* b = NULL;
  #define rule expr_ret_0
  pl0_astnode_t* expr_ret_0 = NULL;
  pl0_astnode_t* expr_ret_1 = NULL;
  pl0_astnode_t* expr_ret_2 = NULL;
  rec(mod_2);
  // ModExprList 0
  // CodeExpr
  #define ret expr_ret_2
  ret = SUCC;
  rule=list(PROGRAM);
  #undef ret
  // ModExprList 1
  if (expr_ret_2) {
    pl0_astnode_t* expr_ret_3 = NULL;
    pl0_astnode_t* expr_ret_4 = SUCC;
    while (expr_ret_4)
    {
      rec(kleene_rew_3);
      pl0_astnode_t* expr_ret_5 = NULL;
      rec(mod_5);
      // ModExprList 0
      pl0_astnode_t* expr_ret_6 = NULL;
      expr_ret_6 = pl0_parse_block(ctx);
      if (ctx->exit) return NULL;
      expr_ret_5 = expr_ret_6;
      b = expr_ret_6;
      // ModExprList 1
      if (expr_ret_5) {
        // CodeExpr
        #define ret expr_ret_5
        ret = SUCC;
        add(rule, b);
        #undef ret
      }

      // ModExprList end
      if (!expr_ret_5) rew(mod_5);
      expr_ret_4 = expr_ret_5;
    }

    expr_ret_3 = SUCC;
    expr_ret_2 = expr_ret_3;
  }

  // ModExprList 2
  if (expr_ret_2) {
    pl0_astnode_t* expr_ret_7 = NULL;
    if (ctx->pos < ctx->len && ctx->tokens[ctx->pos].kind == PL0_TOK_DOT) {
      // Capturing DOT.
      expr_ret_7 = leaf(DOT);
      expr_ret_7->tok_repr = ctx->tokens[ctx->pos].content;
      expr_ret_7->repr_len = ctx->tokens[ctx->pos].len;
      ctx->pos++;
    } else {
      expr_ret_7 = NULL;
    }

    if (!expr_ret_7) {
      FATAL("Expected a dot at the end of the program");
      return NULL;
    }
    expr_ret_2 = expr_ret_7;
  }

  // ModExprList end
  if (!expr_ret_2) rew(mod_2);
  expr_ret_1 = expr_ret_2;
  if (!rule) rule = expr_ret_1;
  if (!expr_ret_1) rule = NULL;
  return rule;
  #undef rule
}

static inline pl0_astnode_t* pl0_parse_vdef(pl0_parser_ctx* ctx) {
  pl0_astnode_t* i = NULL;
  pl0_astnode_t* n = NULL;
  pl0_astnode_t* e = NULL;
  #define rule expr_ret_8
  pl0_astnode_t* expr_ret_8 = NULL;
  pl0_astnode_t* expr_ret_9 = NULL;
  pl0_astnode_t* expr_ret_10 = NULL;

  // SlashExpr 0
  if (!expr_ret_10) {
    pl0_astnode_t* expr_ret_11 = NULL;
    rec(mod_11);
    // ModExprList 0
    if (ctx->pos < ctx->len && ctx->tokens[ctx->pos].kind == PL0_TOK_VAR) {
      // Not capturing VAR.
      expr_ret_11 = SUCC;
      ctx->pos++;
    } else {
      expr_ret_11 = NULL;
    }

    // ModExprList 1
    if (expr_ret_11) {
      // CodeExpr
      #define ret expr_ret_11
      ret = SUCC;
      rule=list(VARLIST);
      #undef ret
    }

    // ModExprList 2
    if (expr_ret_11) {
      pl0_astnode_t* expr_ret_12 = NULL;
      if (ctx->pos < ctx->len && ctx->tokens[ctx->pos].kind == PL0_TOK_IDENT) {
        // Capturing IDENT.
        expr_ret_12 = leaf(IDENT);
        expr_ret_12->tok_repr = ctx->tokens[ctx->pos].content;
        expr_ret_12->repr_len = ctx->tokens[ctx->pos].len;
        ctx->pos++;
      } else {
        expr_ret_12 = NULL;
      }

      if (!expr_ret_12) {
        FATAL("Expected an identifier");
        return NULL;
      }
      expr_ret_11 = expr_ret_12;
      i = expr_ret_12;
    }

    // ModExprList 3
    if (expr_ret_11) {
      // CodeExpr
      #define ret expr_ret_11
      ret = SUCC;
      add(rule, node(IDENT, i));
      #undef ret
    }

    // ModExprList 4
    if (expr_ret_11) {
      pl0_astnode_t* expr_ret_13 = NULL;
      pl0_astnode_t* expr_ret_14 = SUCC;
      while (expr_ret_14)
      {
        rec(kleene_rew_13);
        pl0_astnode_t* expr_ret_15 = NULL;
        rec(mod_15);
        // ModExprList 0
        if (ctx->pos < ctx->len && ctx->tokens[ctx->pos].kind == PL0_TOK_COMMA) {
          // Not capturing COMMA.
          expr_ret_15 = SUCC;
          ctx->pos++;
        } else {
          expr_ret_15 = NULL;
        }

        // ModExprList 1
        if (expr_ret_15) {
          pl0_astnode_t* expr_ret_16 = NULL;
          if (ctx->pos < ctx->len && ctx->tokens[ctx->pos].kind == PL0_TOK_IDENT) {
            // Capturing IDENT.
            expr_ret_16 = leaf(IDENT);
            expr_ret_16->tok_repr = ctx->tokens[ctx->pos].content;
            expr_ret_16->repr_len = ctx->tokens[ctx->pos].len;
            ctx->pos++;
          } else {
            expr_ret_16 = NULL;
          }

          if (!expr_ret_16) {
            FATAL("Expected an identifier");
            return NULL;
          }
          expr_ret_15 = expr_ret_16;
          i = expr_ret_16;
        }

        // ModExprList 2
        if (expr_ret_15) {
          // CodeExpr
          #define ret expr_ret_15
          ret = SUCC;
          add(rule, i);
          #undef ret
        }

        // ModExprList end
        if (!expr_ret_15) rew(mod_15);
        expr_ret_14 = expr_ret_15;
      }

      expr_ret_13 = SUCC;
      expr_ret_11 = expr_ret_13;
    }

    // ModExprList 5
    if (expr_ret_11) {
      pl0_astnode_t* expr_ret_17 = NULL;
      if (ctx->pos < ctx->len && ctx->tokens[ctx->pos].kind == PL0_TOK_SEMI) {
        // Capturing SEMI.
        expr_ret_17 = leaf(SEMI);
        expr_ret_17->tok_repr = ctx->tokens[ctx->pos].content;
        expr_ret_17->repr_len = ctx->tokens[ctx->pos].len;
        ctx->pos++;
      } else {
        expr_ret_17 = NULL;
      }

      if (!expr_ret_17) {
        pl0_astnode_t* expr_ret_18 = NULL;
        // CodeExpr
        #define ret expr_ret_18
        ret = SUCC;
        recover(SEMI);
        #undef ret
        return expr_ret_18==SUCC ? NULL : expr_ret_18;
      }

      expr_ret_11 = expr_ret_17;
    }

    // ModExprList end
    if (!expr_ret_11) rew(mod_11);
    expr_ret_10 = expr_ret_11;
  }

  // SlashExpr 1
  if (!expr_ret_10) {
    pl0_astnode_t* expr_ret_19 = NULL;
    rec(mod_19);
    // ModExprList 0
    if (ctx->pos < ctx->len && ctx->tokens[ctx->pos].kind == PL0_TOK_CONST) {
      // Not capturing CONST.
      expr_ret_19 = SUCC;
      ctx->pos++;
    } else {
      expr_ret_19 = NULL;
    }

    // ModExprList 1
    if (expr_ret_19) {
      // CodeExpr
      #define ret expr_ret_19
      ret = SUCC;
      rule=list(CONSTLIST);
      #undef ret
    }

    // ModExprList 2
    if (expr_ret_19) {
      pl0_astnode_t* expr_ret_20 = NULL;
      if (ctx->pos < ctx->len && ctx->tokens[ctx->pos].kind == PL0_TOK_IDENT) {
        // Capturing IDENT.
        expr_ret_20 = leaf(IDENT);
        expr_ret_20->tok_repr = ctx->tokens[ctx->pos].content;
        expr_ret_20->repr_len = ctx->tokens[ctx->pos].len;
        ctx->pos++;
      } else {
        expr_ret_20 = NULL;
      }

      if (!expr_ret_20) {
        FATAL("Expected an identifier");
        return NULL;
      }
      expr_ret_19 = expr_ret_20;
      i = expr_ret_20;
    }

    // ModExprList 3
    if (expr_ret_19) {
      pl0_astnode_t* expr_ret_21 = NULL;
      if (ctx->pos < ctx->len && ctx->tokens[ctx->pos].kind == PL0_TOK_EQ) {
        // Not capturing EQ.
        expr_ret_21 = SUCC;
        ctx->pos++;
      } else {
        expr_ret_21 = NULL;
      }

      if (!expr_ret_21) {
        FATAL("Expected an equal sign");
        return NULL;
      }
      expr_ret_19 = expr_ret_21;
    }

    // ModExprList 4
    if (expr_ret_19) {
      pl0_astnode_t* expr_ret_22 = NULL;
      if (ctx->pos < ctx->len && ctx->tokens[ctx->pos].kind == PL0_TOK_NUM) {
        // Capturing NUM.
        expr_ret_22 = leaf(NUM);
        expr_ret_22->tok_repr = ctx->tokens[ctx->pos].content;
        expr_ret_22->repr_len = ctx->tokens[ctx->pos].len;
        ctx->pos++;
      } else {
        expr_ret_22 = NULL;
      }

      if (!expr_ret_22) {
        FATAL("Expected a number");
        return NULL;
      }
      expr_ret_19 = expr_ret_22;
      n = expr_ret_22;
    }

    // ModExprList 5
    if (expr_ret_19) {
      // CodeExpr
      #define ret expr_ret_19
      ret = SUCC;
      add(rule, node(CONST, i, n));
      #undef ret
    }

    // ModExprList 6
    if (expr_ret_19) {
      pl0_astnode_t* expr_ret_23 = NULL;
      pl0_astnode_t* expr_ret_24 = SUCC;
      while (expr_ret_24)
      {
        rec(kleene_rew_23);
        pl0_astnode_t* expr_ret_25 = NULL;
        rec(mod_25);
        // ModExprList 0
        if (ctx->pos < ctx->len && ctx->tokens[ctx->pos].kind == PL0_TOK_COMMA) {
          // Not capturing COMMA.
          expr_ret_25 = SUCC;
          ctx->pos++;
        } else {
          expr_ret_25 = NULL;
        }

        // ModExprList 1
        if (expr_ret_25) {
          pl0_astnode_t* expr_ret_26 = NULL;
          if (ctx->pos < ctx->len && ctx->tokens[ctx->pos].kind == PL0_TOK_IDENT) {
            // Capturing IDENT.
            expr_ret_26 = leaf(IDENT);
            expr_ret_26->tok_repr = ctx->tokens[ctx->pos].content;
            expr_ret_26->repr_len = ctx->tokens[ctx->pos].len;
            ctx->pos++;
          } else {
            expr_ret_26 = NULL;
          }

          if (!expr_ret_26) {
            FATAL("Expected an identifier");
            return NULL;
          }
          expr_ret_25 = expr_ret_26;
          i = expr_ret_26;
        }

        // ModExprList 2
        if (expr_ret_25) {
          pl0_astnode_t* expr_ret_27 = NULL;
          if (ctx->pos < ctx->len && ctx->tokens[ctx->pos].kind == PL0_TOK_EQ) {
            // Capturing EQ.
            expr_ret_27 = leaf(EQ);
            expr_ret_27->tok_repr = ctx->tokens[ctx->pos].content;
            expr_ret_27->repr_len = ctx->tokens[ctx->pos].len;
            ctx->pos++;
          } else {
            expr_ret_27 = NULL;
          }

          if (!expr_ret_27) {
            FATAL("Expected an equal sign");
            return NULL;
          }
          expr_ret_25 = expr_ret_27;
          e = expr_ret_27;
        }

        // ModExprList 3
        if (expr_ret_25) {
          pl0_astnode_t* expr_ret_28 = NULL;
          if (ctx->pos < ctx->len && ctx->tokens[ctx->pos].kind == PL0_TOK_NUM) {
            // Capturing NUM.
            expr_ret_28 = leaf(NUM);
            expr_ret_28->tok_repr = ctx->tokens[ctx->pos].content;
            expr_ret_28->repr_len = ctx->tokens[ctx->pos].len;
            ctx->pos++;
          } else {
            expr_ret_28 = NULL;
          }

          if (!expr_ret_28) {
            FATAL("Expected a number");
            return NULL;
          }
          expr_ret_25 = expr_ret_28;
          n = expr_ret_28;
        }

        // ModExprList 4
        if (expr_ret_25) {
          // CodeExpr
          #define ret expr_ret_25
          ret = SUCC;
          add(rule, node(CONST, i, n));
          #undef ret
        }

        // ModExprList end
        if (!expr_ret_25) rew(mod_25);
        expr_ret_24 = expr_ret_25;
      }

      expr_ret_23 = SUCC;
      expr_ret_19 = expr_ret_23;
    }

    // ModExprList 7
    if (expr_ret_19) {
      pl0_astnode_t* expr_ret_29 = NULL;
      if (ctx->pos < ctx->len && ctx->tokens[ctx->pos].kind == PL0_TOK_SEMI) {
        // Capturing SEMI.
        expr_ret_29 = leaf(SEMI);
        expr_ret_29->tok_repr = ctx->tokens[ctx->pos].content;
        expr_ret_29->repr_len = ctx->tokens[ctx->pos].len;
        ctx->pos++;
      } else {
        expr_ret_29 = NULL;
      }

      if (!expr_ret_29) {
        pl0_astnode_t* expr_ret_30 = NULL;
        // CodeExpr
        #define ret expr_ret_30
        ret = SUCC;
        recover(SEMI);
        #undef ret
        return expr_ret_30==SUCC ? NULL : expr_ret_30;
      }

      expr_ret_19 = expr_ret_29;
    }

    // ModExprList end
    if (!expr_ret_19) rew(mod_19);
    expr_ret_10 = expr_ret_19;
  }

  // SlashExpr end
  expr_ret_9 = expr_ret_10;

  if (!rule) rule = expr_ret_9;
  if (!expr_ret_9) rule = NULL;
  return rule;
  #undef rule
}

static inline pl0_astnode_t* pl0_parse_block(pl0_parser_ctx* ctx) {
  pl0_astnode_t* v = NULL;
  pl0_astnode_t* i = NULL;
  pl0_astnode_t* s = NULL;
  #define rule expr_ret_31
  pl0_astnode_t* expr_ret_31 = NULL;
  pl0_astnode_t* expr_ret_32 = NULL;
  pl0_astnode_t* expr_ret_33 = NULL;

  // SlashExpr 0
  if (!expr_ret_33) {
    pl0_astnode_t* expr_ret_34 = NULL;
    rec(mod_34);
    // ModExprList 0
    pl0_astnode_t* expr_ret_35 = NULL;
    expr_ret_35 = pl0_parse_vdef(ctx);
    if (ctx->exit) return NULL;
    expr_ret_34 = expr_ret_35;
    v = expr_ret_35;
    // ModExprList 1
    if (expr_ret_34) {
      // CodeExpr
      #define ret expr_ret_34
      ret = SUCC;
      rule=v;
      #undef ret
    }

    // ModExprList end
    if (!expr_ret_34) rew(mod_34);
    expr_ret_33 = expr_ret_34;
  }

  // SlashExpr 1
  if (!expr_ret_33) {
    pl0_astnode_t* expr_ret_36 = NULL;
    rec(mod_36);
    // ModExprList 0
    // CodeExpr
    #define ret expr_ret_36
    ret = SUCC;
    rule=list(PROCLIST);
    #undef ret
    // ModExprList 1
    if (expr_ret_36) {
      pl0_astnode_t* expr_ret_37 = NULL;
      pl0_astnode_t* expr_ret_38 = SUCC;
      while (expr_ret_38)
      {
        rec(kleene_rew_37);
        pl0_astnode_t* expr_ret_39 = NULL;
        rec(mod_39);
        // ModExprList 0
        if (ctx->pos < ctx->len && ctx->tokens[ctx->pos].kind == PL0_TOK_PROC) {
          // Not capturing PROC.
          expr_ret_39 = SUCC;
          ctx->pos++;
        } else {
          expr_ret_39 = NULL;
        }

        // ModExprList 1
        if (expr_ret_39) {
          pl0_astnode_t* expr_ret_40 = NULL;
          if (ctx->pos < ctx->len && ctx->tokens[ctx->pos].kind == PL0_TOK_IDENT) {
            // Capturing IDENT.
            expr_ret_40 = leaf(IDENT);
            expr_ret_40->tok_repr = ctx->tokens[ctx->pos].content;
            expr_ret_40->repr_len = ctx->tokens[ctx->pos].len;
            ctx->pos++;
          } else {
            expr_ret_40 = NULL;
          }

          if (!expr_ret_40) {
            FATAL("Expected an identifier");
            return NULL;
          }
          expr_ret_39 = expr_ret_40;
          i = expr_ret_40;
        }

        // ModExprList 2
        if (expr_ret_39) {
          pl0_astnode_t* expr_ret_41 = NULL;
          if (ctx->pos < ctx->len && ctx->tokens[ctx->pos].kind == PL0_TOK_SEMI) {
            // Not capturing SEMI.
            expr_ret_41 = SUCC;
            ctx->pos++;
          } else {
            expr_ret_41 = NULL;
          }

          if (!expr_ret_41) {
            pl0_astnode_t* expr_ret_42 = NULL;
            // CodeExpr
            #define ret expr_ret_42
            ret = SUCC;
            recover(SEMI);
            #undef ret
            return expr_ret_42==SUCC ? NULL : expr_ret_42;
          }

          expr_ret_39 = expr_ret_41;
        }

        // ModExprList 3
        if (expr_ret_39) {
          pl0_astnode_t* expr_ret_43 = NULL;
          expr_ret_43 = pl0_parse_vdef(ctx);
          if (ctx->exit) return NULL;
          // optional
          if (!expr_ret_43)
            expr_ret_43 = SUCC;
          expr_ret_39 = expr_ret_43;
          v = expr_ret_43;
        }

        // ModExprList 4
        if (expr_ret_39) {
          // CodeExpr
          #define ret expr_ret_39
          ret = SUCC;
          add(rule, has(v) ? node(PROC, i, v) : node(PROC, i));
          #undef ret
        }

        // ModExprList end
        if (!expr_ret_39) rew(mod_39);
        expr_ret_38 = expr_ret_39;
      }

      expr_ret_37 = SUCC;
      expr_ret_36 = expr_ret_37;
    }

    // ModExprList 2
    if (expr_ret_36) {
      pl0_astnode_t* expr_ret_44 = NULL;
      expr_ret_44 = pl0_parse_statement(ctx);
      if (ctx->exit) return NULL;
      expr_ret_36 = expr_ret_44;
      s = expr_ret_44;
    }

    // ModExprList 3
    if (expr_ret_36) {
      // CodeExpr
      #define ret expr_ret_36
      ret = SUCC;
      add(rule, s);
      #undef ret
    }

    // ModExprList 4
    if (expr_ret_36) {
      pl0_astnode_t* expr_ret_45 = NULL;
      if (ctx->pos < ctx->len && ctx->tokens[ctx->pos].kind == PL0_TOK_SEMI) {
        // Capturing SEMI.
        expr_ret_45 = leaf(SEMI);
        expr_ret_45->tok_repr = ctx->tokens[ctx->pos].content;
        expr_ret_45->repr_len = ctx->tokens[ctx->pos].len;
        ctx->pos++;
      } else {
        expr_ret_45 = NULL;
      }

      if (!expr_ret_45) {
        pl0_astnode_t* expr_ret_46 = NULL;
        // CodeExpr
        #define ret expr_ret_46
        ret = SUCC;
        recover(SEMI);
        #undef ret
        return expr_ret_46==SUCC ? NULL : expr_ret_46;
      }

      expr_ret_36 = expr_ret_45;
    }

    // ModExprList end
    if (!expr_ret_36) rew(mod_36);
    expr_ret_33 = expr_ret_36;
  }

  // SlashExpr end
  expr_ret_32 = expr_ret_33;

  if (!rule) rule = expr_ret_32;
  if (!expr_ret_32) rule = NULL;
  return rule;
  #undef rule
}

static inline pl0_astnode_t* pl0_parse_statement(pl0_parser_ctx* ctx) {
  pl0_astnode_t* id = NULL;
  pl0_astnode_t* e = NULL;
  pl0_astnode_t* smt = NULL;
  pl0_astnode_t* c = NULL;
  #define rule expr_ret_47
  pl0_astnode_t* expr_ret_47 = NULL;
  pl0_astnode_t* expr_ret_48 = NULL;
  pl0_astnode_t* expr_ret_49 = NULL;

  // SlashExpr 0
  if (!expr_ret_49) {
    pl0_astnode_t* expr_ret_50 = NULL;
    rec(mod_50);
    // ModExprList 0
    pl0_astnode_t* expr_ret_51 = NULL;
    if (ctx->pos < ctx->len && ctx->tokens[ctx->pos].kind == PL0_TOK_IDENT) {
      // Capturing IDENT.
      expr_ret_51 = leaf(IDENT);
      expr_ret_51->tok_repr = ctx->tokens[ctx->pos].content;
      expr_ret_51->repr_len = ctx->tokens[ctx->pos].len;
      ctx->pos++;
    } else {
      expr_ret_51 = NULL;
    }

    expr_ret_50 = expr_ret_51;
    id = expr_ret_51;
    // ModExprList 1
    if (expr_ret_50) {
      if (ctx->pos < ctx->len && ctx->tokens[ctx->pos].kind == PL0_TOK_CEQ) {
        // Not capturing CEQ.
        expr_ret_50 = SUCC;
        ctx->pos++;
      } else {
        expr_ret_50 = NULL;
      }

    }

    // ModExprList 2
    if (expr_ret_50) {
      pl0_astnode_t* expr_ret_52 = NULL;
      expr_ret_52 = pl0_parse_expression(ctx);
      if (ctx->exit) return NULL;
      expr_ret_50 = expr_ret_52;
      e = expr_ret_52;
    }

    // ModExprList 3
    if (expr_ret_50) {
      // CodeExpr
      #define ret expr_ret_50
      ret = SUCC;
      rule=node(CEQ, id, e);
      #undef ret
    }

    // ModExprList end
    if (!expr_ret_50) rew(mod_50);
    expr_ret_49 = expr_ret_50;
  }

  // SlashExpr 1
  if (!expr_ret_49) {
    pl0_astnode_t* expr_ret_53 = NULL;
    rec(mod_53);
    // ModExprList 0
    if (ctx->pos < ctx->len && ctx->tokens[ctx->pos].kind == PL0_TOK_CALL) {
      // Not capturing CALL.
      expr_ret_53 = SUCC;
      ctx->pos++;
    } else {
      expr_ret_53 = NULL;
    }

    // ModExprList 1
    if (expr_ret_53) {
      pl0_astnode_t* expr_ret_54 = NULL;
      if (ctx->pos < ctx->len && ctx->tokens[ctx->pos].kind == PL0_TOK_IDENT) {
        // Capturing IDENT.
        expr_ret_54 = leaf(IDENT);
        expr_ret_54->tok_repr = ctx->tokens[ctx->pos].content;
        expr_ret_54->repr_len = ctx->tokens[ctx->pos].len;
        ctx->pos++;
      } else {
        expr_ret_54 = NULL;
      }

      if (!expr_ret_54) {
        FATAL("Expected an identifier");
        return NULL;
      }
      expr_ret_53 = expr_ret_54;
      id = expr_ret_54;
    }

    // ModExprList 2
    if (expr_ret_53) {
      // CodeExpr
      #define ret expr_ret_53
      ret = SUCC;
      rule=node(CALL, id);
      #undef ret
    }

    // ModExprList end
    if (!expr_ret_53) rew(mod_53);
    expr_ret_49 = expr_ret_53;
  }

  // SlashExpr 2
  if (!expr_ret_49) {
    pl0_astnode_t* expr_ret_55 = NULL;
    rec(mod_55);
    // ModExprList 0
    if (ctx->pos < ctx->len && ctx->tokens[ctx->pos].kind == PL0_TOK_WRITE) {
      // Not capturing WRITE.
      expr_ret_55 = SUCC;
      ctx->pos++;
    } else {
      expr_ret_55 = NULL;
    }

    // ModExprList 1
    if (expr_ret_55) {
      pl0_astnode_t* expr_ret_56 = NULL;
      if (ctx->pos < ctx->len && ctx->tokens[ctx->pos].kind == PL0_TOK_IDENT) {
        // Capturing IDENT.
        expr_ret_56 = leaf(IDENT);
        expr_ret_56->tok_repr = ctx->tokens[ctx->pos].content;
        expr_ret_56->repr_len = ctx->tokens[ctx->pos].len;
        ctx->pos++;
      } else {
        expr_ret_56 = NULL;
      }

      if (!expr_ret_56) {
        FATAL("Expected an identifier");
        return NULL;
      }
      expr_ret_55 = expr_ret_56;
      id = expr_ret_56;
    }

    // ModExprList 2
    if (expr_ret_55) {
      // CodeExpr
      #define ret expr_ret_55
      ret = SUCC;
      rule=node(WRITE, id);
      #undef ret
    }

    // ModExprList end
    if (!expr_ret_55) rew(mod_55);
    expr_ret_49 = expr_ret_55;
  }

  // SlashExpr 3
  if (!expr_ret_49) {
    pl0_astnode_t* expr_ret_57 = NULL;
    rec(mod_57);
    // ModExprList 0
    if (ctx->pos < ctx->len && ctx->tokens[ctx->pos].kind == PL0_TOK_BEGIN) {
      // Not capturing BEGIN.
      expr_ret_57 = SUCC;
      ctx->pos++;
    } else {
      expr_ret_57 = NULL;
    }

    // ModExprList 1
    if (expr_ret_57) {
      // CodeExpr
      #define ret expr_ret_57
      ret = SUCC;
      rule=list(BEGIN);
      #undef ret
    }

    // ModExprList 2
    if (expr_ret_57) {
      pl0_astnode_t* expr_ret_58 = NULL;
      expr_ret_58 = pl0_parse_statement(ctx);
      if (ctx->exit) return NULL;
      if (!expr_ret_58) {
        FATAL("Expected a statement");
        return NULL;
      }
      expr_ret_57 = expr_ret_58;
      smt = expr_ret_58;
    }

    // ModExprList 3
    if (expr_ret_57) {
      // CodeExpr
      #define ret expr_ret_57
      ret = SUCC;
      add(rule, node(STATEMENT, smt));
      #undef ret
    }

    // ModExprList 4
    if (expr_ret_57) {
      pl0_astnode_t* expr_ret_59 = NULL;
      pl0_astnode_t* expr_ret_60 = SUCC;
      while (expr_ret_60)
      {
        rec(kleene_rew_59);
        pl0_astnode_t* expr_ret_61 = NULL;
        rec(mod_61);
        // ModExprList 0
        if (ctx->pos < ctx->len && ctx->tokens[ctx->pos].kind == PL0_TOK_SEMI) {
          // Not capturing SEMI.
          expr_ret_61 = SUCC;
          ctx->pos++;
        } else {
          expr_ret_61 = NULL;
        }

        // ModExprList 1
        if (expr_ret_61) {
          pl0_astnode_t* expr_ret_62 = NULL;
          expr_ret_62 = pl0_parse_statement(ctx);
          if (ctx->exit) return NULL;
          expr_ret_61 = expr_ret_62;
          smt = expr_ret_62;
        }

        // ModExprList 2
        if (expr_ret_61) {
          // CodeExpr
          #define ret expr_ret_61
          ret = SUCC;
          add(rule, node(STATEMENT, smt));
          #undef ret
        }

        // ModExprList end
        if (!expr_ret_61) rew(mod_61);
        expr_ret_60 = expr_ret_61;
      }

      expr_ret_59 = SUCC;
      expr_ret_57 = expr_ret_59;
    }

    // ModExprList 5
    if (expr_ret_57) {
      pl0_astnode_t* expr_ret_63 = NULL;
      if (ctx->pos < ctx->len && ctx->tokens[ctx->pos].kind == PL0_TOK_END) {
        // Capturing END.
        expr_ret_63 = leaf(END);
        expr_ret_63->tok_repr = ctx->tokens[ctx->pos].content;
        expr_ret_63->repr_len = ctx->tokens[ctx->pos].len;
        ctx->pos++;
      } else {
        expr_ret_63 = NULL;
      }

      if (!expr_ret_63) {
        FATAL("Expected end");
        return NULL;
      }
      expr_ret_57 = expr_ret_63;
    }

    // ModExprList end
    if (!expr_ret_57) rew(mod_57);
    expr_ret_49 = expr_ret_57;
  }

  // SlashExpr 4
  if (!expr_ret_49) {
    pl0_astnode_t* expr_ret_64 = NULL;
    rec(mod_64);
    // ModExprList 0
    if (ctx->pos < ctx->len && ctx->tokens[ctx->pos].kind == PL0_TOK_IF) {
      // Not capturing IF.
      expr_ret_64 = SUCC;
      ctx->pos++;
    } else {
      expr_ret_64 = NULL;
    }

    // ModExprList 1
    if (expr_ret_64) {
      pl0_astnode_t* expr_ret_65 = NULL;
      expr_ret_65 = pl0_parse_condition(ctx);
      if (ctx->exit) return NULL;
      if (!expr_ret_65) {
        FATAL("Expected a condition");
        return NULL;
      }
      expr_ret_64 = expr_ret_65;
      c = expr_ret_65;
    }

    // ModExprList 2
    if (expr_ret_64) {
      pl0_astnode_t* expr_ret_66 = NULL;
      if (ctx->pos < ctx->len && ctx->tokens[ctx->pos].kind == PL0_TOK_THEN) {
        // Not capturing THEN.
        expr_ret_66 = SUCC;
        ctx->pos++;
      } else {
        expr_ret_66 = NULL;
      }

      if (!expr_ret_66) {
        FATAL("Expected then");
        return NULL;
      }
      expr_ret_64 = expr_ret_66;
    }

    // ModExprList 3
    if (expr_ret_64) {
      pl0_astnode_t* expr_ret_67 = NULL;
      expr_ret_67 = pl0_parse_statement(ctx);
      if (ctx->exit) return NULL;
      if (!expr_ret_67) {
        FATAL("Expected a statement");
        return NULL;
      }
      expr_ret_64 = expr_ret_67;
      smt = expr_ret_67;
    }

    // ModExprList 4
    if (expr_ret_64) {
      // CodeExpr
      #define ret expr_ret_64
      ret = SUCC;
      rule=node(IF, c, smt);
      #undef ret
    }

    // ModExprList end
    if (!expr_ret_64) rew(mod_64);
    expr_ret_49 = expr_ret_64;
  }

  // SlashExpr 5
  if (!expr_ret_49) {
    pl0_astnode_t* expr_ret_68 = NULL;
    rec(mod_68);
    // ModExprList 0
    if (ctx->pos < ctx->len && ctx->tokens[ctx->pos].kind == PL0_TOK_WHILE) {
      // Not capturing WHILE.
      expr_ret_68 = SUCC;
      ctx->pos++;
    } else {
      expr_ret_68 = NULL;
    }

    // ModExprList 1
    if (expr_ret_68) {
      pl0_astnode_t* expr_ret_69 = NULL;
      expr_ret_69 = pl0_parse_condition(ctx);
      if (ctx->exit) return NULL;
      if (!expr_ret_69) {
        FATAL("Expected a condition");
        return NULL;
      }
      expr_ret_68 = expr_ret_69;
      c = expr_ret_69;
    }

    // ModExprList 2
    if (expr_ret_68) {
      pl0_astnode_t* expr_ret_70 = NULL;
      if (ctx->pos < ctx->len && ctx->tokens[ctx->pos].kind == PL0_TOK_DO) {
        // Not capturing DO.
        expr_ret_70 = SUCC;
        ctx->pos++;
      } else {
        expr_ret_70 = NULL;
      }

      if (!expr_ret_70) {
        FATAL("Expected do");
        return NULL;
      }
      expr_ret_68 = expr_ret_70;
    }

    // ModExprList 3
    if (expr_ret_68) {
      pl0_astnode_t* expr_ret_71 = NULL;
      expr_ret_71 = pl0_parse_statement(ctx);
      if (ctx->exit) return NULL;
      if (!expr_ret_71) {
        FATAL("Expected a statement");
        return NULL;
      }
      expr_ret_68 = expr_ret_71;
      smt = expr_ret_71;
    }

    // ModExprList 4
    if (expr_ret_68) {
      // CodeExpr
      #define ret expr_ret_68
      ret = SUCC;
      rule=node(WHILE, c, smt);
      #undef ret
    }

    // ModExprList end
    if (!expr_ret_68) rew(mod_68);
    expr_ret_49 = expr_ret_68;
  }

  // SlashExpr end
  expr_ret_48 = expr_ret_49;

  if (!rule) rule = expr_ret_48;
  if (!expr_ret_48) rule = NULL;
  return rule;
  #undef rule
}

static inline pl0_astnode_t* pl0_parse_condition(pl0_parser_ctx* ctx) {
  pl0_astnode_t* ex = NULL;
  pl0_astnode_t* op = NULL;
  pl0_astnode_t* ex_ = NULL;
  #define rule expr_ret_72
  pl0_astnode_t* expr_ret_72 = NULL;
  pl0_astnode_t* expr_ret_73 = NULL;
  pl0_astnode_t* expr_ret_74 = NULL;

  // SlashExpr 0
  if (!expr_ret_74) {
    pl0_astnode_t* expr_ret_75 = NULL;
    rec(mod_75);
    // ModExprList 0
    if (ctx->pos < ctx->len && ctx->tokens[ctx->pos].kind == PL0_TOK_ODD) {
      // Not capturing ODD.
      expr_ret_75 = SUCC;
      ctx->pos++;
    } else {
      expr_ret_75 = NULL;
    }

    // ModExprList 1
    if (expr_ret_75) {
      pl0_astnode_t* expr_ret_76 = NULL;
      expr_ret_76 = pl0_parse_expression(ctx);
      if (ctx->exit) return NULL;
      expr_ret_75 = expr_ret_76;
      ex = expr_ret_76;
    }

    // ModExprList 2
    if (expr_ret_75) {
      // CodeExpr
      #define ret expr_ret_75
      ret = SUCC;
      rule = node(UNEXPR, ex);;
      #undef ret
    }

    // ModExprList end
    if (!expr_ret_75) rew(mod_75);
    expr_ret_74 = expr_ret_75;
  }

  // SlashExpr 1
  if (!expr_ret_74) {
    pl0_astnode_t* expr_ret_77 = NULL;
    rec(mod_77);
    // ModExprList 0
    pl0_astnode_t* expr_ret_78 = NULL;
    expr_ret_78 = pl0_parse_expression(ctx);
    if (ctx->exit) return NULL;
    expr_ret_77 = expr_ret_78;
    ex = expr_ret_78;
    // ModExprList 1
    if (expr_ret_77) {
      pl0_astnode_t* expr_ret_79 = NULL;
      pl0_astnode_t* expr_ret_80 = NULL;

      // SlashExpr 0
      if (!expr_ret_80) {
        pl0_astnode_t* expr_ret_81 = NULL;
        rec(mod_81);
        // ModExprList Forwarding
        if (ctx->pos < ctx->len && ctx->tokens[ctx->pos].kind == PL0_TOK_EQ) {
          // Capturing EQ.
          expr_ret_81 = leaf(EQ);
          expr_ret_81->tok_repr = ctx->tokens[ctx->pos].content;
          expr_ret_81->repr_len = ctx->tokens[ctx->pos].len;
          ctx->pos++;
        } else {
          expr_ret_81 = NULL;
        }

        // ModExprList end
        if (!expr_ret_81) rew(mod_81);
        expr_ret_80 = expr_ret_81;
      }

      // SlashExpr 1
      if (!expr_ret_80) {
        pl0_astnode_t* expr_ret_82 = NULL;
        rec(mod_82);
        // ModExprList Forwarding
        if (ctx->pos < ctx->len && ctx->tokens[ctx->pos].kind == PL0_TOK_HASH) {
          // Capturing HASH.
          expr_ret_82 = leaf(HASH);
          expr_ret_82->tok_repr = ctx->tokens[ctx->pos].content;
          expr_ret_82->repr_len = ctx->tokens[ctx->pos].len;
          ctx->pos++;
        } else {
          expr_ret_82 = NULL;
        }

        // ModExprList end
        if (!expr_ret_82) rew(mod_82);
        expr_ret_80 = expr_ret_82;
      }

      // SlashExpr 2
      if (!expr_ret_80) {
        pl0_astnode_t* expr_ret_83 = NULL;
        rec(mod_83);
        // ModExprList Forwarding
        if (ctx->pos < ctx->len && ctx->tokens[ctx->pos].kind == PL0_TOK_LT) {
          // Capturing LT.
          expr_ret_83 = leaf(LT);
          expr_ret_83->tok_repr = ctx->tokens[ctx->pos].content;
          expr_ret_83->repr_len = ctx->tokens[ctx->pos].len;
          ctx->pos++;
        } else {
          expr_ret_83 = NULL;
        }

        // ModExprList end
        if (!expr_ret_83) rew(mod_83);
        expr_ret_80 = expr_ret_83;
      }

      // SlashExpr 3
      if (!expr_ret_80) {
        pl0_astnode_t* expr_ret_84 = NULL;
        rec(mod_84);
        // ModExprList Forwarding
        if (ctx->pos < ctx->len && ctx->tokens[ctx->pos].kind == PL0_TOK_LEQ) {
          // Capturing LEQ.
          expr_ret_84 = leaf(LEQ);
          expr_ret_84->tok_repr = ctx->tokens[ctx->pos].content;
          expr_ret_84->repr_len = ctx->tokens[ctx->pos].len;
          ctx->pos++;
        } else {
          expr_ret_84 = NULL;
        }

        // ModExprList end
        if (!expr_ret_84) rew(mod_84);
        expr_ret_80 = expr_ret_84;
      }

      // SlashExpr 4
      if (!expr_ret_80) {
        pl0_astnode_t* expr_ret_85 = NULL;
        rec(mod_85);
        // ModExprList Forwarding
        if (ctx->pos < ctx->len && ctx->tokens[ctx->pos].kind == PL0_TOK_GT) {
          // Capturing GT.
          expr_ret_85 = leaf(GT);
          expr_ret_85->tok_repr = ctx->tokens[ctx->pos].content;
          expr_ret_85->repr_len = ctx->tokens[ctx->pos].len;
          ctx->pos++;
        } else {
          expr_ret_85 = NULL;
        }

        // ModExprList end
        if (!expr_ret_85) rew(mod_85);
        expr_ret_80 = expr_ret_85;
      }

      // SlashExpr 5
      if (!expr_ret_80) {
        pl0_astnode_t* expr_ret_86 = NULL;
        rec(mod_86);
        // ModExprList Forwarding
        if (ctx->pos < ctx->len && ctx->tokens[ctx->pos].kind == PL0_TOK_GEQ) {
          // Capturing GEQ.
          expr_ret_86 = leaf(GEQ);
          expr_ret_86->tok_repr = ctx->tokens[ctx->pos].content;
          expr_ret_86->repr_len = ctx->tokens[ctx->pos].len;
          ctx->pos++;
        } else {
          expr_ret_86 = NULL;
        }

        // ModExprList end
        if (!expr_ret_86) rew(mod_86);
        expr_ret_80 = expr_ret_86;
      }

      // SlashExpr end
      expr_ret_79 = expr_ret_80;

      expr_ret_77 = expr_ret_79;
      op = expr_ret_79;
    }

    // ModExprList 2
    if (expr_ret_77) {
      pl0_astnode_t* expr_ret_87 = NULL;
      expr_ret_87 = pl0_parse_expression(ctx);
      if (ctx->exit) return NULL;
      if (!expr_ret_87) {
        FATAL("Expected an expression");
        return NULL;
      }
      expr_ret_77 = expr_ret_87;
      ex_ = expr_ret_87;
    }

    // ModExprList 3
    if (expr_ret_77) {
      // CodeExpr
      #define ret expr_ret_77
      ret = SUCC;
      rule=node(BINEXPR, op, ex, ex_);
      #undef ret
    }

    // ModExprList end
    if (!expr_ret_77) rew(mod_77);
    expr_ret_74 = expr_ret_77;
  }

  // SlashExpr end
  expr_ret_73 = expr_ret_74;

  if (!rule) rule = expr_ret_73;
  if (!expr_ret_73) rule = NULL;
  return rule;
  #undef rule
}

static inline pl0_astnode_t* pl0_parse_expression(pl0_parser_ctx* ctx) {
  pl0_astnode_t* pm = NULL;
  pl0_astnode_t* t = NULL;
  #define rule expr_ret_88
  pl0_astnode_t* expr_ret_88 = NULL;
  pl0_astnode_t* expr_ret_89 = NULL;
  pl0_astnode_t* expr_ret_90 = NULL;
  rec(mod_90);
  // ModExprList 0
  // CodeExpr
  #define ret expr_ret_90
  ret = SUCC;
  rule=list(EXPRS);
  #undef ret
  // ModExprList 1
  if (expr_ret_90) {
    pl0_astnode_t* expr_ret_91 = NULL;
    pl0_astnode_t* expr_ret_92 = NULL;

    // SlashExpr 0
    if (!expr_ret_92) {
      pl0_astnode_t* expr_ret_93 = NULL;
      rec(mod_93);
      // ModExprList Forwarding
      if (ctx->pos < ctx->len && ctx->tokens[ctx->pos].kind == PL0_TOK_PLUS) {
        // Not capturing PLUS.
        expr_ret_93 = SUCC;
        ctx->pos++;
      } else {
        expr_ret_93 = NULL;
      }

      // ModExprList end
      if (!expr_ret_93) rew(mod_93);
      expr_ret_92 = expr_ret_93;
    }

    // SlashExpr 1
    if (!expr_ret_92) {
      pl0_astnode_t* expr_ret_94 = NULL;
      rec(mod_94);
      // ModExprList Forwarding
      if (ctx->pos < ctx->len && ctx->tokens[ctx->pos].kind == PL0_TOK_MINUS) {
        // Not capturing MINUS.
        expr_ret_94 = SUCC;
        ctx->pos++;
      } else {
        expr_ret_94 = NULL;
      }

      // ModExprList end
      if (!expr_ret_94) rew(mod_94);
      expr_ret_92 = expr_ret_94;
    }

    // SlashExpr end
    expr_ret_91 = expr_ret_92;

    // optional
    if (!expr_ret_91)
      expr_ret_91 = SUCC;
    expr_ret_90 = expr_ret_91;
    pm = expr_ret_91;
  }

  // ModExprList 2
  if (expr_ret_90) {
    pl0_astnode_t* expr_ret_95 = NULL;
    expr_ret_95 = pl0_parse_term(ctx);
    if (ctx->exit) return NULL;
    expr_ret_90 = expr_ret_95;
    t = expr_ret_95;
  }

  // ModExprList 3
  if (expr_ret_90) {
    // CodeExpr
    #define ret expr_ret_90
    ret = SUCC;
    add(rule, has(pm) ? node(UNEXPR, pm, t) : t);
    #undef ret
  }

  // ModExprList 4
  if (expr_ret_90) {
    pl0_astnode_t* expr_ret_96 = NULL;
    pl0_astnode_t* expr_ret_97 = SUCC;
    while (expr_ret_97)
    {
      rec(kleene_rew_96);
      pl0_astnode_t* expr_ret_98 = NULL;
      rec(mod_98);
      // ModExprList 0
      pl0_astnode_t* expr_ret_99 = NULL;
      pl0_astnode_t* expr_ret_100 = NULL;

      // SlashExpr 0
      if (!expr_ret_100) {
        pl0_astnode_t* expr_ret_101 = NULL;
        rec(mod_101);
        // ModExprList Forwarding
        if (ctx->pos < ctx->len && ctx->tokens[ctx->pos].kind == PL0_TOK_PLUS) {
          // Capturing PLUS.
          expr_ret_101 = leaf(PLUS);
          expr_ret_101->tok_repr = ctx->tokens[ctx->pos].content;
          expr_ret_101->repr_len = ctx->tokens[ctx->pos].len;
          ctx->pos++;
        } else {
          expr_ret_101 = NULL;
        }

        // ModExprList end
        if (!expr_ret_101) rew(mod_101);
        expr_ret_100 = expr_ret_101;
      }

      // SlashExpr 1
      if (!expr_ret_100) {
        pl0_astnode_t* expr_ret_102 = NULL;
        rec(mod_102);
        // ModExprList Forwarding
        if (ctx->pos < ctx->len && ctx->tokens[ctx->pos].kind == PL0_TOK_MINUS) {
          // Capturing MINUS.
          expr_ret_102 = leaf(MINUS);
          expr_ret_102->tok_repr = ctx->tokens[ctx->pos].content;
          expr_ret_102->repr_len = ctx->tokens[ctx->pos].len;
          ctx->pos++;
        } else {
          expr_ret_102 = NULL;
        }

        // ModExprList end
        if (!expr_ret_102) rew(mod_102);
        expr_ret_100 = expr_ret_102;
      }

      // SlashExpr end
      expr_ret_99 = expr_ret_100;

      expr_ret_98 = expr_ret_99;
      pm = expr_ret_99;
      // ModExprList 1
      if (expr_ret_98) {
        pl0_astnode_t* expr_ret_103 = NULL;
        expr_ret_103 = pl0_parse_term(ctx);
        if (ctx->exit) return NULL;
        if (!expr_ret_103) {
          FATAL("Expected a term");
          return NULL;
        }
        expr_ret_98 = expr_ret_103;
        t = expr_ret_103;
      }

      // ModExprList 2
      if (expr_ret_98) {
        // CodeExpr
        #define ret expr_ret_98
        ret = SUCC;
        add(rule, has(pm) ? node(BINEXPR, pm, t) : t);
        #undef ret
      }

      // ModExprList end
      if (!expr_ret_98) rew(mod_98);
      expr_ret_97 = expr_ret_98;
    }

    expr_ret_96 = SUCC;
    expr_ret_90 = expr_ret_96;
  }

  // ModExprList end
  if (!expr_ret_90) rew(mod_90);
  expr_ret_89 = expr_ret_90;
  if (!rule) rule = expr_ret_89;
  if (!expr_ret_89) rule = NULL;
  return rule;
  #undef rule
}

static inline pl0_astnode_t* pl0_parse_term(pl0_parser_ctx* ctx) {
  pl0_astnode_t* f = NULL;
  pl0_astnode_t* sd = NULL;
  #define rule expr_ret_104
  pl0_astnode_t* expr_ret_104 = NULL;
  pl0_astnode_t* expr_ret_105 = NULL;
  pl0_astnode_t* expr_ret_106 = NULL;
  rec(mod_106);
  // ModExprList 0
  // CodeExpr
  #define ret expr_ret_106
  ret = SUCC;
  rule=list(EXPRS);
  #undef ret
  // ModExprList 1
  if (expr_ret_106) {
    pl0_astnode_t* expr_ret_107 = NULL;
    expr_ret_107 = pl0_parse_factor(ctx);
    if (ctx->exit) return NULL;
    expr_ret_106 = expr_ret_107;
    f = expr_ret_107;
  }

  // ModExprList 2
  if (expr_ret_106) {
    // CodeExpr
    #define ret expr_ret_106
    ret = SUCC;
    add(rule, f);
    #undef ret
  }

  // ModExprList 3
  if (expr_ret_106) {
    pl0_astnode_t* expr_ret_108 = NULL;
    pl0_astnode_t* expr_ret_109 = SUCC;
    while (expr_ret_109)
    {
      rec(kleene_rew_108);
      pl0_astnode_t* expr_ret_110 = NULL;
      rec(mod_110);
      // ModExprList 0
      pl0_astnode_t* expr_ret_111 = NULL;
      pl0_astnode_t* expr_ret_112 = NULL;

      // SlashExpr 0
      if (!expr_ret_112) {
        pl0_astnode_t* expr_ret_113 = NULL;
        rec(mod_113);
        // ModExprList Forwarding
        if (ctx->pos < ctx->len && ctx->tokens[ctx->pos].kind == PL0_TOK_STAR) {
          // Capturing STAR.
          expr_ret_113 = leaf(STAR);
          expr_ret_113->tok_repr = ctx->tokens[ctx->pos].content;
          expr_ret_113->repr_len = ctx->tokens[ctx->pos].len;
          ctx->pos++;
        } else {
          expr_ret_113 = NULL;
        }

        // ModExprList end
        if (!expr_ret_113) rew(mod_113);
        expr_ret_112 = expr_ret_113;
      }

      // SlashExpr 1
      if (!expr_ret_112) {
        pl0_astnode_t* expr_ret_114 = NULL;
        rec(mod_114);
        // ModExprList Forwarding
        if (ctx->pos < ctx->len && ctx->tokens[ctx->pos].kind == PL0_TOK_DIV) {
          // Capturing DIV.
          expr_ret_114 = leaf(DIV);
          expr_ret_114->tok_repr = ctx->tokens[ctx->pos].content;
          expr_ret_114->repr_len = ctx->tokens[ctx->pos].len;
          ctx->pos++;
        } else {
          expr_ret_114 = NULL;
        }

        // ModExprList end
        if (!expr_ret_114) rew(mod_114);
        expr_ret_112 = expr_ret_114;
      }

      // SlashExpr end
      expr_ret_111 = expr_ret_112;

      expr_ret_110 = expr_ret_111;
      sd = expr_ret_111;
      // ModExprList 1
      if (expr_ret_110) {
        pl0_astnode_t* expr_ret_115 = NULL;
        expr_ret_115 = pl0_parse_factor(ctx);
        if (ctx->exit) return NULL;
        if (!expr_ret_115) {
          FATAL("Expected a factor");
          return NULL;
        }
        expr_ret_110 = expr_ret_115;
        f = expr_ret_115;
      }

      // ModExprList 2
      if (expr_ret_110) {
        // CodeExpr
        #define ret expr_ret_110
        ret = SUCC;
        add(rule, node(TERM, sd, f));
        #undef ret
      }

      // ModExprList end
      if (!expr_ret_110) rew(mod_110);
      expr_ret_109 = expr_ret_110;
    }

    expr_ret_108 = SUCC;
    expr_ret_106 = expr_ret_108;
  }

  // ModExprList end
  if (!expr_ret_106) rew(mod_106);
  expr_ret_105 = expr_ret_106;
  if (!rule) rule = expr_ret_105;
  if (!expr_ret_105) rule = NULL;
  return rule;
  #undef rule
}

static inline pl0_astnode_t* pl0_parse_factor(pl0_parser_ctx* ctx) {
  pl0_astnode_t* i = NULL;
  pl0_astnode_t* n = NULL;
  pl0_astnode_t* e = NULL;
  #define rule expr_ret_116
  pl0_astnode_t* expr_ret_116 = NULL;
  pl0_astnode_t* expr_ret_117 = NULL;
  pl0_astnode_t* expr_ret_118 = NULL;

  // SlashExpr 0
  if (!expr_ret_118) {
    pl0_astnode_t* expr_ret_119 = NULL;
    rec(mod_119);
    // ModExprList Forwarding
    pl0_astnode_t* expr_ret_120 = NULL;
    if (ctx->pos < ctx->len && ctx->tokens[ctx->pos].kind == PL0_TOK_IDENT) {
      // Capturing IDENT.
      expr_ret_120 = leaf(IDENT);
      expr_ret_120->tok_repr = ctx->tokens[ctx->pos].content;
      expr_ret_120->repr_len = ctx->tokens[ctx->pos].len;
      ctx->pos++;
    } else {
      expr_ret_120 = NULL;
    }

    expr_ret_119 = expr_ret_120;
    i = expr_ret_120;
    // ModExprList end
    if (!expr_ret_119) rew(mod_119);
    expr_ret_118 = expr_ret_119;
  }

  // SlashExpr 1
  if (!expr_ret_118) {
    pl0_astnode_t* expr_ret_121 = NULL;
    rec(mod_121);
    // ModExprList Forwarding
    pl0_astnode_t* expr_ret_122 = NULL;
    if (ctx->pos < ctx->len && ctx->tokens[ctx->pos].kind == PL0_TOK_NUM) {
      // Capturing NUM.
      expr_ret_122 = leaf(NUM);
      expr_ret_122->tok_repr = ctx->tokens[ctx->pos].content;
      expr_ret_122->repr_len = ctx->tokens[ctx->pos].len;
      ctx->pos++;
    } else {
      expr_ret_122 = NULL;
    }

    expr_ret_121 = expr_ret_122;
    n = expr_ret_122;
    // ModExprList end
    if (!expr_ret_121) rew(mod_121);
    expr_ret_118 = expr_ret_121;
  }

  // SlashExpr 2
  if (!expr_ret_118) {
    pl0_astnode_t* expr_ret_123 = NULL;
    rec(mod_123);
    // ModExprList 0
    if (ctx->pos < ctx->len && ctx->tokens[ctx->pos].kind == PL0_TOK_OPEN) {
      // Not capturing OPEN.
      expr_ret_123 = SUCC;
      ctx->pos++;
    } else {
      expr_ret_123 = NULL;
    }

    // ModExprList 1
    if (expr_ret_123) {
      pl0_astnode_t* expr_ret_124 = NULL;
      expr_ret_124 = pl0_parse_expression(ctx);
      if (ctx->exit) return NULL;
      expr_ret_123 = expr_ret_124;
      e = expr_ret_124;
    }

    // ModExprList 2
    if (expr_ret_123) {
      pl0_astnode_t* expr_ret_125 = NULL;
      if (ctx->pos < ctx->len && ctx->tokens[ctx->pos].kind == PL0_TOK_CLOSE) {
        // Not capturing CLOSE.
        expr_ret_125 = SUCC;
        ctx->pos++;
      } else {
        expr_ret_125 = NULL;
      }

      if (!expr_ret_125) {
        FATAL("Expected closing parentheses");
        return NULL;
      }
      expr_ret_123 = expr_ret_125;
    }

    // ModExprList 3
    if (expr_ret_123) {
      // CodeExpr
      #define ret expr_ret_123
      ret = SUCC;
      rule=e;
      #undef ret
    }

    // ModExprList end
    if (!expr_ret_123) rew(mod_123);
    expr_ret_118 = expr_ret_123;
  }

  // SlashExpr end
  expr_ret_117 = expr_ret_118;

  if (!rule) rule = expr_ret_117;
  if (!expr_ret_117) rule = NULL;
  return rule;
  #undef rule
}




#undef rec
#undef rew
#undef node
#undef kind
#undef list
#undef leaf
#undef add
#undef has
#undef expect
#undef repr
#undef srepr
#undef cprepr
#undef rret
#undef SUCC

#undef PGEN_MIN
#undef PGEN_MAX
#undef PGEN_MIN1
#undef PGEN_MAX1
#undef PGEN_MIN2
#undef PGEN_MAX2
#undef PGEN_MIN3
#undef PGEN_MAX3
#undef PGEN_MIN4
#undef PGEN_MAX4
#undef PGEN_MIN5
#undef PGEN_MAX5

#undef LB
#undef RB

#undef INFO
#undef INFO_F
#undef WARNING
#undef WARNING_F
#undef ERROR
#undef ERROR_F
#undef FATAL
#undef FATAL_F
#endif /* PGEN_PL0_ASTNODE_INCLUDE */

#endif /* PGEN_PL0_PARSER_H */
