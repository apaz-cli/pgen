#ifndef PGEN_BF_PARSER_H
#define PGEN_BF_PARSER_H


/* START OF UTF8 LIBRARY */

#ifndef UTF8_INCLUDED
#define UTF8_INCLUDED
#include <inttypes.h>
#include <stdint.h>
#include <stdlib.h>

#define UTF8_END -1 /* 1111 1111 */
#define UTF8_ERR -2 /* 1111 1110 */

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
    if (c1 >= 0) {
      c = ((c0 & 0x1F) << 6) | c1;
      if (c >= 128)
        return c;
    }
  } else if ((c0 & 0xF0) == 0xE0) {
    c1 = UTF8_contByte(state);
    c2 = UTF8_contByte(state);
    if ((c1 | c2) >= 0) {
      c = ((c0 & 0x0F) << 12) | (c1 << 6) | c2;
      if ((c >= 2048) & ((c < 55296) | (c > 57343)))
        return c;
    }
  } else if ((c0 & 0xF8) == 0xF0) {
    c1 = UTF8_contByte(state);
    c2 = UTF8_contByte(state);
    c3 = UTF8_contByte(state);
    if ((c1 | c2 | c3) >= 0) {
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


struct bf_astnode_t;
typedef struct bf_astnode_t bf_astnode_t;


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

#ifndef BF_TOKENIZER_INCLUDE
#define BF_TOKENIZER_INCLUDE

typedef enum {
  BF_TOK_STREAMBEGIN,
  BF_TOK_STREAMEND,
  BF_TOK_PLUS,
  BF_TOK_MINUS,
  BF_TOK_RS,
  BF_TOK_LS,
  BF_TOK_LBRACK,
  BF_TOK_RBRACK,
  BF_TOK_PUTC,
  BF_TOK_GETC,
  BF_TOK_COMMENT,
} bf_token_kind;

// The 0th token is beginning of stream.
// The 1st token isend of stream.
// Tokens 1 through 9 are the ones you defined.
// This totals 11 kinds of tokens.
#define BF_NUM_TOKENKINDS 11
static const char* bf_tokenkind_name[BF_NUM_TOKENKINDS] = {
  "STREAMBEGIN",
  "STREAMEND",
  "PLUS",
  "MINUS",
  "RS",
  "LS",
  "LBRACK",
  "RBRACK",
  "PUTC",
  "GETC",
  "COMMENT",
};

typedef struct {
  bf_token_kind kind;
  codepoint_t* content; // The token begins at tokenizer->start[token->start].
  size_t len;
  size_t line;
  size_t col;
} bf_token;

typedef struct {
  codepoint_t* start;
  size_t len;
  size_t pos;
  size_t pos_line;
  size_t pos_col;
} bf_tokenizer;

static inline void bf_tokenizer_init(bf_tokenizer* tokenizer, codepoint_t* start, size_t len) {
  tokenizer->start = start;
  tokenizer->len = len;
  tokenizer->pos = 0;
  tokenizer->pos_line = 1;
  tokenizer->pos_col = 0;
}

static inline bf_token bf_nextToken(bf_tokenizer* tokenizer) {
  codepoint_t* current = tokenizer->start + tokenizer->pos;
  size_t remaining = tokenizer->len - tokenizer->pos;

  int trie_state = 0;
  int smaut_state_0 = 0;
  size_t trie_munch_size = 0;
  size_t smaut_munch_size_0 = 0;
  bf_token_kind trie_tokenkind = BF_TOK_STREAMEND;

  for (size_t iidx = 0; iidx < remaining; iidx++) {
    codepoint_t c = current[iidx];
    int all_dead = 1;

    // Trie
    if (trie_state != -1) {
      all_dead = 0;
      if (trie_state == 0) {
        if (c == 43 /*'+'*/) trie_state = 1;
        else if (c == 44 /*','*/) trie_state = 8;
        else if (c == 45 /*'-'*/) trie_state = 2;
        else if (c == 46 /*'.'*/) trie_state = 7;
        else if (c == 60 /*'<'*/) trie_state = 4;
        else if (c == 62 /*'>'*/) trie_state = 3;
        else if (c == 91 /*'['*/) trie_state = 5;
        else if (c == 93 /*']'*/) trie_state = 6;
        else trie_state = -1;
      }
      else {
        trie_state = -1;
      }

      // Check accept
      if (trie_state == 1) {
        trie_tokenkind =  BF_TOK_PLUS;
        trie_munch_size = iidx + 1;
      }
      else if (trie_state == 2) {
        trie_tokenkind =  BF_TOK_MINUS;
        trie_munch_size = iidx + 1;
      }
      else if (trie_state == 3) {
        trie_tokenkind =  BF_TOK_RS;
        trie_munch_size = iidx + 1;
      }
      else if (trie_state == 4) {
        trie_tokenkind =  BF_TOK_LS;
        trie_munch_size = iidx + 1;
      }
      else if (trie_state == 5) {
        trie_tokenkind =  BF_TOK_LBRACK;
        trie_munch_size = iidx + 1;
      }
      else if (trie_state == 6) {
        trie_tokenkind =  BF_TOK_RBRACK;
        trie_munch_size = iidx + 1;
      }
      else if (trie_state == 7) {
        trie_tokenkind =  BF_TOK_PUTC;
        trie_munch_size = iidx + 1;
      }
      else if (trie_state == 8) {
        trie_tokenkind =  BF_TOK_GETC;
        trie_munch_size = iidx + 1;
      }
    }

    // Transition COMMENT State Machine
    if (smaut_state_0 != -1) {
      all_dead = 0;

      if ((smaut_state_0 == 0) &
         (!(((c >= '+') & (c <= '[')) | (c == ']') | (c == '>') | (c == '<') | (c == '.') | (c == ',')))) {
          smaut_state_0 = 1;
      }
      else {
        smaut_state_0 = -1;
      }

      // Check accept
      if (smaut_state_0 == 1) {
        smaut_munch_size_0 = iidx + 1;
      }
    }

    if (all_dead)
      break;
  }

  // Determine what token was accepted, if any.
  bf_token_kind kind = BF_TOK_STREAMEND;
  size_t max_munch = 0;
  if (smaut_munch_size_0 >= max_munch) {
    kind = BF_TOK_COMMENT;
    max_munch = smaut_munch_size_0;
  }
  if (trie_munch_size >= max_munch) {
    kind = trie_tokenkind;
    max_munch = trie_munch_size;
  }

  bf_token tok;
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

#endif /* BF_TOKENIZER_INCLUDE */

#ifndef PGEN_BF_ASTNODE_INCLUDE
#define PGEN_BF_ASTNODE_INCLUDE

struct bf_parse_err;
typedef struct bf_parse_err bf_parse_err;
struct bf_parse_err {
  const char* msg;
  int severity;
  size_t line;
  size_t col;
};

#ifndef BF_MAX_PARSER_ERRORS
#define BF_MAX_PARSER_ERRORS 20
#endif
typedef struct {
  bf_token* tokens;
  size_t len;
  size_t pos;
  int exit;
  pgen_allocator *alloc;
  size_t num_errors;
  bf_parse_err errlist[BF_MAX_PARSER_ERRORS];
} bf_parser_ctx;

static inline void bf_parser_ctx_init(bf_parser_ctx* parser,
                                       pgen_allocator* allocator,
                                       bf_token* tokens, size_t num_tokens) {
  parser->tokens = tokens;
  parser->len = num_tokens;
  parser->pos = 0;
  parser->exit = 0;
  parser->alloc = allocator;
  parser->num_errors = 0;
  size_t to_zero = sizeof(bf_parse_err) * BF_MAX_PARSER_ERRORS;
  memset(&parser->errlist, 0, to_zero);
}
static inline void freemsg(const char* msg, void* extra) {
  (void)extra;
  PGEN_FREE((void*)msg);
}

static inline bf_parse_err* bf_report_parse_error(bf_parser_ctx* ctx,
              const char* msg, void (*msgfree)(const char* msg, void* extra), int severity) {
  if (ctx->num_errors >= BF_MAX_PARSER_ERRORS) {
    ctx->exit = 1;
    return NULL;
  }
  bf_parse_err* err = &ctx->errlist[ctx->num_errors++];
  err->msg = (const char*)msg;
  err->severity = severity;
  size_t toknum = ctx->pos + (ctx->pos != ctx->len - 1);
  bf_token tok = ctx->tokens[toknum];
  err->line = tok.line;
  err->col = tok.col;

  if (severity == 3)
    ctx->exit = 1;
  return err;
}

typedef enum {
  BF_NODE_PLUS,
  BF_NODE_MINUS,
  BF_NODE_RS,
  BF_NODE_LS,
  BF_NODE_LBRACK,
  BF_NODE_RBRACK,
  BF_NODE_PUTC,
  BF_NODE_GETC,
  BF_NODE_COMMENT,
} bf_astnode_kind;

#define BF_NUM_NODEKINDS 9
static const char* bf_nodekind_name[BF_NUM_NODEKINDS] = {
  "PLUS",
  "MINUS",
  "RS",
  "LS",
  "LBRACK",
  "RBRACK",
  "PUTC",
  "GETC",
  "COMMENT",
};

struct bf_astnode_t {
  bf_astnode_t* parent;
  uint16_t num_children;
  uint16_t max_children;
  bf_astnode_kind kind;

  codepoint_t* tok_repr;
  size_t repr_len;
  // No %extra directives.
  bf_astnode_t** children;
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


static inline bf_astnode_t* bf_astnode_list(
                             pgen_allocator* alloc,
                             bf_astnode_kind kind,
                             size_t initial_size) {
  char* ret = pgen_alloc(alloc,
                         sizeof(bf_astnode_t),
                         _Alignof(bf_astnode_t));
  bf_astnode_t *node = (bf_astnode_t*)ret;

  bf_astnode_t **children;
  if (initial_size) {
    children = (bf_astnode_t**)PGEN_MALLOC(sizeof(bf_astnode_t*) * initial_size);
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

static inline bf_astnode_t* bf_astnode_leaf(
                             pgen_allocator* alloc,
                             bf_astnode_kind kind) {
  char* ret = pgen_alloc(alloc,
                         sizeof(bf_astnode_t),
                         _Alignof(bf_astnode_t));
  bf_astnode_t *node = (bf_astnode_t *)ret;
  bf_astnode_t *children = NULL;
  node->kind = kind;
  node->parent = NULL;
  node->max_children = 0;
  node->num_children = 0;
  node->children = NULL;
  node->tok_repr = NULL;
  node->repr_len = 0;
  return node;
}

static inline bf_astnode_t* bf_astnode_fixed_1(
                             pgen_allocator* alloc,
                             bf_astnode_kind kind,
                             bf_astnode_t* PGEN_RESTRICT n0) {
  char* ret = pgen_alloc(alloc,
                         sizeof(bf_astnode_t) +
                         sizeof(bf_astnode_t *) * 1,
                         _Alignof(bf_astnode_t));
  bf_astnode_t *node = (bf_astnode_t *)ret;
  bf_astnode_t **children = (bf_astnode_t **)(node + 1);
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

static inline bf_astnode_t* bf_astnode_fixed_2(
                             pgen_allocator* alloc,
                             bf_astnode_kind kind,
                             bf_astnode_t* PGEN_RESTRICT n0,
                             bf_astnode_t* PGEN_RESTRICT n1) {
  char* ret = pgen_alloc(alloc,
                         sizeof(bf_astnode_t) +
                         sizeof(bf_astnode_t *) * 2,
                         _Alignof(bf_astnode_t));
  bf_astnode_t *node = (bf_astnode_t *)ret;
  bf_astnode_t **children = (bf_astnode_t **)(node + 1);
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

static inline bf_astnode_t* bf_astnode_fixed_3(
                             pgen_allocator* alloc,
                             bf_astnode_kind kind,
                             bf_astnode_t* PGEN_RESTRICT n0,
                             bf_astnode_t* PGEN_RESTRICT n1,
                             bf_astnode_t* PGEN_RESTRICT n2) {
  char* ret = pgen_alloc(alloc,
                         sizeof(bf_astnode_t) +
                         sizeof(bf_astnode_t *) * 3,
                         _Alignof(bf_astnode_t));
  bf_astnode_t *node = (bf_astnode_t *)ret;
  bf_astnode_t **children = (bf_astnode_t **)(node + 1);
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

static inline bf_astnode_t* bf_astnode_fixed_4(
                             pgen_allocator* alloc,
                             bf_astnode_kind kind,
                             bf_astnode_t* PGEN_RESTRICT n0,
                             bf_astnode_t* PGEN_RESTRICT n1,
                             bf_astnode_t* PGEN_RESTRICT n2,
                             bf_astnode_t* PGEN_RESTRICT n3) {
  char* ret = pgen_alloc(alloc,
                         sizeof(bf_astnode_t) +
                         sizeof(bf_astnode_t *) * 4,
                         _Alignof(bf_astnode_t));
  bf_astnode_t *node = (bf_astnode_t *)ret;
  bf_astnode_t **children = (bf_astnode_t **)(node + 1);
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

static inline bf_astnode_t* bf_astnode_fixed_5(
                             pgen_allocator* alloc,
                             bf_astnode_kind kind,
                             bf_astnode_t* PGEN_RESTRICT n0,
                             bf_astnode_t* PGEN_RESTRICT n1,
                             bf_astnode_t* PGEN_RESTRICT n2,
                             bf_astnode_t* PGEN_RESTRICT n3,
                             bf_astnode_t* PGEN_RESTRICT n4) {
  char* ret = pgen_alloc(alloc,
                         sizeof(bf_astnode_t) +
                         sizeof(bf_astnode_t *) * 5,
                         _Alignof(bf_astnode_t));
  bf_astnode_t *node = (bf_astnode_t *)ret;
  bf_astnode_t **children = (bf_astnode_t **)(node + 1);
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

static inline void bf_astnode_add(pgen_allocator* alloc, bf_astnode_t *list, bf_astnode_t *node) {
  if (list->max_children == list->num_children) {
    // Figure out the new size. Check for overflow where applicable.
    uint64_t new_max = (uint64_t)list->max_children * 2;
    if (new_max > UINT16_MAX || new_max > SIZE_MAX) PGEN_OOM();
    if (SIZE_MAX < UINT16_MAX && (size_t)new_max > SIZE_MAX / sizeof(bf_astnode_t)) PGEN_OOM();
    size_t new_bytes = (size_t)new_max * sizeof(bf_astnode_t);

    // Reallocate the list, and inform the allocator.
    void* old_ptr = list->children;
    void* new_ptr = realloc(list->children, new_bytes);
    if (!new_ptr) PGEN_OOM();
    list->children = (bf_astnode_t **)new_ptr;
    list->max_children = (uint16_t)new_max;
    pgen_allocator_realloced(alloc, old_ptr, new_ptr, free);
  }
  node->parent = list;
  list->children[list->num_children++] = node;
}

static inline void bf_parser_rewind(bf_parser_ctx *ctx, pgen_parser_rewind_t rew) {
  pgen_allocator_rewind(ctx->alloc, rew.arew);
  ctx->pos = rew.prew;
}

static inline bf_astnode_t* bf_astnode_repr(bf_astnode_t* node, bf_astnode_t* t) {
  node->tok_repr = t->tok_repr;
  node->repr_len = t->repr_len;
  return node;
}

static inline bf_astnode_t* bf_astnode_cprepr(bf_astnode_t* node, codepoint_t* cps, size_t repr_len) {
  node->tok_repr = cps;
  node->repr_len = repr_len;
  return node;
}

static inline bf_astnode_t* bf_astnode_srepr(pgen_allocator* allocator, bf_astnode_t* node, char* s) {
  size_t cpslen = strlen(s);
  codepoint_t* cps = (codepoint_t*)pgen_alloc(allocator, (cpslen + 1) * sizeof(codepoint_t), _Alignof(codepoint_t));
  for (size_t i = 0; i < cpslen; i++) cps[i] = (codepoint_t)s[i];
  cps[cpslen] = 0;
  node->tok_repr = cps;
  node->repr_len = cpslen;
  return node;
}

static inline int bf_node_print_content(bf_astnode_t* node, bf_token* tokens) {
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
        else fputc(utf8[i], stdout);
      return PGEN_FREE(utf8), 1;
    }
  }
  return 0;
}

static inline int bf_astnode_print_h(bf_token* tokens, bf_astnode_t *node, size_t depth, int fl) {
  #define indent() for (size_t i = 0; i < depth; i++) printf("  ")
  if (!node)
    return 0;
  else if (node == (bf_astnode_t*)(void*)(uintptr_t)_Alignof(bf_astnode_t))
    puts("ERROR, CAPTURED SUCC."), exit(1);

  indent(); puts("{");
  depth++;
  indent(); printf("\"kind\": "); printf("\"%s\",\n", bf_nodekind_name[node->kind]);
  if (!(!node->tok_repr & !node->repr_len)) {
    indent();
    printf("\"content\": \"");
    bf_node_print_content(node, tokens);
    printf("\",\n");
  }
  size_t cnum = node->num_children;
  if (cnum) {
    indent(); printf("\"num_children\": %zu,\n", cnum);
    indent(); printf("\"children\": [");
    putchar('\n');
    for (size_t i = 0; i < cnum; i++)
      bf_astnode_print_h(tokens, node->children[i], depth + 1, i == cnum - 1);
    indent();
    printf("]\n");
  }
  depth--;
  indent(); putchar('}'); if (fl != 1) putchar(','); putchar('\n');
  return 0;
#undef indent
}

static inline void bf_astnode_print_json(bf_token* tokens, bf_astnode_t *node) {
  if (node)    bf_astnode_print_h(tokens, node, 0, 1);
  else    puts("The AST is null.");}

#define SUCC                     (bf_astnode_t*)(void*)(uintptr_t)_Alignof(bf_astnode_t)

#define rec(label)               pgen_parser_rewind_t _rew_##label = (pgen_parser_rewind_t){ctx->alloc->rew, ctx->pos};
#define rew(label)               bf_parser_rewind(ctx, _rew_##label)
#define node(kindname, ...)      PGEN_CAT(bf_astnode_fixed_, PGEN_NARG(__VA_ARGS__))(ctx->alloc, kind(kindname), __VA_ARGS__)
#define kind(name)               BF_NODE_##name
#define list(kind)               bf_astnode_list(ctx->alloc, BF_NODE_##kind, 16)
#define leaf(kind)               bf_astnode_leaf(ctx->alloc, BF_NODE_##kind)
#define add(list, node)          bf_astnode_add(ctx->alloc, list, node)
#define has(node)                (((uintptr_t)node <= (uintptr_t)SUCC) ? 0 : 1)
#define repr(node, t)            bf_astnode_repr(node, t)
#define srepr(node, s)           bf_astnode_srepr(ctx->alloc, node, (char*)s)
#define cprepr(node, cps, len)   bf_astnode_cprepr(node, cps, len)
#define expect(kind, cap)        ((ctx->pos < ctx->len && ctx->tokens[ctx->pos].kind == bf_TOK_##kind) ? ctx->pos++, (cap ? cprepr(leaf(kind), NULL, ctx->pos-1) : SUCC) : NULL)

#define LB {
#define RB }

#define INFO(msg)                bf_report_parse_error(ctx, (const char*)msg, NULL,   0)
#define WARNING(msg)             bf_report_parse_error(ctx, (const char*)msg, NULL,   1)
#define ERROR(msg)               bf_report_parse_error(ctx, (const char*)msg, NULL,   2)
#define FATAL(msg)               bf_report_parse_error(ctx, (const char*)msg, NULL,   3)
#define INFO_F(msg, freefn)      bf_report_parse_error(ctx, (const char*)msg, freefn, 0)
#define WARNING_F(msg, freefn)   bf_report_parse_error(ctx, (const char*)msg, freefn, 1)
#define ERROR_F(msg, freefn)     bf_report_parse_error(ctx, (const char*)msg, freefn, 2)
#define FATAL_F(msg, freefn)     bf_report_parse_error(ctx, (const char*)msg, freefn, 3)

/******************/
/* Mid Directives */
/******************/
#include <string.h>
#define ARRSIZE 30000
static char array[ARRSIZE];
static char *ptr;

static inline bf_astnode_t* bf_parse_runprogam(bf_parser_ctx* ctx);
static inline bf_astnode_t* bf_parse_char(bf_parser_ctx* ctx);


static inline bf_astnode_t* bf_parse_runprogam(bf_parser_ctx* ctx) {
  #define rule expr_ret_0
  bf_astnode_t* expr_ret_0 = NULL;
  bf_astnode_t* expr_ret_1 = NULL;
  bf_astnode_t* expr_ret_2 = NULL;
  rec(mod_2);
  // ModExprList 0
  // CodeExpr
  #define ret expr_ret_2
  ret = SUCC;
  memset(array, 0, ARRSIZE), ptr = array;
  #undef ret
  // ModExprList 1
  if (expr_ret_2) {
    bf_astnode_t* expr_ret_3 = NULL;
    bf_astnode_t* expr_ret_4 = SUCC;
    while (expr_ret_4)
    {
      rec(kleene_rew_3);
      expr_ret_4 = bf_parse_char(ctx);
      if (ctx->exit) return NULL;
    }

    expr_ret_3 = SUCC;
    expr_ret_2 = expr_ret_3;
  }

  // ModExprList end
  if (!expr_ret_2) rew(mod_2);
  expr_ret_1 = expr_ret_2;
  if (!rule) rule = expr_ret_1;
  if (!expr_ret_1) rule = NULL;
  return rule;
  #undef rule
}

static inline bf_astnode_t* bf_parse_char(bf_parser_ctx* ctx) {
  #define rule expr_ret_5
  bf_astnode_t* expr_ret_5 = NULL;
  bf_astnode_t* expr_ret_6 = NULL;
  bf_astnode_t* expr_ret_7 = NULL;

  // SlashExpr 0
  if (!expr_ret_7) {
    bf_astnode_t* expr_ret_8 = NULL;
    rec(mod_8);
    // ModExprList 0
    if (ctx->pos < ctx->len && ctx->tokens[ctx->pos].kind == BF_TOK_PLUS) {
      // Not capturing PLUS.
      expr_ret_8 = SUCC;
      ctx->pos++;
    } else {
      expr_ret_8 = NULL;
    }

    // ModExprList 1
    if (expr_ret_8) {
      // CodeExpr
      #define ret expr_ret_8
      ret = SUCC;
      ++*ptr;
      #undef ret
    }

    // ModExprList end
    if (!expr_ret_8) rew(mod_8);
    expr_ret_7 = expr_ret_8;
  }

  // SlashExpr 1
  if (!expr_ret_7) {
    bf_astnode_t* expr_ret_9 = NULL;
    rec(mod_9);
    // ModExprList 0
    if (ctx->pos < ctx->len && ctx->tokens[ctx->pos].kind == BF_TOK_MINUS) {
      // Not capturing MINUS.
      expr_ret_9 = SUCC;
      ctx->pos++;
    } else {
      expr_ret_9 = NULL;
    }

    // ModExprList 1
    if (expr_ret_9) {
      // CodeExpr
      #define ret expr_ret_9
      ret = SUCC;
      --*ptr;
      #undef ret
    }

    // ModExprList end
    if (!expr_ret_9) rew(mod_9);
    expr_ret_7 = expr_ret_9;
  }

  // SlashExpr 2
  if (!expr_ret_7) {
    bf_astnode_t* expr_ret_10 = NULL;
    rec(mod_10);
    // ModExprList 0
    if (ctx->pos < ctx->len && ctx->tokens[ctx->pos].kind == BF_TOK_RS) {
      // Not capturing RS.
      expr_ret_10 = SUCC;
      ctx->pos++;
    } else {
      expr_ret_10 = NULL;
    }

    // ModExprList 1
    if (expr_ret_10) {
      // CodeExpr
      #define ret expr_ret_10
      ret = SUCC;
      ++ptr;
      #undef ret
    }

    // ModExprList end
    if (!expr_ret_10) rew(mod_10);
    expr_ret_7 = expr_ret_10;
  }

  // SlashExpr 3
  if (!expr_ret_7) {
    bf_astnode_t* expr_ret_11 = NULL;
    rec(mod_11);
    // ModExprList 0
    if (ctx->pos < ctx->len && ctx->tokens[ctx->pos].kind == BF_TOK_LS) {
      // Not capturing LS.
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
      --ptr;
      #undef ret
    }

    // ModExprList end
    if (!expr_ret_11) rew(mod_11);
    expr_ret_7 = expr_ret_11;
  }

  // SlashExpr 4
  if (!expr_ret_7) {
    bf_astnode_t* expr_ret_12 = NULL;
    rec(mod_12);
    // ModExprList 0
    if (ctx->pos < ctx->len && ctx->tokens[ctx->pos].kind == BF_TOK_LBRACK) {
      // Not capturing LBRACK.
      expr_ret_12 = SUCC;
      ctx->pos++;
    } else {
      expr_ret_12 = NULL;
    }

    // ModExprList 1
    if (expr_ret_12) {
      // CodeExpr
      #define ret expr_ret_12
      ret = SUCC;
      
        if (*ptr == 0) {
          int m = 1;
          while(m) {
            ctx->pos++;
            if (ctx->tokens[ctx->pos].kind == BF_TOK_LBRACK) m++;
            if (ctx->tokens[ctx->pos].kind == BF_TOK_RBRACK) m--;
          }
          ctx->pos++;
        }
      ;
      #undef ret
    }

    // ModExprList end
    if (!expr_ret_12) rew(mod_12);
    expr_ret_7 = expr_ret_12;
  }

  // SlashExpr 5
  if (!expr_ret_7) {
    bf_astnode_t* expr_ret_13 = NULL;
    rec(mod_13);
    // ModExprList 0
    if (ctx->pos < ctx->len && ctx->tokens[ctx->pos].kind == BF_TOK_RBRACK) {
      // Not capturing RBRACK.
      expr_ret_13 = SUCC;
      ctx->pos++;
    } else {
      expr_ret_13 = NULL;
    }

    // ModExprList 1
    if (expr_ret_13) {
      // CodeExpr
      #define ret expr_ret_13
      ret = SUCC;
      
        if (*ptr) {
          int m = 1;
          while (m) {
            ctx->pos--;
            if (ctx->tokens[ctx->pos].kind == BF_TOK_LBRACK) m--;
            if (ctx->tokens[ctx->pos].kind == BF_TOK_RBRACK) m++;
          }
          ctx->pos++;
        }
      ;
      #undef ret
    }

    // ModExprList end
    if (!expr_ret_13) rew(mod_13);
    expr_ret_7 = expr_ret_13;
  }

  // SlashExpr 6
  if (!expr_ret_7) {
    bf_astnode_t* expr_ret_14 = NULL;
    rec(mod_14);
    // ModExprList 0
    if (ctx->pos < ctx->len && ctx->tokens[ctx->pos].kind == BF_TOK_PUTC) {
      // Not capturing PUTC.
      expr_ret_14 = SUCC;
      ctx->pos++;
    } else {
      expr_ret_14 = NULL;
    }

    // ModExprList 1
    if (expr_ret_14) {
      // CodeExpr
      #define ret expr_ret_14
      ret = SUCC;
      putchar(*ptr);
      #undef ret
    }

    // ModExprList end
    if (!expr_ret_14) rew(mod_14);
    expr_ret_7 = expr_ret_14;
  }

  // SlashExpr 7
  if (!expr_ret_7) {
    bf_astnode_t* expr_ret_15 = NULL;
    rec(mod_15);
    // ModExprList 0
    if (ctx->pos < ctx->len && ctx->tokens[ctx->pos].kind == BF_TOK_GETC) {
      // Not capturing GETC.
      expr_ret_15 = SUCC;
      ctx->pos++;
    } else {
      expr_ret_15 = NULL;
    }

    // ModExprList 1
    if (expr_ret_15) {
      // CodeExpr
      #define ret expr_ret_15
      ret = SUCC;
      *ptr=getchar();
      #undef ret
    }

    // ModExprList end
    if (!expr_ret_15) rew(mod_15);
    expr_ret_7 = expr_ret_15;
  }

  // SlashExpr 8
  if (!expr_ret_7) {
    bf_astnode_t* expr_ret_16 = NULL;
    rec(mod_16);
    // ModExprList 0
    if (ctx->pos < ctx->len && ctx->tokens[ctx->pos].kind == BF_TOK_COMMENT) {
      // Not capturing COMMENT.
      expr_ret_16 = SUCC;
      ctx->pos++;
    } else {
      expr_ret_16 = NULL;
    }

    // ModExprList 1
    if (expr_ret_16) {
      // CodeExpr
      #define ret expr_ret_16
      ret = SUCC;
      ;
      #undef ret
    }

    // ModExprList end
    if (!expr_ret_16) rew(mod_16);
    expr_ret_7 = expr_ret_16;
  }

  // SlashExpr end
  expr_ret_6 = expr_ret_7;

  if (!rule) rule = expr_ret_6;
  if (!expr_ret_6) rule = NULL;
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
#endif /* PGEN_BF_ASTNODE_INCLUDE */

#endif /* PGEN_BF_PARSER_H */
