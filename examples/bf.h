
/* START OF UTF8 LIBRARY */

#ifndef PGEN_UTF8_INCLUDED
#define PGEN_UTF8_INCLUDED
#include <stdint.h>
#include <stdlib.h>
#include <inttypes.h>

#define UTF8_END -1 /* 1111 1111 */
#define UTF8_ERR -2 /* 1111 1110 */

typedef int32_t codepoint_t;
#define PRI_CODEPOINT PRIu32

typedef struct {
  size_t idx;
  size_t len;
  size_t chr;
  size_t byte;
  char *inp;
} UTF8Decoder;

static inline void UTF8_decoder_init(UTF8Decoder *state, char *str,
                                     size_t len) {
  state->idx = 0;
  state->len = len;
  state->chr = 0;
  state->byte = 0;
  state->inp = str;
}

static inline char UTF8_nextByte(UTF8Decoder *state) {
  char c;
  if (state->idx >= state->len)
    return UTF8_END;
  c = (state->inp[state->idx] & 0xFF);
  state->idx += 1;
  return c;
}

static inline char UTF8_contByte(UTF8Decoder *state) {
  char c;
  c = UTF8_nextByte(state);
  return ((c & 0xC0) == 0x80) ? (c & 0x3F) : UTF8_ERR;
}

/* Extract the next unicode code point. Returns c, UTF8_END, or UTF8_ERR. */
static inline codepoint_t UTF8_decodeNext(UTF8Decoder *state) {
  codepoint_t c;
  char c0, c1, c2, c3;

  if (state->idx >= state->len)
    return state->idx == state->len ? UTF8_END : UTF8_ERR;

  state->byte = state->idx;
  state->chr += 1;
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
 * This will malloc() a buffer large enough, and store it to retstr and its
 * length to retcps. The result is not null terminated.
 * Returns 1 on success, 0 on failure. Cleans up the buffer and does not store
 * to retstr or retlen on failure.
 */
static inline int UTF8_encode(codepoint_t *codepoints, size_t len, char **retstr,
                       size_t *retlen) {
  char buf4[4];
  size_t characters_used = 0, used, i, j;
  char *out_buf, *new_obuf;

  if ((!codepoints) | (!len))
    return 0;
  if (!(out_buf = (char *)malloc(len * sizeof(codepoint_t) + 1)))
    return 0;

  for (i = 0; i < len; i++) {
    if (!(used = UTF8_encodeNext(codepoints[i], buf4)))
      return 0;
    for (j = 0; j < used; j++)
      out_buf[characters_used++] = buf4[j];
  }

  out_buf[characters_used] = '\0';
  new_obuf = (char *)realloc(out_buf, characters_used + 1);
  *retstr = new_obuf ? new_obuf : out_buf;
  *retlen = characters_used;
  return 1;
}

/*
 * Convert a UTF8 string to UTF32 codepoints.
 * This will malloc() a buffer large enough, and store it to retstr and its
 * length to retcps. The result is not null terminated.
 * Returns 1 on success, 0 on failure. Cleans up the buffer and does not store
 * to retcps or retlen on failure.
 */
static inline int UTF8_decode(char *str, size_t len, codepoint_t **retcps,
                              size_t *retlen) {
  UTF8Decoder state;
  codepoint_t *cpbuf, cp;
  size_t cps_read = 0;

  if ((!str) | (!len))
    return 0;
  if (!(cpbuf = (codepoint_t *)malloc(sizeof(codepoint_t) * len)))
    return 0;

  UTF8_decoder_init(&state, str, len);
  for (;;) {
    cp = UTF8_decodeNext(&state);
    if ((cp == UTF8_ERR) | (cp == UTF8_END))
      break;
    cpbuf[cps_read++] = cp;
  }

  if (cp == UTF8_ERR)
    return free(cpbuf), 0;

  *retcps = cpbuf;
  *retlen = cps_read;
  return 1;
}

#endif /* PGEN_UTF8 */

/* END OF UTF8 LIBRARY */


#ifndef PGEN_DEBUG
#define PGEN_DEBUG 0

#define PGEN_ALLOCATOR_DEBUG 0

#endif /* PGEN_DEBUG */

/**************/
/* Directives */
/**************/
#include <string.h>
#define ARRSIZE 30000
static char array[ARRSIZE];
static char *ptr;


/* START OF AST ALLOCATOR */

#ifndef PGEN_ARENA_INCLUDED
#define PGEN_ARENA_INCLUDED
#include <limits.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#define PGEN_ALIGNMENT _Alignof(max_align_t)
#define PGEN_BUFFER_SIZE (PGEN_PAGESIZE * 1024)
#define NUM_ARENAS 256
#define NUM_FREELIST 256

#ifndef PGEN_PAGESIZE
#define PGEN_PAGESIZE 4096
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
#define PGEN_AlLOCATOR_DEBUG 0
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
  return (n + align - (n % align));
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
  pgen_arena_t arenas[NUM_ARENAS];
  pgen_freelist_t freelist;
} pgen_allocator;

static inline pgen_allocator pgen_allocator_new(void) {
  pgen_allocator alloc;

  alloc.rew.arena_idx = 0;
  alloc.rew.filled = 0;

  for (size_t i = 0; i < NUM_ARENAS; i++) {
    alloc.arenas[i].freefn = NULL;
    alloc.arenas[i].buf = NULL;
    alloc.arenas[i].cap = 0;
  }

  alloc.freelist.entries = (pgen_freelist_entry_t *)malloc(
      sizeof(pgen_freelist_entry_t) * NUM_FREELIST);
  if (alloc.freelist.entries) {
    alloc.freelist.cap = NUM_FREELIST;
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
  for (size_t i = 0; i < NUM_ARENAS; i++) {
    if (!allocator->arenas[i].buf) {
      allocator->arenas[i] = arena;
      return 1;
    }
  }
  return 0;
}

static inline void pgen_allocator_destroy(pgen_allocator *allocator) {
  // Free all the buffers
  for (size_t i = 0; i < NUM_ARENAS; i++) {
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

#define PGEN_ALLOC_OF(allocator, type)                                         \
  pgen_alloc(allocator, sizeof(type), _Alignof(type))
static inline char *pgen_alloc(pgen_allocator *allocator, size_t n,
                               size_t alignment) {
#if PGEN_AlLOCATOR_DEBUG
  printf("Allocating, from: (%u, %u)\n", allocator->rew.arena_idx,
         allocator->rew.filled);
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
      if (allocator->rew.arena_idx + 1 >= NUM_ARENAS)
        return ret;

      // Allocate a new arena if necessary
      if (allocator->arenas[allocator->rew.arena_idx].buf)
        allocator->rew.arena_idx++;
      if (!allocator->arenas[allocator->rew.arena_idx].buf) {
        char *nb = (char *)malloc(PGEN_BUFFER_SIZE);
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
  allocator->rew.filled = bufnext;

#if PGEN_AlLOCATOR_DEBUG
  printf("Allocated, to: (%u, %u)", allocator->rew.arena_idx,
         allocator->rew.filled);
#endif

  return ret;
}

// Does not take a pgen_allocator_rewind_t, so does not rebind the
// lifetime of the reallocated object.
static inline void pgen_allocator_realloced(pgen_allocator *allocator,
                                            void *old_ptr, void *new_ptr,
                                            void (*new_free_fn)(void *)) {

#if PGEN_AlLOCATOR_DEBUG
  printf("realloc(%p -> %p): ", old_ptr, new_ptr);
  for (size_t i = 0; i < allocator->freelist.len; i++) {
    printf("(%p, %p, (%u, %u)) ", allocator->freelist.entries->freefn,
           allocator->freelist.entries->ptr,
           allocator->freelist.entries->rew.arena_idx,
           allocator->freelist.entries->rew.filled);
  }
  puts("");
#endif

  for (size_t i = 0; i < allocator->freelist.len; i++) {
    void *ptr = allocator->freelist.entries[i].ptr;
    if (ptr == old_ptr) {
      allocator->freelist.entries[i].ptr = new_ptr;
      allocator->freelist.entries[i].freefn = new_free_fn;
      return;
    }
  }

#if PGEN_AlLOCATOR_DEBUG
  printf("Realloced: ");
  for (size_t i = 0; i < allocator->freelist.len; i++) {
    printf("(%p, %p, (%u, %u)) ", allocator->freelist.entries->freefn,
           allocator->freelist.entries->ptr,
           allocator->freelist.entries->rew.arena_idx,
           allocator->freelist.entries->rew.filled);
  }
#endif
}

static inline void pgen_defer(pgen_allocator *allocator, void (*freefn)(void *),
                              void *ptr, pgen_allocator_rewind_t rew) {
#if PGEN_AlLOCATOR_DEBUG
  printf("defer(%p, (%u, %u)) (%u): ", ptr, rew.arena_idx, rew.filled,
         allocator->freelist.len);
  for (size_t i = 0; i < allocator->freelist.len; i++) {
    printf("(%p, %p, (%u, %u)) ", allocator->freelist.entries->freefn,
           allocator->freelist.entries->ptr,
           allocator->freelist.entries->rew.arena_idx,
           allocator->freelist.entries->rew.filled);
  }
  puts("");
#endif

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
  allocator->freelist.len = next_len;

#if PGEN_AlLOCATOR_DEBUG
  printf("Deferred: (%u) ", allocator->freelist.len);
  for (size_t i = 0; i < allocator->freelist.len; i++) {
    printf("(%p, (%u, %u)) ", allocator->freelist.entries->ptr,
           allocator->freelist.entries->rew.arena_idx,
           allocator->freelist.entries->rew.filled);
  }
#endif
}

static inline void pgen_allocator_rewind(pgen_allocator *allocator,
                                         pgen_allocator_rewind_t rew) {

#if PGEN_AlLOCATOR_DEBUG
  printf("rewind((%u, %u) -> (%u, %u)): (%u) ",
         allocator->freelist.entries->rew.arena_idx,
         allocator->freelist.entries->rew.filled, rew.arena_idx, rew.filled,
         allocator->freelist.len);
  for (size_t i = 0; i < allocator->freelist.len; i++) {
    printf("(%p, %p, (%u, %u)) ", allocator->freelist.entries->freefn,
           allocator->freelist.entries->ptr,
           allocator->freelist.entries->rew.arena_idx,
           allocator->freelist.entries->rew.filled);
  }
  puts("");
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
    allocator->freelist.len = i;
  allocator->rew = rew;

#if PGEN_AlLOCATOR_DEBUG
  printf("rewound(%u, %u): (%u) ", rew.arena_idx, rew.filled,
         allocator->freelist.len);
  for (size_t i = 0; i < allocator->freelist.len; i++) {
    printf("(%p, %p, (%u, %u)) ", allocator->freelist.entries->freefn,
           allocator->freelist.entries->ptr,
           allocator->freelist.entries->rew.arena_idx,
           allocator->freelist.entries->rew.filled);
  }
#endif
}

#endif /* PGEN_ARENA_INCLUDED */

#ifndef PGEN_PARSER_MACROS_INCLUDED
#define PGEN_PARSER_MACROS_INCLUDED
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

#ifndef BF_TOKENIZER_SOURCEINFO
#define BF_TOKENIZER_SOURCEINFO 1
#endif

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
  "BF_TOK_STREAMBEGIN",
  "BF_TOK_STREAMEND",
  "BF_TOK_PLUS",
  "BF_TOK_MINUS",
  "BF_TOK_RS",
  "BF_TOK_LS",
  "BF_TOK_LBRACK",
  "BF_TOK_RBRACK",
  "BF_TOK_PUTC",
  "BF_TOK_GETC",
  "BF_TOK_COMMENT",
};

typedef struct {
  bf_token_kind kind;
  size_t start; // The token begins at tokenizer->start[token->start].
  size_t len;   // It goes until tokenizer->start[token->start + token->len] (non-inclusive).
#if BF_TOKENIZER_SOURCEINFO
  size_t line;
  size_t col;
  char* sourceFile;
#endif
#ifdef BF_TOKEN_EXTRA
  BF_TOKEN_EXTRA
#endif
} bf_token;

typedef struct {
  codepoint_t* start;
  size_t len;
  size_t pos;
#if BF_TOKENIZER_SOURCEINFO
  size_t pos_line;
  size_t pos_col;
  char* pos_sourceFile;
#endif
} bf_tokenizer;

static inline void bf_tokenizer_init(bf_tokenizer* tokenizer, codepoint_t* start, size_t len, char* sourceFile) {
  tokenizer->start = start;
  tokenizer->len = len;
  tokenizer->pos = 0;
#if BF_TOKENIZER_SOURCEINFO
  tokenizer->pos_line = 0;
  tokenizer->pos_col = 0;
  tokenizer->pos_sourceFile = sourceFile;
#else
  (void)sourceFile;
#endif
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
         (!(((c >= 43) & (c <= 91)) | (c == 93) | (c == 62) | (c == 60) | (c == 46) | (c == 44)))) {
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

  bf_token ret;
  ret.kind = kind;
  ret.start = tokenizer->pos;
  ret.len = max_munch;

#if BF_TOKENIZER_SOURCEINFO
  ret.line = tokenizer->pos_line;
  ret.col = tokenizer->pos_col;
  ret.sourceFile = tokenizer->pos_sourceFile;

  for (size_t i = 0; i < ret.len; i++) {
    if (current[i] == '\n') {
      tokenizer->pos_line++;
      tokenizer->pos_col = 0;
    } else {
      tokenizer->pos_col++;
    }
  }
#endif

  tokenizer->pos += max_munch;
  return ret;
}

#endif /* BF_TOKENIZER_INCLUDE */

#ifndef PGEN_BF_ASTNODE_INCLUDE
#define PGEN_BF_ASTNODE_INCLUDE

typedef struct {
  bf_token* tokens;
  size_t len;
  size_t pos;
  pgen_allocator *alloc;
} bf_parser_ctx;

static inline void bf_parser_ctx_init(bf_parser_ctx* parser,
                                       pgen_allocator* allocator,
                                       bf_token* tokens, size_t num_tokens) {
  parser->tokens = tokens;
  parser->len = num_tokens;
  parser->pos = 0;
  parser->alloc = allocator;
}
typedef enum {
  bf_NODE_EMPTY,
} bf_astnode_kind;

#define BF_NUM_NODEKINDS 1
static const char* bf_nodekind_name[BF_NUM_NODEKINDS] = {
  "BF_NODE_EMPTY",
};

struct bf_astnode_t;
typedef struct bf_astnode_t bf_astnode_t;
struct bf_astnode_t {
  // No %extra directives.

  bf_astnode_kind kind;
  bf_astnode_t* parent;
  size_t num_children;
  size_t max_children;
  bf_astnode_t** children;
};

static inline bf_astnode_t* bf_astnode_list(
                             pgen_allocator* alloc,
                             bf_astnode_kind kind,
                             size_t initial_size) {
  char* ret = pgen_alloc(alloc,
                         sizeof(bf_astnode_t),
                         _Alignof(bf_astnode_t));
  if (!ret) PGEN_OOM();
  bf_astnode_t *node = (bf_astnode_t*)ret;

  bf_astnode_t **children;
  if (initial_size) {
    children = (bf_astnode_t**)malloc(sizeof(bf_astnode_t*) * initial_size);
    if (!children)
      PGEN_OOM();
    pgen_defer(alloc, free, children, alloc->rew);
  } else {
    children = NULL;
  }

  node->kind = kind;
  node->parent = NULL;
  node->max_children = initial_size;
  node->num_children = 0;
  node->children = children;
  return node;
}

static inline bf_astnode_t* bf_astnode_leaf(
                             pgen_allocator* alloc,
                             bf_astnode_kind kind) {
  char* ret = pgen_alloc(alloc,
                         sizeof(bf_astnode_t),
                         _Alignof(bf_astnode_t));
  if (!ret) PGEN_OOM();
  bf_astnode_t *node = (bf_astnode_t *)ret;
  bf_astnode_t *children = NULL;
  node->kind = kind;
  node->parent = NULL;
  node->max_children = 0;
  node->num_children = 0;
  node->children = NULL;
  return node;
}

static inline bf_astnode_t* bf_astnode_fixed_1(
                             pgen_allocator* alloc,
                             bf_astnode_kind kind,
                             bf_astnode_t* n0) {
  char* ret = pgen_alloc(alloc,
                         sizeof(bf_astnode_t) +
                         sizeof(bf_astnode_t *) * 1,
                                        _Alignof(bf_astnode_t));
  if (!ret) PGEN_OOM();
  bf_astnode_t *node = (bf_astnode_t *)ret;
  bf_astnode_t **children = (bf_astnode_t **)(node + 1);
  node->kind = kind;
  node->parent = NULL;
  node->max_children = 0;
  node->num_children = 1;
  node->children = children;
  children[0] = n0;
  return node;
}

static inline bf_astnode_t* bf_astnode_fixed_2(
                             pgen_allocator* alloc,
                             bf_astnode_kind kind,
                             bf_astnode_t* n0,
                             bf_astnode_t* n1) {
  char* ret = pgen_alloc(alloc,
                         sizeof(bf_astnode_t) +
                         sizeof(bf_astnode_t *) * 2,
                                        _Alignof(bf_astnode_t));
  if (!ret) PGEN_OOM();
  bf_astnode_t *node = (bf_astnode_t *)ret;
  bf_astnode_t **children = (bf_astnode_t **)(node + 1);
  node->kind = kind;
  node->parent = NULL;
  node->max_children = 0;
  node->num_children = 2;
  node->children = children;
  children[0] = n0;
  children[1] = n1;
  return node;
}

static inline bf_astnode_t* bf_astnode_fixed_3(
                             pgen_allocator* alloc,
                             bf_astnode_kind kind,
                             bf_astnode_t* n0,
                             bf_astnode_t* n1,
                             bf_astnode_t* n2) {
  char* ret = pgen_alloc(alloc,
                         sizeof(bf_astnode_t) +
                         sizeof(bf_astnode_t *) * 3,
                                        _Alignof(bf_astnode_t));
  if (!ret) PGEN_OOM();
  bf_astnode_t *node = (bf_astnode_t *)ret;
  bf_astnode_t **children = (bf_astnode_t **)(node + 1);
  node->kind = kind;
  node->parent = NULL;
  node->max_children = 0;
  node->num_children = 3;
  node->children = children;
  children[0] = n0;
  children[1] = n1;
  children[2] = n2;
  return node;
}

static inline bf_astnode_t* bf_astnode_fixed_4(
                             pgen_allocator* alloc,
                             bf_astnode_kind kind,
                             bf_astnode_t* n0,
                             bf_astnode_t* n1,
                             bf_astnode_t* n2,
                             bf_astnode_t* n3) {
  char* ret = pgen_alloc(alloc,
                         sizeof(bf_astnode_t) +
                         sizeof(bf_astnode_t *) * 4,
                                        _Alignof(bf_astnode_t));
  if (!ret) PGEN_OOM();
  bf_astnode_t *node = (bf_astnode_t *)ret;
  bf_astnode_t **children = (bf_astnode_t **)(node + 1);
  node->kind = kind;
  node->parent = NULL;
  node->max_children = 0;
  node->num_children = 4;
  node->children = children;
  children[0] = n0;
  children[1] = n1;
  children[2] = n2;
  children[3] = n3;
  return node;
}

static inline bf_astnode_t* bf_astnode_fixed_5(
                             pgen_allocator* alloc,
                             bf_astnode_kind kind,
                             bf_astnode_t* n0,
                             bf_astnode_t* n1,
                             bf_astnode_t* n2,
                             bf_astnode_t* n3,
                             bf_astnode_t* n4) {
  char* ret = pgen_alloc(alloc,
                         sizeof(bf_astnode_t) +
                         sizeof(bf_astnode_t *) * 5,
                                        _Alignof(bf_astnode_t));
  if (!ret) PGEN_OOM();
  bf_astnode_t *node = (bf_astnode_t *)ret;
  bf_astnode_t **children = (bf_astnode_t **)(node + 1);
  node->kind = kind;
  node->parent = NULL;
  node->max_children = 0;
  node->num_children = 5;
  node->children = children;
  children[0] = n0;
  children[1] = n1;
  children[2] = n2;
  children[3] = n3;
  children[4] = n4;
  return node;
}

static inline void bf_astnode_add(pgen_allocator* alloc, bf_astnode_t *list, bf_astnode_t *node) {
  if (list->max_children == list->num_children) {
    size_t new_max = list->max_children * 2;
    void* old_ptr = list->children;
    void* new_ptr = realloc(list->children, new_max);
    if (!new_ptr)
      PGEN_OOM();
    list->children = (bf_astnode_t **)new_ptr;
    list->max_children = new_max;
    pgen_allocator_realloced(alloc, old_ptr, new_ptr, free);
  }
  node->parent = list;
  list->children[list->num_children++] = node;
}

static inline void bf_parser_rewind(bf_parser_ctx *ctx, pgen_parser_rewind_t rew) {
  pgen_allocator_rewind(ctx->alloc, rew.arew);
  ctx->pos = rew.prew;
}

#define rec(label)               pgen_parser_rewind_t _rew_##label = (pgen_parser_rewind_t){ctx->alloc->rew, ctx->pos};
#define rew(label)               bf_parser_rewind(ctx, _rew_##label)
#define node(kind, ...)          PGEN_CAT(bf_astnode_fixed_, PGEN_NARG(__VA_ARGS__))(ctx->alloc, BF_NODE_##kind, __VA_ARGS__)
#define list(kind)               bf_astnode_list(ctx->alloc, BF_NODE_##kind, 16)
#define leaf(kind)               bf_astnode_leaf(ctx->alloc, BF_NODE_##kind)
#define add(list, node)  bf_astnode_add(ctx->alloc, list, node)
#define defer(node, freefn, ptr) pgen_defer(ctx->alloc, freefn, ptr, ctx->alloc->rew)
#define SUCC                     ((bf_astnode_t*)(void*)(uintptr_t)_Alignof(bf_astnode_t))

static inline void bf_astnode_print_h(bf_astnode_t *node, size_t depth, int fl) {
  #define indent() for (size_t i = 0; i < depth; i++) printf("  ")
  if (node == SUCC)
    puts("ERROR, CAPTURED SUCC."), exit(1);

  indent(); puts("{");
  depth++;
  indent(); printf("\"kind\": "); printf("\"%s\",\n", bf_nodekind_name[node->kind] + 8);
  size_t cnum = node->num_children;
  indent(); printf("\"num_children\": %zu,\n", cnum);
  indent(); printf("\"children\": [");  if (cnum) {
    putchar('\n');
    for (size_t i = 0; i < cnum; i++)
      bf_astnode_print_h(node->children[i], depth + 1, i == cnum - 1);
    indent();
  }
  printf("]\n");
  depth--;
  indent(); putchar('}'); if (fl != 1) putchar(','); putchar('\n');
}

static inline void bf_astnode_print_json(bf_astnode_t *node) {
  bf_astnode_print_h(node, 0, 1);
}

static inline bf_astnode_t* bf_parse_prog(bf_parser_ctx* ctx);
static inline bf_astnode_t* bf_parse_char(bf_parser_ctx* ctx);


static inline bf_astnode_t* bf_parse_prog(bf_parser_ctx* ctx) {
  bf_astnode_t* expr_ret_1 = NULL;
  bf_astnode_t* expr_ret_0 = NULL;
  #define rule expr_ret_0

  bf_astnode_t* expr_ret_2 = NULL;
  rec(mod_2);
  // ModExprList 0
  {
    // CodeExpr
    #define ret expr_ret_2
    ret = SUCC;

    memset(array, 0, ARRSIZE), ptr = array;

    #undef ret
  }

  // ModExprList 1
  if (expr_ret_2)
  {
    bf_astnode_t* expr_ret_3 = NULL;
    expr_ret_3 = SUCC;
    while (expr_ret_3)
    {
      expr_ret_3 = bf_parse_char(ctx);
    }

    expr_ret_3 = SUCC;
    expr_ret_2 = expr_ret_3;
  }

  // ModExprList end
  if (!expr_ret_2) rew(mod_2);
  expr_ret_1 = expr_ret_2 ? SUCC : NULL;
  return expr_ret_1 ? rule : NULL;
  #undef rule
}

static inline bf_astnode_t* bf_parse_char(bf_parser_ctx* ctx) {
  bf_astnode_t* expr_ret_5 = NULL;
  bf_astnode_t* expr_ret_4 = NULL;
  #define rule expr_ret_4

  bf_astnode_t* expr_ret_6 = NULL;

  rec(slash_6);

  // SlashExpr 0
  if (!expr_ret_6)
  {
    bf_astnode_t* expr_ret_7 = NULL;
    rec(mod_7);
    // ModExprList 0
    {
      if (ctx->tokens[ctx->pos].kind == BF_TOK_PLUS) {
        // Not capturing PLUS.
        expr_ret_7 = SUCC;
        ctx->pos++;
      } else {
        expr_ret_7 = NULL;
      }

    }

    // ModExprList 1
    if (expr_ret_7)
    {
      // CodeExpr
      #define ret expr_ret_7
      ret = SUCC;

      ++*ptr;

      #undef ret
    }

    // ModExprList end
    if (!expr_ret_7) rew(mod_7);
    expr_ret_6 = expr_ret_7 ? SUCC : NULL;
  }

  // SlashExpr 1
  if (!expr_ret_6)
  {
    bf_astnode_t* expr_ret_8 = NULL;
    rec(mod_8);
    // ModExprList 0
    {
      if (ctx->tokens[ctx->pos].kind == BF_TOK_MINUS) {
        // Not capturing MINUS.
        expr_ret_8 = SUCC;
        ctx->pos++;
      } else {
        expr_ret_8 = NULL;
      }

    }

    // ModExprList 1
    if (expr_ret_8)
    {
      // CodeExpr
      #define ret expr_ret_8
      ret = SUCC;

      --*ptr;

      #undef ret
    }

    // ModExprList end
    if (!expr_ret_8) rew(mod_8);
    expr_ret_6 = expr_ret_8 ? SUCC : NULL;
  }

  // SlashExpr 2
  if (!expr_ret_6)
  {
    bf_astnode_t* expr_ret_9 = NULL;
    rec(mod_9);
    // ModExprList 0
    {
      if (ctx->tokens[ctx->pos].kind == BF_TOK_RS) {
        // Not capturing RS.
        expr_ret_9 = SUCC;
        ctx->pos++;
      } else {
        expr_ret_9 = NULL;
      }

    }

    // ModExprList 1
    if (expr_ret_9)
    {
      // CodeExpr
      #define ret expr_ret_9
      ret = SUCC;

      ++ptr;

      #undef ret
    }

    // ModExprList end
    if (!expr_ret_9) rew(mod_9);
    expr_ret_6 = expr_ret_9 ? SUCC : NULL;
  }

  // SlashExpr 3
  if (!expr_ret_6)
  {
    bf_astnode_t* expr_ret_10 = NULL;
    rec(mod_10);
    // ModExprList 0
    {
      if (ctx->tokens[ctx->pos].kind == BF_TOK_LS) {
        // Not capturing LS.
        expr_ret_10 = SUCC;
        ctx->pos++;
      } else {
        expr_ret_10 = NULL;
      }

    }

    // ModExprList 1
    if (expr_ret_10)
    {
      // CodeExpr
      #define ret expr_ret_10
      ret = SUCC;

      --ptr;

      #undef ret
    }

    // ModExprList end
    if (!expr_ret_10) rew(mod_10);
    expr_ret_6 = expr_ret_10 ? SUCC : NULL;
  }

  // SlashExpr 4
  if (!expr_ret_6)
  {
    bf_astnode_t* expr_ret_11 = NULL;
    rec(mod_11);
    // ModExprList 0
    {
      if (ctx->tokens[ctx->pos].kind == BF_TOK_LBRACK) {
        // Not capturing LBRACK.
        expr_ret_11 = SUCC;
        ctx->pos++;
      } else {
        expr_ret_11 = NULL;
      }

    }

    // ModExprList 1
    if (expr_ret_11)
    {
      // CodeExpr
      #define ret expr_ret_11
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
    if (!expr_ret_11) rew(mod_11);
    expr_ret_6 = expr_ret_11 ? SUCC : NULL;
  }

  // SlashExpr 5
  if (!expr_ret_6)
  {
    bf_astnode_t* expr_ret_12 = NULL;
    rec(mod_12);
    // ModExprList 0
    {
      if (ctx->tokens[ctx->pos].kind == BF_TOK_RBRACK) {
        // Not capturing RBRACK.
        expr_ret_12 = SUCC;
        ctx->pos++;
      } else {
        expr_ret_12 = NULL;
      }

    }

    // ModExprList 1
    if (expr_ret_12)
    {
      // CodeExpr
      #define ret expr_ret_12
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
    if (!expr_ret_12) rew(mod_12);
    expr_ret_6 = expr_ret_12 ? SUCC : NULL;
  }

  // SlashExpr 6
  if (!expr_ret_6)
  {
    bf_astnode_t* expr_ret_13 = NULL;
    rec(mod_13);
    // ModExprList 0
    {
      if (ctx->tokens[ctx->pos].kind == BF_TOK_PUTC) {
        // Not capturing PUTC.
        expr_ret_13 = SUCC;
        ctx->pos++;
      } else {
        expr_ret_13 = NULL;
      }

    }

    // ModExprList 1
    if (expr_ret_13)
    {
      // CodeExpr
      #define ret expr_ret_13
      ret = SUCC;

      putchar(*ptr);

      #undef ret
    }

    // ModExprList end
    if (!expr_ret_13) rew(mod_13);
    expr_ret_6 = expr_ret_13 ? SUCC : NULL;
  }

  // SlashExpr 7
  if (!expr_ret_6)
  {
    bf_astnode_t* expr_ret_14 = NULL;
    rec(mod_14);
    // ModExprList 0
    {
      if (ctx->tokens[ctx->pos].kind == BF_TOK_GETC) {
        // Not capturing GETC.
        expr_ret_14 = SUCC;
        ctx->pos++;
      } else {
        expr_ret_14 = NULL;
      }

    }

    // ModExprList 1
    if (expr_ret_14)
    {
      // CodeExpr
      #define ret expr_ret_14
      ret = SUCC;

      *ptr=getchar();

      #undef ret
    }

    // ModExprList end
    if (!expr_ret_14) rew(mod_14);
    expr_ret_6 = expr_ret_14 ? SUCC : NULL;
  }

  // SlashExpr 8
  if (!expr_ret_6)
  {
    bf_astnode_t* expr_ret_15 = NULL;
    rec(mod_15);
    // ModExprList 0
    {
      if (ctx->tokens[ctx->pos].kind == BF_TOK_COMMENT) {
        // Not capturing COMMENT.
        expr_ret_15 = SUCC;
        ctx->pos++;
      } else {
        expr_ret_15 = NULL;
      }

    }

    // ModExprList 1
    if (expr_ret_15)
    {
      // CodeExpr
      #define ret expr_ret_15
      ret = SUCC;

      ;

      #undef ret
    }

    // ModExprList end
    if (!expr_ret_15) rew(mod_15);
    expr_ret_6 = expr_ret_15 ? SUCC : NULL;
  }

  // SlashExpr end
  if (!expr_ret_6) rew(slash_6);
  expr_ret_5 = expr_ret_6;

  return expr_ret_5 ? rule : NULL;
  #undef rule
}



#undef rec
#undef rew
#undef node
#undef list
#undef leaf
#undef add
#undef defer
#undef SUCC

#endif /* PGEN_BF_ASTNODE_INCLUDE */

