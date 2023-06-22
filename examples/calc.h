#ifndef PGEN_CALC_PARSER_H
#define PGEN_CALC_PARSER_H


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
#define PGEN_INTERACTIVE 1

#define PGEN_ALLOCATOR_DEBUG 0

#endif /* PGEN_INTERACTIVE */


/* START OF AST ALLOCATOR */

#ifndef PGEN_ARENA_INCLUDED
#define PGEN_ARENA_INCLUDED
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


struct calc_astnode_t;
typedef struct calc_astnode_t calc_astnode_t;


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

#ifndef CALC_TOKENIZER_INCLUDE
#define CALC_TOKENIZER_INCLUDE

typedef enum {
  CALC_TOK_STREAMBEGIN,
  CALC_TOK_STREAMEND,
  CALC_TOK_PLUS,
  CALC_TOK_MINUS,
  CALC_TOK_MULT,
  CALC_TOK_DIV,
  CALC_TOK_OPEN,
  CALC_TOK_CLOSE,
  CALC_TOK_NUMBER,
  CALC_TOK_WS,
} calc_token_kind;

// The 0th token is beginning of stream.
// The 1st token isend of stream.
// Tokens 1 through 8 are the ones you defined.
// This totals 10 kinds of tokens.
#define CALC_NUM_TOKENKINDS 10
static const char* calc_tokenkind_name[CALC_NUM_TOKENKINDS] = {
  "STREAMBEGIN",
  "STREAMEND",
  "PLUS",
  "MINUS",
  "MULT",
  "DIV",
  "OPEN",
  "CLOSE",
  "NUMBER",
  "WS",
};

typedef struct {
  calc_token_kind kind;
  codepoint_t* content; // The token begins at tokenizer->start[token->start].
  size_t len;
  size_t line;
  size_t col;
} calc_token;

typedef struct {
  codepoint_t* start;
  size_t len;
  size_t pos;
  size_t pos_line;
  size_t pos_col;
} calc_tokenizer;

static inline void calc_tokenizer_init(calc_tokenizer* tokenizer, codepoint_t* start, size_t len) {
  tokenizer->start = start;
  tokenizer->len = len;
  tokenizer->pos = 0;
  tokenizer->pos_line = 1;
  tokenizer->pos_col = 0;
}

static inline calc_token calc_nextToken(calc_tokenizer* tokenizer) {
  codepoint_t* current = tokenizer->start + tokenizer->pos;
  size_t remaining = tokenizer->len - tokenizer->pos;

  int trie_state = 0;
  int smaut_state_0 = 0;
  int smaut_state_1 = 0;
  size_t trie_munch_size = 0;
  size_t smaut_munch_size_0 = 0;
  size_t smaut_munch_size_1 = 0;
  calc_token_kind trie_tokenkind = CALC_TOK_STREAMEND;

  for (size_t iidx = 0; iidx < remaining; iidx++) {
    codepoint_t c = current[iidx];
    int all_dead = 1;

    // Trie
    if (trie_state != -1) {
      all_dead = 0;
      if (trie_state == 0) {
        if (c == 40 /*'('*/) trie_state = 5;
        else if (c == 41 /*')'*/) trie_state = 6;
        else if (c == 42 /*'*'*/) trie_state = 3;
        else if (c == 43 /*'+'*/) trie_state = 1;
        else if (c == 45 /*'-'*/) trie_state = 2;
        else if (c == 47 /*'/'*/) trie_state = 4;
        else trie_state = -1;
      }
      else {
        trie_state = -1;
      }

      // Check accept
      if (trie_state == 1) {
        trie_tokenkind =  CALC_TOK_PLUS;
        trie_munch_size = iidx + 1;
      }
      else if (trie_state == 2) {
        trie_tokenkind =  CALC_TOK_MINUS;
        trie_munch_size = iidx + 1;
      }
      else if (trie_state == 3) {
        trie_tokenkind =  CALC_TOK_MULT;
        trie_munch_size = iidx + 1;
      }
      else if (trie_state == 4) {
        trie_tokenkind =  CALC_TOK_DIV;
        trie_munch_size = iidx + 1;
      }
      else if (trie_state == 5) {
        trie_tokenkind =  CALC_TOK_OPEN;
        trie_munch_size = iidx + 1;
      }
      else if (trie_state == 6) {
        trie_tokenkind =  CALC_TOK_CLOSE;
        trie_munch_size = iidx + 1;
      }
    }

    // Transition NUMBER State Machine
    if (smaut_state_0 != -1) {
      all_dead = 0;

      if ((smaut_state_0 == 0) &
         ((c == '-') | (c == '+'))) {
          smaut_state_0 = 1;
      }
      else if (((smaut_state_0 >= 0) & (smaut_state_0 <= 2)) &
         ((c >= '0') & (c <= '9'))) {
          smaut_state_0 = 2;
      }
      else {
        smaut_state_0 = -1;
      }

      // Check accept
      if (smaut_state_0 == 2) {
        smaut_munch_size_0 = iidx + 1;
      }
    }

    // Transition WS State Machine
    if (smaut_state_1 != -1) {
      all_dead = 0;

      if (((smaut_state_1 == 0) | (smaut_state_1 == 1)) &
         ((c == 32) | (c == '\n') | (c == 13) | (c == 9))) {
          smaut_state_1 = 1;
      }
      else {
        smaut_state_1 = -1;
      }

      // Check accept
      if (smaut_state_1 == 1) {
        smaut_munch_size_1 = iidx + 1;
      }
    }

    if (all_dead)
      break;
  }

  // Determine what token was accepted, if any.
  calc_token_kind kind = CALC_TOK_STREAMEND;
  size_t max_munch = 0;
  if (smaut_munch_size_1 >= max_munch) {
    kind = CALC_TOK_WS;
    max_munch = smaut_munch_size_1;
  }
  if (smaut_munch_size_0 >= max_munch) {
    kind = CALC_TOK_NUMBER;
    max_munch = smaut_munch_size_0;
  }
  if (trie_munch_size >= max_munch) {
    kind = trie_tokenkind;
    max_munch = trie_munch_size;
  }

  calc_token tok;
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

#endif /* CALC_TOKENIZER_INCLUDE */

#ifndef PGEN_CALC_ASTNODE_INCLUDE
#define PGEN_CALC_ASTNODE_INCLUDE

struct calc_parse_err;
typedef struct calc_parse_err calc_parse_err;
struct calc_parse_err {
  const char* msg;
  int severity;
  size_t line;
  size_t col;
};

#ifndef CALC_MAX_PARSER_ERRORS
#define CALC_MAX_PARSER_ERRORS 20
#endif
typedef struct {
  calc_token* tokens;
  size_t len;
  size_t pos;
  int exit;
  pgen_allocator *alloc;
  size_t num_errors;
  calc_parse_err errlist[CALC_MAX_PARSER_ERRORS];
} calc_parser_ctx;

static inline void calc_parser_ctx_init(calc_parser_ctx* parser,
                                       pgen_allocator* allocator,
                                       calc_token* tokens, size_t num_tokens) {
  parser->tokens = tokens;
  parser->len = num_tokens;
  parser->pos = 0;
  parser->exit = 0;
  parser->alloc = allocator;
  parser->num_errors = 0;
  size_t to_zero = sizeof(calc_parse_err) * CALC_MAX_PARSER_ERRORS;
  memset(&parser->errlist, 0, to_zero);
}
static inline void freemsg(const char* msg, void* extra) {
  (void)extra;
  PGEN_FREE((void*)msg);
}

static inline calc_parse_err* calc_report_parse_error(calc_parser_ctx* ctx,
              const char* msg, void (*msgfree)(const char* msg, void* extra), int severity) {
  if (ctx->num_errors >= CALC_MAX_PARSER_ERRORS) {
    ctx->exit = 1;
    return NULL;
  }
  calc_parse_err* err = &ctx->errlist[ctx->num_errors++];
  err->msg = (const char*)msg;
  err->severity = severity;
  size_t toknum = ctx->pos + (ctx->pos != ctx->len - 1);
  calc_token tok = ctx->tokens[toknum];
  err->line = tok.line;
  err->col = tok.col;

  if (severity == 3)
    ctx->exit = 1;
  return err;
}

typedef enum {
  CALC_NODE_PLUS,
  CALC_NODE_MINUS,
  CALC_NODE_MULT,
  CALC_NODE_DIV,
  CALC_NODE_OPEN,
  CALC_NODE_CLOSE,
  CALC_NODE_NUMBER,
  CALC_NODE_WS,
} calc_astnode_kind;

#define CALC_NUM_NODEKINDS 8
static const char* calc_nodekind_name[CALC_NUM_NODEKINDS] = {
  "PLUS",
  "MINUS",
  "MULT",
  "DIV",
  "OPEN",
  "CLOSE",
  "NUMBER",
  "WS",
};

struct calc_astnode_t {
  calc_astnode_t* parent;
  uint16_t num_children;
  uint16_t max_children;
  calc_astnode_kind kind;

  codepoint_t* tok_repr;
  size_t repr_len;
  // No %extra directives.
  calc_astnode_t** children;
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


static inline calc_astnode_t* calc_astnode_list(
                             pgen_allocator* alloc,
                             calc_astnode_kind kind,
                             size_t initial_size) {
  char* ret = pgen_alloc(alloc,
                         sizeof(calc_astnode_t),
                         _Alignof(calc_astnode_t));
  calc_astnode_t *node = (calc_astnode_t*)ret;

  calc_astnode_t **children;
  if (initial_size) {
    children = (calc_astnode_t**)PGEN_MALLOC(sizeof(calc_astnode_t*) * initial_size);
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

static inline calc_astnode_t* calc_astnode_leaf(
                             pgen_allocator* alloc,
                             calc_astnode_kind kind) {
  char* ret = pgen_alloc(alloc,
                         sizeof(calc_astnode_t),
                         _Alignof(calc_astnode_t));
  calc_astnode_t *node = (calc_astnode_t *)ret;
  calc_astnode_t *children = NULL;
  node->kind = kind;
  node->parent = NULL;
  node->max_children = 0;
  node->num_children = 0;
  node->children = NULL;
  node->tok_repr = NULL;
  node->repr_len = 0;
  return node;
}

static inline calc_astnode_t* calc_astnode_fixed_1(
                             pgen_allocator* alloc,
                             calc_astnode_kind kind,
                             calc_astnode_t* PGEN_RESTRICT n0) {
  char* ret = pgen_alloc(alloc,
                         sizeof(calc_astnode_t) +
                         sizeof(calc_astnode_t *) * 1,
                         _Alignof(calc_astnode_t));
  calc_astnode_t *node = (calc_astnode_t *)ret;
  calc_astnode_t **children = (calc_astnode_t **)(node + 1);
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

static inline calc_astnode_t* calc_astnode_fixed_2(
                             pgen_allocator* alloc,
                             calc_astnode_kind kind,
                             calc_astnode_t* PGEN_RESTRICT n0,
                             calc_astnode_t* PGEN_RESTRICT n1) {
  char* ret = pgen_alloc(alloc,
                         sizeof(calc_astnode_t) +
                         sizeof(calc_astnode_t *) * 2,
                         _Alignof(calc_astnode_t));
  calc_astnode_t *node = (calc_astnode_t *)ret;
  calc_astnode_t **children = (calc_astnode_t **)(node + 1);
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

static inline calc_astnode_t* calc_astnode_fixed_3(
                             pgen_allocator* alloc,
                             calc_astnode_kind kind,
                             calc_astnode_t* PGEN_RESTRICT n0,
                             calc_astnode_t* PGEN_RESTRICT n1,
                             calc_astnode_t* PGEN_RESTRICT n2) {
  char* ret = pgen_alloc(alloc,
                         sizeof(calc_astnode_t) +
                         sizeof(calc_astnode_t *) * 3,
                         _Alignof(calc_astnode_t));
  calc_astnode_t *node = (calc_astnode_t *)ret;
  calc_astnode_t **children = (calc_astnode_t **)(node + 1);
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

static inline calc_astnode_t* calc_astnode_fixed_4(
                             pgen_allocator* alloc,
                             calc_astnode_kind kind,
                             calc_astnode_t* PGEN_RESTRICT n0,
                             calc_astnode_t* PGEN_RESTRICT n1,
                             calc_astnode_t* PGEN_RESTRICT n2,
                             calc_astnode_t* PGEN_RESTRICT n3) {
  char* ret = pgen_alloc(alloc,
                         sizeof(calc_astnode_t) +
                         sizeof(calc_astnode_t *) * 4,
                         _Alignof(calc_astnode_t));
  calc_astnode_t *node = (calc_astnode_t *)ret;
  calc_astnode_t **children = (calc_astnode_t **)(node + 1);
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

static inline calc_astnode_t* calc_astnode_fixed_5(
                             pgen_allocator* alloc,
                             calc_astnode_kind kind,
                             calc_astnode_t* PGEN_RESTRICT n0,
                             calc_astnode_t* PGEN_RESTRICT n1,
                             calc_astnode_t* PGEN_RESTRICT n2,
                             calc_astnode_t* PGEN_RESTRICT n3,
                             calc_astnode_t* PGEN_RESTRICT n4) {
  char* ret = pgen_alloc(alloc,
                         sizeof(calc_astnode_t) +
                         sizeof(calc_astnode_t *) * 5,
                         _Alignof(calc_astnode_t));
  calc_astnode_t *node = (calc_astnode_t *)ret;
  calc_astnode_t **children = (calc_astnode_t **)(node + 1);
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

static inline void calc_astnode_add(pgen_allocator* alloc, calc_astnode_t *list, calc_astnode_t *node) {
  if (list->max_children == list->num_children) {
    // Figure out the new size. Check for overflow where applicable.
    uint64_t new_max = (uint64_t)list->max_children * 2;
    if (new_max > UINT16_MAX || new_max > SIZE_MAX) PGEN_OOM();
    if (SIZE_MAX < UINT16_MAX && (size_t)new_max > SIZE_MAX / sizeof(calc_astnode_t)) PGEN_OOM();
    size_t new_bytes = (size_t)new_max * sizeof(calc_astnode_t);

    // Reallocate the list, and inform the allocator.
    void* old_ptr = list->children;
    void* new_ptr = realloc(list->children, new_bytes);
    if (!new_ptr) PGEN_OOM();
    list->children = (calc_astnode_t **)new_ptr;
    list->max_children = (uint16_t)new_max;
    pgen_allocator_realloced(alloc, old_ptr, new_ptr, free);
  }
  node->parent = list;
  list->children[list->num_children++] = node;
}

static inline void calc_parser_rewind(calc_parser_ctx *ctx, pgen_parser_rewind_t rew) {
  pgen_allocator_rewind(ctx->alloc, rew.arew);
  ctx->pos = rew.prew;
}

static inline calc_astnode_t* calc_astnode_repr(calc_astnode_t* node, calc_astnode_t* t) {
  node->tok_repr = t->tok_repr;
  node->repr_len = t->repr_len;
  return node;
}

static inline calc_astnode_t* calc_astnode_cprepr(calc_astnode_t* node, codepoint_t* cps, size_t repr_len) {
  node->tok_repr = cps;
  node->repr_len = repr_len;
  return node;
}

static inline calc_astnode_t* calc_astnode_srepr(pgen_allocator* allocator, calc_astnode_t* node, char* s) {
  size_t cpslen = strlen(s);
  codepoint_t* cps = (codepoint_t*)pgen_alloc(allocator, (cpslen + 1) * sizeof(codepoint_t), _Alignof(codepoint_t));
  for (size_t i = 0; i < cpslen; i++) cps[i] = (codepoint_t)s[i];
  cps[cpslen] = 0;
  node->tok_repr = cps;
  node->repr_len = cpslen;
  return node;
}

static inline int calc_node_print_content(calc_astnode_t* node, calc_token* tokens) {
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

static inline int calc_astnode_print_h(calc_token* tokens, calc_astnode_t *node, size_t depth, int fl) {
  #define indent() for (size_t i = 0; i < depth; i++) printf("  ")
  if (!node)
    return 0;
  else if (node == (calc_astnode_t*)(void*)(uintptr_t)_Alignof(calc_astnode_t))
    puts("ERROR, CAPTURED SUCC."), exit(1);

  indent(); puts("{");
  depth++;
  indent(); printf("\"kind\": "); printf("\"%s\",\n", calc_nodekind_name[node->kind]);
  if (!(!node->tok_repr & !node->repr_len)) {
    indent();
    printf("\"content\": \"");
    calc_node_print_content(node, tokens);
    printf("\",\n");
  }
  size_t cnum = node->num_children;
  if (cnum) {
    indent(); printf("\"num_children\": %zu,\n", cnum);
    indent(); printf("\"children\": [");
    putchar('\n');
    for (size_t i = 0; i < cnum; i++)
      calc_astnode_print_h(tokens, node->children[i], depth + 1, i == cnum - 1);
    indent();
    printf("]\n");
  }
  depth--;
  indent(); putchar('}'); if (fl != 1) putchar(','); putchar('\n');
  return 0;
#undef indent
}

static inline void calc_astnode_print_json(calc_token* tokens, calc_astnode_t *node) {
  if (node)    calc_astnode_print_h(tokens, node, 0, 1);
  else    puts("The AST is null.");}

#define SUCC                     (calc_astnode_t*)(void*)(uintptr_t)_Alignof(calc_astnode_t)

#define rec(label)               pgen_parser_rewind_t _rew_##label = (pgen_parser_rewind_t){ctx->alloc->rew, ctx->pos};
#define rew(label)               calc_parser_rewind(ctx, _rew_##label)
#define node(kindname, ...)      PGEN_CAT(calc_astnode_fixed_, PGEN_NARG(__VA_ARGS__))(ctx->alloc, kind(kindname), __VA_ARGS__)
#define kind(name)               CALC_NODE_##name
#define list(kind)               calc_astnode_list(ctx->alloc, CALC_NODE_##kind, 16)
#define leaf(kind)               calc_astnode_leaf(ctx->alloc, CALC_NODE_##kind)
#define add(list, node)          calc_astnode_add(ctx->alloc, list, node)
#define has(node)                (((uintptr_t)node <= (uintptr_t)SUCC) ? 0 : 1)
#define repr(node, t)            calc_astnode_repr(node, t)
#define srepr(node, s)           calc_astnode_srepr(ctx->alloc, node, (char*)s)
#define cprepr(node, cps, len)   calc_astnode_cprepr(node, cps, len)
#define expect(kind, cap)        ((ctx->pos < ctx->len && ctx->tokens[ctx->pos].kind == calc_TOK_##kind) ? ctx->pos++, (cap ? cprepr(leaf(kind), NULL, ctx->pos-1) : SUCC) : NULL)

#define LB {
#define RB }

#define INFO(msg)                calc_report_parse_error(ctx, (const char*)msg, NULL,   0)
#define WARNING(msg)             calc_report_parse_error(ctx, (const char*)msg, NULL,   1)
#define ERROR(msg)               calc_report_parse_error(ctx, (const char*)msg, NULL,   2)
#define FATAL(msg)               calc_report_parse_error(ctx, (const char*)msg, NULL,   3)
#define INFO_F(msg, freefn)      calc_report_parse_error(ctx, (const char*)msg, freefn, 0)
#define WARNING_F(msg, freefn)   calc_report_parse_error(ctx, (const char*)msg, freefn, 1)
#define ERROR_F(msg, freefn)     calc_report_parse_error(ctx, (const char*)msg, freefn, 2)
#define FATAL_F(msg, freefn)     calc_report_parse_error(ctx, (const char*)msg, freefn, 3)


#define PGEN_INTERACTIVE_WIDTH 11
typedef struct {
  const char* rule_name;
  size_t pos;
} intr_entry;

static struct {
  intr_entry rules[500];
  size_t size;
  int status;
  int first;
} intr_stack;

#include <unistd.h>
#include <sys/ioctl.h>
#include <string.h>
static inline void intr_display(calc_parser_ctx* ctx, const char* last) {
  if (!intr_stack.first) intr_stack.first = 1;
  else getchar();

  struct winsize w;
  ioctl(STDOUT_FILENO, TIOCGWINSZ, &w);
  size_t width = w.ws_col;
  size_t leftwidth = (width - (1 + 3 + 1)) / 2;
  size_t rightwidth = leftwidth + (leftwidth % 2);
  size_t height = w.ws_row - 6;

// Clear screen, cursor to top left
  printf("\x1b[2J\x1b[H");

  // Write first line in color.
  if (intr_stack.status == -1) {
    printf("\x1b[31m"); // Red
    printf("Failed: %s\n", last);
  } else if (intr_stack.status == 0) {
    printf("\x1b[34m"); // Blue
    printf("Entering: %s\n", last);
  } else if (intr_stack.status == 1) {
    printf("\x1b[32m"); // Green
    printf("Accepted: %s\n", last);
  } else {
    printf("\x1b[33m"); // Green
    printf("SUCCED: %s\n", last), exit(1);
  }
  printf("\x1b[0m"); // Clear Formatting

  // Write labels and line.
  putchar('-');
  for (size_t i = 0; i < 11; i++)
    putchar('-');
  printf("-+-");
  for (size_t i = 0; i < 11; i++)
    putchar('-');
  printf("-+-");
  for (size_t i = 29; i < width; i++)
    putchar('-');
  printf(" %-11s | %-11s | %-11s", "Call Stack",  "Token Stack",  "Token Repr");
  for (size_t i = 40; i < width; i++)
    putchar(' ');
  putchar('-');
  for (size_t i = 0; i < 11; i++)
    putchar('-');
  printf("-+-");
  for (size_t i = 0; i < 11; i++)
    putchar('-');
  printf("-+-");
  for (size_t i = 29; i < width; i++)
    putchar('-');
  // Write following lines
  for (size_t i = height; i --> 0;) {
    putchar(' ');

    // Print rule stack
    if (i < intr_stack.size) {
      ssize_t d = (ssize_t)intr_stack.size - (ssize_t)height;
      size_t disp = d > 0 ? i + (size_t)d : i;
      printf("%-11s", intr_stack.rules[disp].rule_name);
    } else {
      for (size_t sp = 0; sp < 11; sp++)
        putchar(' ');
    }

    printf(" | "); // Left bar

    // Print tokens
    size_t remaining_tokens = ctx->len - ctx->pos;
    if (i < remaining_tokens) {
      calc_token tok = ctx->tokens[ctx->pos + i];
      const char *name = calc_tokenkind_name[tok.kind];
      printf("%-11s", name);
    } else {
      printf("%-11s", "");
    }
    printf(" | "); // Right bar

    // Print token content
    if (i < remaining_tokens) {
      calc_token tok = ctx->tokens[ctx->pos + i];
      if (tok.content && tok.len) {        size_t tok_content_len = 0;
        char *tok_content = NULL;
        size_t trunc_to = 17;
        int truncd = tok.len > trunc_to;
        size_t trunc_len = tok.len > trunc_to ? trunc_to : tok.len;
        UTF8_encode(tok.content, trunc_len, &tok_content, &tok_content_len);
        printf("%s%s", tok_content, truncd ? "..." : "");
        UTF8_FREE(tok_content);
      }
    }
    putchar(' ');
    putchar('\n');
  }
}

static inline void intr_enter(calc_parser_ctx* ctx, const char* name, size_t pos) {
  intr_stack.rules[intr_stack.size++] = (intr_entry){name, pos};
  intr_stack.status = 0;
  intr_display(ctx, name);
}

static inline void intr_accept(calc_parser_ctx* ctx, const char* accpeting) {
  intr_stack.size--;
  intr_stack.status = 1;
  intr_display(ctx, accpeting);
}

static inline void intr_reject(calc_parser_ctx* ctx, const char* rejecting) {
  intr_stack.size--;
  intr_stack.status = -1;
  intr_display(ctx, rejecting);
}
static inline void intr_succ(calc_parser_ctx* ctx, const char* succing) {
  intr_stack.size--;
  intr_stack.status = 2;
  intr_display(ctx, succing);
}
static inline calc_astnode_t* calc_parse_expr(calc_parser_ctx* ctx);
static inline calc_astnode_t* calc_parse_sumexpr(calc_parser_ctx* ctx);
static inline calc_astnode_t* calc_parse_multexpr(calc_parser_ctx* ctx);
static inline calc_astnode_t* calc_parse_baseexpr(calc_parser_ctx* ctx);


static inline calc_astnode_t* calc_parse_expr(calc_parser_ctx* ctx) {
  #define rule expr_ret_0
  calc_astnode_t* expr_ret_0 = NULL;
  calc_astnode_t* expr_ret_1 = NULL;
  intr_enter(ctx, "expr", ctx->pos);
  calc_astnode_t* expr_ret_2 = NULL;
  rec(mod_2);
  // ModExprList Forwarding
  expr_ret_2 = calc_parse_sumexpr(ctx);
  if (ctx->exit) return NULL;
  // ModExprList end
  if (!expr_ret_2) rew(mod_2);
  expr_ret_1 = expr_ret_2;
  if (!rule) rule = expr_ret_1;
  if (!expr_ret_1) rule = NULL;
  if (rule==SUCC) intr_succ(ctx, "expr");
  else if (rule) intr_accept(ctx, "expr");
  else intr_reject(ctx, "expr");
  return rule;
  #undef rule
}

static inline calc_astnode_t* calc_parse_sumexpr(calc_parser_ctx* ctx) {
  calc_astnode_t* rule = NULL;
  calc_astnode_t* n = NULL;
  #define rule expr_ret_3
  calc_astnode_t* expr_ret_3 = NULL;
  calc_astnode_t* expr_ret_4 = NULL;
  intr_enter(ctx, "sumexpr", ctx->pos);
  calc_astnode_t* expr_ret_5 = NULL;
  rec(mod_5);
  // ModExprList 0
  calc_astnode_t* expr_ret_6 = NULL;
  expr_ret_6 = calc_parse_multexpr(ctx);
  if (ctx->exit) return NULL;
  expr_ret_5 = expr_ret_6;
  rule = expr_ret_6;
  // ModExprList 1
  if (expr_ret_5) {
    calc_astnode_t* expr_ret_7 = NULL;
    calc_astnode_t* expr_ret_8 = SUCC;
    while (expr_ret_8)
    {
      rec(kleene_rew_7);
      calc_astnode_t* expr_ret_9 = NULL;

      // SlashExpr 0
      if (!expr_ret_9) {
        calc_astnode_t* expr_ret_10 = NULL;
        rec(mod_10);
        // ModExprList 0
        intr_enter(ctx, "PLUS", ctx->pos);
        if (ctx->pos < ctx->len && ctx->tokens[ctx->pos].kind == CALC_TOK_PLUS) {
          // Not capturing PLUS.
          expr_ret_10 = SUCC;
          ctx->pos++;
        } else {
          expr_ret_10 = NULL;
        }

        if (expr_ret_10) intr_accept(ctx, "PLUS"); else intr_reject(ctx, "PLUS");
        // ModExprList 1
        if (expr_ret_10) {
          calc_astnode_t* expr_ret_11 = NULL;
          expr_ret_11 = calc_parse_multexpr(ctx);
          if (ctx->exit) return NULL;
          expr_ret_10 = expr_ret_11;
          n = expr_ret_11;
        }

        // ModExprList 2
        if (expr_ret_10) {
          // CodeExpr
          intr_enter(ctx, "CodeExpr", ctx->pos);
          #define ret expr_ret_10
          ret = SUCC;
          rule=node(PLUS, rule, n);
          if (ret) intr_accept(ctx, "CodeExpr"); else intr_reject(ctx, "CodeExpr");
          #undef ret
        }

        // ModExprList end
        if (!expr_ret_10) rew(mod_10);
        expr_ret_9 = expr_ret_10;
      }

      // SlashExpr 1
      if (!expr_ret_9) {
        calc_astnode_t* expr_ret_12 = NULL;
        rec(mod_12);
        // ModExprList 0
        intr_enter(ctx, "MINUS", ctx->pos);
        if (ctx->pos < ctx->len && ctx->tokens[ctx->pos].kind == CALC_TOK_MINUS) {
          // Not capturing MINUS.
          expr_ret_12 = SUCC;
          ctx->pos++;
        } else {
          expr_ret_12 = NULL;
        }

        if (expr_ret_12) intr_accept(ctx, "MINUS"); else intr_reject(ctx, "MINUS");
        // ModExprList 1
        if (expr_ret_12) {
          calc_astnode_t* expr_ret_13 = NULL;
          expr_ret_13 = calc_parse_multexpr(ctx);
          if (ctx->exit) return NULL;
          expr_ret_12 = expr_ret_13;
          n = expr_ret_13;
        }

        // ModExprList 2
        if (expr_ret_12) {
          // CodeExpr
          intr_enter(ctx, "CodeExpr", ctx->pos);
          #define ret expr_ret_12
          ret = SUCC;
          rule=node(MINUS, rule, n);
          if (ret) intr_accept(ctx, "CodeExpr"); else intr_reject(ctx, "CodeExpr");
          #undef ret
        }

        // ModExprList end
        if (!expr_ret_12) rew(mod_12);
        expr_ret_9 = expr_ret_12;
      }

      // SlashExpr end
      expr_ret_8 = expr_ret_9;

    }

    expr_ret_7 = SUCC;
    expr_ret_5 = expr_ret_7;
  }

  // ModExprList end
  if (!expr_ret_5) rew(mod_5);
  expr_ret_4 = expr_ret_5;
  if (!rule) rule = expr_ret_4;
  if (!expr_ret_4) rule = NULL;
  if (rule==SUCC) intr_succ(ctx, "sumexpr");
  else if (rule) intr_accept(ctx, "sumexpr");
  else intr_reject(ctx, "sumexpr");
  return rule;
  #undef rule
}

static inline calc_astnode_t* calc_parse_multexpr(calc_parser_ctx* ctx) {
  calc_astnode_t* rule = NULL;
  calc_astnode_t* n = NULL;
  #define rule expr_ret_14
  calc_astnode_t* expr_ret_14 = NULL;
  calc_astnode_t* expr_ret_15 = NULL;
  intr_enter(ctx, "multexpr", ctx->pos);
  calc_astnode_t* expr_ret_16 = NULL;
  rec(mod_16);
  // ModExprList 0
  calc_astnode_t* expr_ret_17 = NULL;
  expr_ret_17 = calc_parse_baseexpr(ctx);
  if (ctx->exit) return NULL;
  expr_ret_16 = expr_ret_17;
  rule = expr_ret_17;
  // ModExprList 1
  if (expr_ret_16) {
    calc_astnode_t* expr_ret_18 = NULL;
    calc_astnode_t* expr_ret_19 = SUCC;
    while (expr_ret_19)
    {
      rec(kleene_rew_18);
      calc_astnode_t* expr_ret_20 = NULL;

      // SlashExpr 0
      if (!expr_ret_20) {
        calc_astnode_t* expr_ret_21 = NULL;
        rec(mod_21);
        // ModExprList 0
        intr_enter(ctx, "MULT", ctx->pos);
        if (ctx->pos < ctx->len && ctx->tokens[ctx->pos].kind == CALC_TOK_MULT) {
          // Not capturing MULT.
          expr_ret_21 = SUCC;
          ctx->pos++;
        } else {
          expr_ret_21 = NULL;
        }

        if (expr_ret_21) intr_accept(ctx, "MULT"); else intr_reject(ctx, "MULT");
        // ModExprList 1
        if (expr_ret_21) {
          calc_astnode_t* expr_ret_22 = NULL;
          expr_ret_22 = calc_parse_baseexpr(ctx);
          if (ctx->exit) return NULL;
          expr_ret_21 = expr_ret_22;
          n = expr_ret_22;
        }

        // ModExprList 2
        if (expr_ret_21) {
          // CodeExpr
          intr_enter(ctx, "CodeExpr", ctx->pos);
          #define ret expr_ret_21
          ret = SUCC;
          rule=node(MULT, rule, n);
          if (ret) intr_accept(ctx, "CodeExpr"); else intr_reject(ctx, "CodeExpr");
          #undef ret
        }

        // ModExprList end
        if (!expr_ret_21) rew(mod_21);
        expr_ret_20 = expr_ret_21;
      }

      // SlashExpr 1
      if (!expr_ret_20) {
        calc_astnode_t* expr_ret_23 = NULL;
        rec(mod_23);
        // ModExprList 0
        intr_enter(ctx, "DIV", ctx->pos);
        if (ctx->pos < ctx->len && ctx->tokens[ctx->pos].kind == CALC_TOK_DIV) {
          // Not capturing DIV.
          expr_ret_23 = SUCC;
          ctx->pos++;
        } else {
          expr_ret_23 = NULL;
        }

        if (expr_ret_23) intr_accept(ctx, "DIV"); else intr_reject(ctx, "DIV");
        // ModExprList 1
        if (expr_ret_23) {
          calc_astnode_t* expr_ret_24 = NULL;
          expr_ret_24 = calc_parse_baseexpr(ctx);
          if (ctx->exit) return NULL;
          expr_ret_23 = expr_ret_24;
          n = expr_ret_24;
        }

        // ModExprList 2
        if (expr_ret_23) {
          // CodeExpr
          intr_enter(ctx, "CodeExpr", ctx->pos);
          #define ret expr_ret_23
          ret = SUCC;
          rule=node(DIV,  rule, n);
          if (ret) intr_accept(ctx, "CodeExpr"); else intr_reject(ctx, "CodeExpr");
          #undef ret
        }

        // ModExprList end
        if (!expr_ret_23) rew(mod_23);
        expr_ret_20 = expr_ret_23;
      }

      // SlashExpr end
      expr_ret_19 = expr_ret_20;

    }

    expr_ret_18 = SUCC;
    expr_ret_16 = expr_ret_18;
  }

  // ModExprList end
  if (!expr_ret_16) rew(mod_16);
  expr_ret_15 = expr_ret_16;
  if (!rule) rule = expr_ret_15;
  if (!expr_ret_15) rule = NULL;
  if (rule==SUCC) intr_succ(ctx, "multexpr");
  else if (rule) intr_accept(ctx, "multexpr");
  else intr_reject(ctx, "multexpr");
  return rule;
  #undef rule
}

static inline calc_astnode_t* calc_parse_baseexpr(calc_parser_ctx* ctx) {
  calc_astnode_t* rule = NULL;
  #define rule expr_ret_25
  calc_astnode_t* expr_ret_25 = NULL;
  calc_astnode_t* expr_ret_26 = NULL;
  intr_enter(ctx, "baseexpr", ctx->pos);
  calc_astnode_t* expr_ret_27 = NULL;

  // SlashExpr 0
  if (!expr_ret_27) {
    calc_astnode_t* expr_ret_28 = NULL;
    rec(mod_28);
    // ModExprList 0
    intr_enter(ctx, "OPEN", ctx->pos);
    if (ctx->pos < ctx->len && ctx->tokens[ctx->pos].kind == CALC_TOK_OPEN) {
      // Not capturing OPEN.
      expr_ret_28 = SUCC;
      ctx->pos++;
    } else {
      expr_ret_28 = NULL;
    }

    if (expr_ret_28) intr_accept(ctx, "OPEN"); else intr_reject(ctx, "OPEN");
    // ModExprList 1
    if (expr_ret_28) {
      calc_astnode_t* expr_ret_29 = NULL;
      expr_ret_29 = calc_parse_expr(ctx);
      if (ctx->exit) return NULL;
      expr_ret_28 = expr_ret_29;
      rule = expr_ret_29;
    }

    // ModExprList 2
    if (expr_ret_28) {
      intr_enter(ctx, "CLOSE", ctx->pos);
      if (ctx->pos < ctx->len && ctx->tokens[ctx->pos].kind == CALC_TOK_CLOSE) {
        // Capturing CLOSE.
        expr_ret_28 = leaf(CLOSE);
        expr_ret_28->tok_repr = ctx->tokens[ctx->pos].content;
        expr_ret_28->repr_len = ctx->tokens[ctx->pos].len;
        ctx->pos++;
      } else {
        expr_ret_28 = NULL;
      }

      if (expr_ret_28) intr_accept(ctx, "CLOSE"); else intr_reject(ctx, "CLOSE");
    }

    // ModExprList end
    if (!expr_ret_28) rew(mod_28);
    expr_ret_27 = expr_ret_28;
  }

  // SlashExpr 1
  if (!expr_ret_27) {
    calc_astnode_t* expr_ret_30 = NULL;
    rec(mod_30);
    // ModExprList Forwarding
    intr_enter(ctx, "NUMBER", ctx->pos);
    if (ctx->pos < ctx->len && ctx->tokens[ctx->pos].kind == CALC_TOK_NUMBER) {
      // Capturing NUMBER.
      expr_ret_30 = leaf(NUMBER);
      expr_ret_30->tok_repr = ctx->tokens[ctx->pos].content;
      expr_ret_30->repr_len = ctx->tokens[ctx->pos].len;
      ctx->pos++;
    } else {
      expr_ret_30 = NULL;
    }

    if (expr_ret_30) intr_accept(ctx, "NUMBER"); else intr_reject(ctx, "NUMBER");
    // ModExprList end
    if (!expr_ret_30) rew(mod_30);
    expr_ret_27 = expr_ret_30;
  }

  // SlashExpr end
  expr_ret_26 = expr_ret_27;

  if (!rule) rule = expr_ret_26;
  if (!expr_ret_26) rule = NULL;
  if (rule==SUCC) intr_succ(ctx, "baseexpr");
  else if (rule) intr_accept(ctx, "baseexpr");
  else intr_reject(ctx, "baseexpr");
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
#endif /* PGEN_CALC_ASTNODE_INCLUDE */

#endif /* PGEN_CALC_PARSER_H */
