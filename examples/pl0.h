
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


/**************/
/* Directives */
/**************/
#define PGEN_OOM() exit(1)


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
  char *buf;
} pgen_allocator_ret_t;

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
  for (size_t i = 0; i < NUM_ARENAS; i++) {
    pgen_arena_t a = allocator->arenas[i];
    if (a.freefn)
      a.freefn(a.buf);
  }
  free(allocator->freelist.entries);
}

#define PGEN_ALLOC_OF(allocator, type)                                         \
  pgen_alloc(allocator, sizeof(type), _Alignof(type))
static inline pgen_allocator_ret_t pgen_alloc(pgen_allocator *allocator,
                                              size_t n, size_t alignment) {

  pgen_allocator_ret_t ret;
  ret.rew = allocator->rew;
  ret.buf = NULL;

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

  ret.buf = allocator->arenas[allocator->rew.arena_idx].buf + bufcurrent;
  allocator->rew.filled = bufnext;

  return ret;
}

static inline void pgen_allocator_realloced(pgen_allocator *allocator,
                                            void *old_ptr, void *new_ptr,
                                            void (*new_free_fn)(void *),
                                            pgen_allocator_rewind_t new_rew) {

  printf("realloc(%p -> %p), (%u, %u)): ", old_ptr, new_ptr, new_rew.arena_idx,
         new_rew.filled);
  for (size_t i = 0; i < allocator->freelist.len; i++) {
    printf("(%p, %p, (%u, %u)) ", allocator->freelist.entries->freefn,
           allocator->freelist.entries->ptr,
           allocator->freelist.entries->rew.arena_idx,
           allocator->freelist.entries->rew.filled);
  }
  puts("");

  for (size_t i = 0; i < allocator->freelist.len; i++) {
    void *ptr = allocator->freelist.entries[i].ptr;
    if (ptr == old_ptr) {
      allocator->freelist.entries[i].ptr = new_ptr;
      allocator->freelist.entries[i].freefn = new_free_fn;
      allocator->freelist.entries[i].rew = new_rew;
      return;
    }
  }

  printf("Realloced: ");
  for (size_t i = 0; i < allocator->freelist.len; i++) {
    printf("(%p, %p, (%u, %u)) ", allocator->freelist.entries->freefn,
           allocator->freelist.entries->ptr,
           allocator->freelist.entries->rew.arena_idx,
           allocator->freelist.entries->rew.filled);
  }
}

static inline void pgen_defer(pgen_allocator *allocator, void (*freefn)(void *),
                              void *ptr, pgen_allocator_rewind_t rew) {
  printf("defer(%p, (%u, %u)): ", ptr, rew.arena_idx, rew.filled);
  for (size_t i = 0; i < allocator->freelist.len; i++) {
    printf("(%p, %p, (%u, %u)) ", allocator->freelist.entries->freefn,
           allocator->freelist.entries->ptr,
           allocator->freelist.entries->rew.arena_idx,
           allocator->freelist.entries->rew.filled);
  }
  puts("");

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

  printf("Deferred: ");
  for (size_t i = 0; i < allocator->freelist.len; i++) {
    printf("(%p, (%u, %u)) ", allocator->freelist.entries->ptr,
           allocator->freelist.entries->rew.arena_idx,
           allocator->freelist.entries->rew.filled);
  }
}

static inline void pgen_allocator_rewind(pgen_allocator *allocator,
                                         pgen_allocator_rewind_t rew) {

  printf("rewind((%u, %u) -> (%u, %u)): ", rew.arena_idx, rew.filled,
         allocator->freelist.entries->rew.arena_idx,
         allocator->freelist.entries->rew.filled);
  for (size_t i = 0; i < allocator->freelist.len; i++) {
    printf("(%p, %p, (%u, %u)) ", allocator->freelist.entries->freefn,
           allocator->freelist.entries->ptr,
           allocator->freelist.entries->rew.arena_idx,
           allocator->freelist.entries->rew.filled);
  }
  puts("");

  // Free all the objects associated with nodes implicitly destroyed.
  size_t i = allocator->freelist.len;
  while (i) {
    i--;

    pgen_freelist_entry_t entry = allocator->freelist.entries[i];
    uint32_t arena_idx = entry.rew.arena_idx;
    uint32_t filled = entry.rew.filled;

    if ((rew.arena_idx < arena_idx) | (rew.filled < filled))
      break;

    entry.freefn(entry.ptr);
  }
  allocator->freelist.len = i;
  allocator->rew = rew;

  printf("rewound(%u, %u): ", rew.arena_idx, rew.filled);
  for (size_t i = 0; i < allocator->freelist.len; i++) {
    printf("(%p, %p, (%u, %u)) ", allocator->freelist.entries->freefn,
           allocator->freelist.entries->ptr,
           allocator->freelist.entries->rew.arena_idx,
           allocator->freelist.entries->rew.filled);
  }
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

#ifndef PL0_TOKENIZER_INCLUDE
#define PL0_TOKENIZER_INCLUDE

#ifndef PL0_TOKENIZER_SOURCEINFO
#define PL0_TOKENIZER_SOURCEINFO 1
#endif

typedef enum {
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

// The 0th token is end of stream.
// Tokens 1 through 32 are the ones you defined.
// This totals 33 kinds of tokens.
#define PL0_NUM_TOKENKINDS 33
static const char* pl0_tokenkind_name[PL0_NUM_TOKENKINDS] = {
  "PL0_TOK_STREAMEND",
  "PL0_TOK_EQ",
  "PL0_TOK_CEQ",
  "PL0_TOK_SEMI",
  "PL0_TOK_DOT",
  "PL0_TOK_COMMA",
  "PL0_TOK_OPEN",
  "PL0_TOK_CLOSE",
  "PL0_TOK_HASH",
  "PL0_TOK_LT",
  "PL0_TOK_LEQ",
  "PL0_TOK_GT",
  "PL0_TOK_GEQ",
  "PL0_TOK_PLUS",
  "PL0_TOK_MINUS",
  "PL0_TOK_STAR",
  "PL0_TOK_DIV",
  "PL0_TOK_VAR",
  "PL0_TOK_PROC",
  "PL0_TOK_CONST",
  "PL0_TOK_BEGIN",
  "PL0_TOK_END",
  "PL0_TOK_IF",
  "PL0_TOK_THEN",
  "PL0_TOK_WHILE",
  "PL0_TOK_DO",
  "PL0_TOK_ODD",
  "PL0_TOK_CALL",
  "PL0_TOK_IDENT",
  "PL0_TOK_NUM",
  "PL0_TOK_WS",
  "PL0_TOK_MLCOM",
  "PL0_TOK_SLCOM",
};

typedef struct {
  pl0_token_kind kind;
  size_t start; // The token begins at tokenizer->start[token->start].
  size_t len;   // It goes until tokenizer->start[token->start + token->len] (non-inclusive).
#if PL0_TOKENIZER_SOURCEINFO
  size_t line;
  size_t col;
  char* sourceFile;
#endif
#ifdef PL0_TOKEN_EXTRA
  PL0_TOKEN_EXTRA
#endif
} pl0_token;

typedef struct {
  codepoint_t* start;
  size_t len;
  size_t pos;
#if PL0_TOKENIZER_SOURCEINFO
  size_t pos_line;
  size_t pos_col;
  char* pos_sourceFile;
#endif
} pl0_tokenizer;

static inline void pl0_tokenizer_init(pl0_tokenizer* tokenizer, codepoint_t* start, size_t len, char* sourceFile) {
  tokenizer->start = start;
  tokenizer->len = len;
  tokenizer->pos = 0;
#if PL0_TOKENIZER_SOURCEINFO
  tokenizer->pos_line = 0;
  tokenizer->pos_col = 0;
  tokenizer->pos_sourceFile = sourceFile;
#else
  (void)sourceFile;
#endif
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
        else if (c == 98 /*'b'*/) trie_state = 35;
        else if (c == 99 /*'c'*/) trie_state = 30;
        else if (c == 100 /*'d'*/) trie_state = 54;
        else if (c == 101 /*'e'*/) trie_state = 40;
        else if (c == 105 /*'i'*/) trie_state = 43;
        else if (c == 111 /*'o'*/) trie_state = 56;
        else if (c == 112 /*'p'*/) trie_state = 21;
        else if (c == 116 /*'t'*/) trie_state = 45;
        else if (c == 118 /*'v'*/) trie_state = 18;
        else if (c == 119 /*'w'*/) trie_state = 49;
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
        if (c == 97 /*'a'*/) trie_state = 59;
        else if (c == 111 /*'o'*/) trie_state = 31;
        else trie_state = -1;
      }
      else if (trie_state == 31) {
        if (c == 110 /*'n'*/) trie_state = 32;
        else trie_state = -1;
      }
      else if (trie_state == 32) {
        if (c == 115 /*'s'*/) trie_state = 33;
        else trie_state = -1;
      }
      else if (trie_state == 33) {
        if (c == 116 /*'t'*/) trie_state = 34;
        else trie_state = -1;
      }
      else if (trie_state == 35) {
        if (c == 101 /*'e'*/) trie_state = 36;
        else trie_state = -1;
      }
      else if (trie_state == 36) {
        if (c == 103 /*'g'*/) trie_state = 37;
        else trie_state = -1;
      }
      else if (trie_state == 37) {
        if (c == 105 /*'i'*/) trie_state = 38;
        else trie_state = -1;
      }
      else if (trie_state == 38) {
        if (c == 110 /*'n'*/) trie_state = 39;
        else trie_state = -1;
      }
      else if (trie_state == 40) {
        if (c == 110 /*'n'*/) trie_state = 41;
        else trie_state = -1;
      }
      else if (trie_state == 41) {
        if (c == 100 /*'d'*/) trie_state = 42;
        else trie_state = -1;
      }
      else if (trie_state == 43) {
        if (c == 102 /*'f'*/) trie_state = 44;
        else trie_state = -1;
      }
      else if (trie_state == 45) {
        if (c == 104 /*'h'*/) trie_state = 46;
        else trie_state = -1;
      }
      else if (trie_state == 46) {
        if (c == 101 /*'e'*/) trie_state = 47;
        else trie_state = -1;
      }
      else if (trie_state == 47) {
        if (c == 110 /*'n'*/) trie_state = 48;
        else trie_state = -1;
      }
      else if (trie_state == 49) {
        if (c == 104 /*'h'*/) trie_state = 50;
        else trie_state = -1;
      }
      else if (trie_state == 50) {
        if (c == 105 /*'i'*/) trie_state = 51;
        else trie_state = -1;
      }
      else if (trie_state == 51) {
        if (c == 108 /*'l'*/) trie_state = 52;
        else trie_state = -1;
      }
      else if (trie_state == 52) {
        if (c == 101 /*'e'*/) trie_state = 53;
        else trie_state = -1;
      }
      else if (trie_state == 54) {
        if (c == 111 /*'o'*/) trie_state = 55;
        else trie_state = -1;
      }
      else if (trie_state == 56) {
        if (c == 100 /*'d'*/) trie_state = 57;
        else trie_state = -1;
      }
      else if (trie_state == 57) {
        if (c == 100 /*'d'*/) trie_state = 58;
        else trie_state = -1;
      }
      else if (trie_state == 59) {
        if (c == 108 /*'l'*/) trie_state = 60;
        else trie_state = -1;
      }
      else if (trie_state == 60) {
        if (c == 108 /*'l'*/) trie_state = 61;
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
        trie_tokenkind =  PL0_TOK_CONST;
        trie_munch_size = iidx + 1;
      }
      else if (trie_state == 39) {
        trie_tokenkind =  PL0_TOK_BEGIN;
        trie_munch_size = iidx + 1;
      }
      else if (trie_state == 42) {
        trie_tokenkind =  PL0_TOK_END;
        trie_munch_size = iidx + 1;
      }
      else if (trie_state == 44) {
        trie_tokenkind =  PL0_TOK_IF;
        trie_munch_size = iidx + 1;
      }
      else if (trie_state == 48) {
        trie_tokenkind =  PL0_TOK_THEN;
        trie_munch_size = iidx + 1;
      }
      else if (trie_state == 53) {
        trie_tokenkind =  PL0_TOK_WHILE;
        trie_munch_size = iidx + 1;
      }
      else if (trie_state == 55) {
        trie_tokenkind =  PL0_TOK_DO;
        trie_munch_size = iidx + 1;
      }
      else if (trie_state == 58) {
        trie_tokenkind =  PL0_TOK_ODD;
        trie_munch_size = iidx + 1;
      }
      else if (trie_state == 61) {
        trie_tokenkind =  PL0_TOK_CALL;
        trie_munch_size = iidx + 1;
      }
    }

    // Transition IDENT State Machine
    if (smaut_state_0 != -1) {
      all_dead = 0;

      if ((smaut_state_0 == 0) &
         ((c == 95) | ((c >= 97) & (c <= 122)) | ((c >= 65) & (c <= 90)))) {
          smaut_state_0 = 1;
      }
      else if (((smaut_state_0 == 1) | (smaut_state_0 == 2)) &
         ((c == 95) | ((c >= 97) & (c <= 122)) | ((c >= 65) & (c <= 90)) | ((c >= 48) & (c <= 57)))) {
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
         ((c == 45) | (c == 43))) {
          smaut_state_1 = 1;
      }
      else if (((smaut_state_1 == 0) | (smaut_state_1 == 1) | (smaut_state_1 == 2)) &
         (((c >= 48) & (c <= 57)))) {
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
         ((c == 32) | (c == 10) | (c == 13) | (c == 9))) {
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
         (c == 47)) {
          smaut_state_3 = 1;
      }
      else if ((smaut_state_3 == 1) &
         (c == 42)) {
          smaut_state_3 = 2;
      }
      else if ((smaut_state_3 == 2) &
         (c == 42)) {
          smaut_state_3 = 3;
      }
      else if ((smaut_state_3 == 2) &
         (1)) {
          smaut_state_3 = 2;
      }
      else if ((smaut_state_3 == 3) &
         (c == 42)) {
          smaut_state_3 = 3;
      }
      else if ((smaut_state_3 == 3) &
         (c == 47)) {
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
         (c == 47)) {
          smaut_state_4 = 1;
      }
      else if ((smaut_state_4 == 1) &
         (c == 47)) {
          smaut_state_4 = 2;
      }
      else if ((smaut_state_4 == 2) &
         (!(c == 10))) {
          smaut_state_4 = 2;
      }
      else if ((smaut_state_4 == 2) &
         (c == 10)) {
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

  pl0_token ret;
  ret.kind = kind;
  ret.start = tokenizer->pos;
  ret.len = max_munch;

#if PL0_TOKENIZER_SOURCEINFO
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

#endif /* PL0_TOKENIZER_INCLUDE */

#ifndef PGEN_PL0_ASTNODE_INCLUDE
#define PGEN_PL0_ASTNODE_INCLUDE

typedef struct {
  pl0_token* tokens;
  size_t len;
  size_t pos;
  pgen_allocator *alloc;
} pl0_parser_ctx;

static inline void pl0_parser_ctx_init(pl0_parser_ctx* parser,
                                       pgen_allocator* allocator,
                                       pl0_token* tokens, size_t num_tokens) {
  parser->tokens = tokens;
  parser->len = num_tokens;
  parser->pos = 0;
  parser->alloc = allocator;
}
typedef enum {
  pl0_NODE_EMPTY,
  PL0_NODE_BLOCKLIST,
  PL0_NODE_VAR,
  PL0_NODE_VARLIST,
  PL0_NODE_CONST,
  PL0_NODE_CONSTLIST,
  PL0_NODE_PROC,
  PL0_NODE_PROCLIST,
  PL0_NODE_CEQ,
  PL0_NODE_CALL,
  PL0_NODE_BEGIN,
  PL0_NODE_IF,
  PL0_NODE_WHILE,
  PL0_NODE_STATEMENT,
  PL0_NODE_EXPRS,
  PL0_NODE_UNEXPR,
  PL0_NODE_BINEXPR,
  PL0_NODE_EQ,
  PL0_NODE_HASH,
  PL0_NODE_LT,
  PL0_NODE_LEQ,
  PL0_NODE_GT,
  PL0_NODE_GEQ,
  PL0_NODE_PLUS,
  PL0_NODE_MINUS,
  PL0_NODE_SIGN,
  PL0_NODE_STAR,
  PL0_NODE_DIV,
  PL0_NODE_IDENT,
  PL0_NODE_NUM,
} pl0_astnode_kind;

#define PL0_NUM_NODEKINDS 30
static const char* pl0_nodekind_name[PL0_NUM_NODEKINDS] = {
  "PL0_NODE_EMPTY",
  "PL0_NODE_BLOCKLIST",
  "PL0_NODE_VAR",
  "PL0_NODE_VARLIST",
  "PL0_NODE_CONST",
  "PL0_NODE_CONSTLIST",
  "PL0_NODE_PROC",
  "PL0_NODE_PROCLIST",
  "PL0_NODE_CEQ",
  "PL0_NODE_CALL",
  "PL0_NODE_BEGIN",
  "PL0_NODE_IF",
  "PL0_NODE_WHILE",
  "PL0_NODE_STATEMENT",
  "PL0_NODE_EXPRS",
  "PL0_NODE_UNEXPR",
  "PL0_NODE_BINEXPR",
  "PL0_NODE_EQ",
  "PL0_NODE_HASH",
  "PL0_NODE_LT",
  "PL0_NODE_LEQ",
  "PL0_NODE_GT",
  "PL0_NODE_GEQ",
  "PL0_NODE_PLUS",
  "PL0_NODE_MINUS",
  "PL0_NODE_SIGN",
  "PL0_NODE_STAR",
  "PL0_NODE_DIV",
  "PL0_NODE_IDENT",
  "PL0_NODE_NUM",
};

struct pl0_astnode_t;
typedef struct pl0_astnode_t pl0_astnode_t;
struct pl0_astnode_t {
  // No %extra directives.

  pl0_astnode_kind kind;
  size_t num_children;
  size_t max_children;
  pl0_astnode_t** children;
  pgen_allocator_rewind_t rew;
};

static inline pl0_astnode_t* pl0_astnode_list(
                             pgen_allocator* alloc,
                             pl0_astnode_kind kind,
                             size_t initial_size) {
  pgen_allocator_ret_t ret = pgen_alloc(alloc,
                                        sizeof(pl0_astnode_t),
                                        _Alignof(pl0_astnode_t));
  pl0_astnode_t *node = (pl0_astnode_t*)ret.buf;

  pl0_astnode_t **children;
  if (initial_size) {
    children = (pl0_astnode_t**)malloc(sizeof(pl0_astnode_t*) * initial_size);
    if (!children)
      PGEN_OOM();
    pgen_defer(alloc, free, children, ret.rew);
  } else {
    children = NULL;
  }

  node->kind = kind;
  node->max_children = initial_size;
  node->num_children = 0;
  node->children = children;
  node->rew = ret.rew;
  return node;
}

static inline pl0_astnode_t* pl0_astnode_leaf(
                             pgen_allocator* alloc,
                             pl0_astnode_kind kind) {
  pgen_allocator_ret_t ret = pgen_alloc(alloc,
                                        sizeof(pl0_astnode_t),
                                        _Alignof(pl0_astnode_t));
  pl0_astnode_t *node = (pl0_astnode_t *)ret.buf;
  pl0_astnode_t *children = NULL;
  node->kind = kind;
  node->max_children = 0;
  node->num_children = 0;
  node->children = NULL;
  node->rew = ret.rew;
  return node;
}

static inline pl0_astnode_t* pl0_astnode_fixed_1(
                             pgen_allocator* alloc,
                             pl0_astnode_kind kind,
                             pl0_astnode_t* n0) {
  pgen_allocator_ret_t ret = pgen_alloc(alloc,
                                        sizeof(pl0_astnode_t) +
                                        sizeof(pl0_astnode_t *) * 1,
                                        _Alignof(pl0_astnode_t));
  pl0_astnode_t *node = (pl0_astnode_t *)ret.buf;
  pl0_astnode_t **children = (pl0_astnode_t **)(node + 1);
  node->kind = kind;
  node->max_children = 0;
  node->num_children = 1;
  node->children = children;
  node->rew = ret.rew;
  children[0] = n0;
  return node;
}

static inline pl0_astnode_t* pl0_astnode_fixed_2(
                             pgen_allocator* alloc,
                             pl0_astnode_kind kind,
                             pl0_astnode_t* n0,
                             pl0_astnode_t* n1) {
  pgen_allocator_ret_t ret = pgen_alloc(alloc,
                                        sizeof(pl0_astnode_t) +
                                        sizeof(pl0_astnode_t *) * 2,
                                        _Alignof(pl0_astnode_t));
  pl0_astnode_t *node = (pl0_astnode_t *)ret.buf;
  pl0_astnode_t **children = (pl0_astnode_t **)(node + 1);
  node->kind = kind;
  node->max_children = 0;
  node->num_children = 2;
  node->children = children;
  node->rew = ret.rew;
  children[0] = n0;
  children[1] = n1;
  return node;
}

static inline pl0_astnode_t* pl0_astnode_fixed_3(
                             pgen_allocator* alloc,
                             pl0_astnode_kind kind,
                             pl0_astnode_t* n0,
                             pl0_astnode_t* n1,
                             pl0_astnode_t* n2) {
  pgen_allocator_ret_t ret = pgen_alloc(alloc,
                                        sizeof(pl0_astnode_t) +
                                        sizeof(pl0_astnode_t *) * 3,
                                        _Alignof(pl0_astnode_t));
  pl0_astnode_t *node = (pl0_astnode_t *)ret.buf;
  pl0_astnode_t **children = (pl0_astnode_t **)(node + 1);
  node->kind = kind;
  node->max_children = 0;
  node->num_children = 3;
  node->children = children;
  node->rew = ret.rew;
  children[0] = n0;
  children[1] = n1;
  children[2] = n2;
  return node;
}

static inline pl0_astnode_t* pl0_astnode_fixed_4(
                             pgen_allocator* alloc,
                             pl0_astnode_kind kind,
                             pl0_astnode_t* n0,
                             pl0_astnode_t* n1,
                             pl0_astnode_t* n2,
                             pl0_astnode_t* n3) {
  pgen_allocator_ret_t ret = pgen_alloc(alloc,
                                        sizeof(pl0_astnode_t) +
                                        sizeof(pl0_astnode_t *) * 4,
                                        _Alignof(pl0_astnode_t));
  pl0_astnode_t *node = (pl0_astnode_t *)ret.buf;
  pl0_astnode_t **children = (pl0_astnode_t **)(node + 1);
  node->kind = kind;
  node->max_children = 0;
  node->num_children = 4;
  node->children = children;
  node->rew = ret.rew;
  children[0] = n0;
  children[1] = n1;
  children[2] = n2;
  children[3] = n3;
  return node;
}

static inline pl0_astnode_t* pl0_astnode_fixed_5(
                             pgen_allocator* alloc,
                             pl0_astnode_kind kind,
                             pl0_astnode_t* n0,
                             pl0_astnode_t* n1,
                             pl0_astnode_t* n2,
                             pl0_astnode_t* n3,
                             pl0_astnode_t* n4) {
  pgen_allocator_ret_t ret = pgen_alloc(alloc,
                                        sizeof(pl0_astnode_t) +
                                        sizeof(pl0_astnode_t *) * 5,
                                        _Alignof(pl0_astnode_t));
  pl0_astnode_t *node = (pl0_astnode_t *)ret.buf;
  pl0_astnode_t **children = (pl0_astnode_t **)(node + 1);
  node->kind = kind;
  node->max_children = 0;
  node->num_children = 5;
  node->children = children;
  node->rew = ret.rew;
  children[0] = n0;
  children[1] = n1;
  children[2] = n2;
  children[3] = n3;
  children[4] = n4;
  return node;
}

static inline void pl0_astnode_add(pgen_allocator* alloc, pl0_astnode_t *list, pl0_astnode_t *node) {
  if (list->num_children > list->max_children)
    PGEN_OOM();

  if (list->max_children == list->num_children) {
    size_t new_max = list->max_children * 2;
    void* old_ptr = list->children;
    void* new_ptr = realloc(list->children, new_max);
    if (!new_ptr)
      PGEN_OOM();
    list->children = (pl0_astnode_t **)new_ptr;
    list->max_children = new_max;
    pgen_allocator_realloced(alloc, old_ptr, new_ptr, free, list->rew);
  }

  printf("list: %p, children: %zu", list, list ? list->num_children : 500);
  fflush(stdout);
  list->children[list->num_children++] = node;
}

static inline void pl0_astnode_print_h(pl0_astnode_t *node, size_t depth) {
  for (size_t i = 0; i < depth; i++) putchar(' ');
  printf("%p\n", node);
  for (size_t i = 0; i < depth; i++)
    pl0_astnode_print_h(node->children[i], depth + 1);
}

static inline void pl0_astnode_print(pl0_astnode_t *node) {
  pl0_astnode_print_h(node, 0);
}

static inline void pl0_parser_rewind(pl0_parser_ctx *ctx, pgen_parser_rewind_t rew) {
  pgen_allocator_rewind(ctx->alloc, rew.arew);
  ctx->pos = rew.prew;
}

#define rec(label)               pgen_parser_rewind_t _rew_##label = (pgen_parser_rewind_t){ctx->alloc->rew, ctx->pos};
#define rew(label)               pl0_parser_rewind(ctx, _rew_##label)
#define node(kind, ...)          PGEN_CAT(pl0_astnode_fixed_, PGEN_NARG(__VA_ARGS__))(ctx->alloc, PL0_NODE_##kind, __VA_ARGS__)
#define list(kind)               pl0_astnode_list(ctx->alloc, PL0_NODE_##kind, 32)
#define leaf(kind)               pl0_astnode_leaf(ctx->alloc, PL0_NODE_##kind)
#define add(list, node)          pl0_astnode_add(ctx->alloc, list, node)
#define defer(node, freefn, ptr) pgen_defer(ctx->alloc, freefn, ptr, node->rew)
#define SUCC                     ((pl0_astnode_t*)(void*)(uintptr_t)_Alignof(pl0_astnode_t))

#ifndef PGEN_DEBUG
#define PGEN_DEBUG

#define PGEN_DEBUG_WIDTH 10
typedef struct {
  const char* rule_name;
  size_t pos;
} dbg_entry;

static struct {
  dbg_entry rules[500];
  size_t size;
  int status;
  int first;
} dbg_stack;

#include <unistd.h>
#include <sys/ioctl.h>
#include <string.h>
static inline void dbg_display(pl0_parser_ctx* ctx, const char* last) {
  if (!dbg_stack.first) dbg_stack.first = 1;
  else getchar();

  struct winsize w;
  ioctl(STDOUT_FILENO, TIOCGWINSZ, &w);
  size_t width = w.ws_col;
  size_t leftwidth = (width - (1 + 3 + 1)) / 2;
  size_t rightwidth = leftwidth + (leftwidth % 2);
  size_t height = w.ws_row - 4;

// Clear screen, cursor to top left
  printf("\x1b[2J\x1b[H");

  // Write first line in color.
  if (dbg_stack.status == -1) {
    printf("\x1b[31m"); // Red
    printf("Failed: %s\n", last);
  } else if (dbg_stack.status == 0) {
    printf("\x1b[34m"); // Blue
    printf("Entering: %s\n", last);
  } else {
    printf("\x1b[32m"); // Green
    printf("Accepted: %s\n", last);
    
  }
  printf("\x1b[0m"); // Clear Formatting

  // Write labels and line.
  for (size_t i = 0; i < width; i++) putchar('-');  // Write following lines
  for (size_t i = height; i --> 0;) {
    putchar(' ');

    // Print rule stack
    if (i < dbg_stack.size) {
      printf("%-10s", dbg_stack.rules[i].rule_name);
    } else {
      for (size_t sp = 0; sp < 10; sp++) putchar(' ');
    }

    printf(" | "); // 3 Separator chars

    // Print tokens
    size_t remaining_tokens = ctx->len - ctx->pos;
    if (i < remaining_tokens) {
      const char* name = pl0_tokenkind_name[ctx->tokens[ctx->pos + i].kind];
      size_t ns = strlen(name);
      size_t remaining = rightwidth - ns;
      printf("%s", name);
      for (size_t sp = 0; sp < remaining; sp++) putchar(' ');
    }

    putchar(' ');
    putchar('\n');
  }
}

#endif /* PGEN_DEBUG */

static inline void dbg_enter(pl0_parser_ctx* ctx, const char* name, size_t pos) {
  dbg_stack.rules[dbg_stack.size++] = (dbg_entry){name, pos};
  dbg_stack.status = 0;
  dbg_display(ctx, name);
}

static inline void dbg_accept(pl0_parser_ctx* ctx, const char* accpeting) {
  dbg_stack.size--;
  dbg_stack.status = 1;
  dbg_display(ctx, accpeting);
}

static inline void dbg_reject(pl0_parser_ctx* ctx, const char* rejecting) {
  dbg_stack.size--;
  dbg_stack.status = -1;
  dbg_display(ctx, rejecting);
}

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
  pl0_astnode_t* expr_ret_1 = NULL;
  pl0_astnode_t* expr_ret_0 = NULL;
  #define rule expr_ret_0

  dbg_enter(ctx, "program", ctx->pos);
  pl0_astnode_t* expr_ret_2 = NULL;
  rec(mod_2);
  // ModExprList 0
  {
    // CodeExpr
    dbg_enter(ctx, "CodeExpr", ctx->pos);
    #define ret expr_ret_2
    ret = SUCC;

    rule=list(BLOCKLIST);

    if (ret) dbg_accept(ctx, "CodeExpr"); else dbg_reject(ctx, "CodeExpr");
    #undef ret
  }

  // ModExprList 1
  if (expr_ret_2)
  {
    pl0_astnode_t* expr_ret_3 = NULL;
    expr_ret_3 = SUCC;
    while (expr_ret_3)
    {
      pl0_astnode_t* expr_ret_4 = NULL;
      rec(mod_4);
      // ModExprList 0
      {
        pl0_astnode_t* expr_ret_5 = NULL;
        expr_ret_5 = pl0_parse_block(ctx);
        expr_ret_4 = expr_ret_5;
        b = expr_ret_5;
      }

      // ModExprList 1
      if (expr_ret_4)
      {
        // CodeExpr
        dbg_enter(ctx, "CodeExpr", ctx->pos);
        #define ret expr_ret_4
        ret = SUCC;

        printf("%p", b);

        if (ret) dbg_accept(ctx, "CodeExpr"); else dbg_reject(ctx, "CodeExpr");
        #undef ret
      }

      // ModExprList 2
      if (expr_ret_4)
      {
        // CodeExpr
        dbg_enter(ctx, "CodeExpr", ctx->pos);
        #define ret expr_ret_4
        ret = SUCC;

        add(rule, b);

        if (ret) dbg_accept(ctx, "CodeExpr"); else dbg_reject(ctx, "CodeExpr");
        #undef ret
      }

      // ModExprList end
      if (!expr_ret_4) rew(mod_4);
      expr_ret_3 = expr_ret_4 ? SUCC : NULL;
    }

    expr_ret_3 = SUCC;
    expr_ret_2 = expr_ret_3;
  }

  // ModExprList 2
  if (expr_ret_2)
  {
    dbg_enter(ctx, "DOT", ctx->pos);
    if (ctx->tokens[ctx->pos].kind == PL0_TOK_DOT) {
      expr_ret_2 = SUCC; // Not capturing DOT.
      ctx->pos++;
    } else {
      expr_ret_2 = NULL;
    }

    if (expr_ret_2) dbg_accept(ctx, "DOT"); else dbg_reject(ctx, "DOT");
  }

  // ModExprList end
  if (!expr_ret_2) rew(mod_2);
  expr_ret_1 = expr_ret_2 ? SUCC : NULL;
  if (expr_ret_1) dbg_accept(ctx, "program"); else dbg_reject(ctx, "program");
  return expr_ret_1 ? rule : NULL;
  #undef rule
}

static inline pl0_astnode_t* pl0_parse_vdef(pl0_parser_ctx* ctx) {
  pl0_astnode_t* i = NULL;
  pl0_astnode_t* n = NULL;
  pl0_astnode_t* e = NULL;
  pl0_astnode_t* expr_ret_7 = NULL;
  pl0_astnode_t* expr_ret_6 = NULL;
  #define rule expr_ret_6

  dbg_enter(ctx, "vdef", ctx->pos);
  pl0_astnode_t* expr_ret_8 = NULL;

  rec(slash_8);

  // SlashExpr 0
  if (!expr_ret_8)
  {
    pl0_astnode_t* expr_ret_9 = NULL;
    rec(mod_9);
    // ModExprList 0
    {
      dbg_enter(ctx, "VAR", ctx->pos);
      if (ctx->tokens[ctx->pos].kind == PL0_TOK_VAR) {
        expr_ret_9 = SUCC; // Not capturing VAR.
        ctx->pos++;
      } else {
        expr_ret_9 = NULL;
      }

      if (expr_ret_9) dbg_accept(ctx, "VAR"); else dbg_reject(ctx, "VAR");
    }

    // ModExprList 1
    if (expr_ret_9)
    {
      // CodeExpr
      dbg_enter(ctx, "CodeExpr", ctx->pos);
      #define ret expr_ret_9
      ret = SUCC;

      rule=list(VARLIST);

      if (ret) dbg_accept(ctx, "CodeExpr"); else dbg_reject(ctx, "CodeExpr");
      #undef ret
    }

    // ModExprList 2
    if (expr_ret_9)
    {
      pl0_astnode_t* expr_ret_10 = NULL;
      dbg_enter(ctx, "IDENT", ctx->pos);
      if (ctx->tokens[ctx->pos].kind == PL0_TOK_IDENT) {
        // Capturing IDENT.
        expr_ret_10 = leaf(IDENT);
        ctx->pos++;
      } else {
        expr_ret_10 = NULL;
      }

      if (expr_ret_10) dbg_accept(ctx, "IDENT"); else dbg_reject(ctx, "IDENT");
      expr_ret_9 = expr_ret_10;
      i = expr_ret_10;
    }

    // ModExprList 3
    if (expr_ret_9)
    {
      // CodeExpr
      dbg_enter(ctx, "CodeExpr", ctx->pos);
      #define ret expr_ret_9
      ret = SUCC;

      add(rule, node(IDENT, i));

      if (ret) dbg_accept(ctx, "CodeExpr"); else dbg_reject(ctx, "CodeExpr");
      #undef ret
    }

    // ModExprList 4
    if (expr_ret_9)
    {
      pl0_astnode_t* expr_ret_11 = NULL;
      expr_ret_11 = SUCC;
      while (expr_ret_11)
      {
        pl0_astnode_t* expr_ret_12 = NULL;
        rec(mod_12);
        // ModExprList 0
        {
          dbg_enter(ctx, "COMMA", ctx->pos);
          if (ctx->tokens[ctx->pos].kind == PL0_TOK_COMMA) {
            expr_ret_12 = SUCC; // Not capturing COMMA.
            ctx->pos++;
          } else {
            expr_ret_12 = NULL;
          }

          if (expr_ret_12) dbg_accept(ctx, "COMMA"); else dbg_reject(ctx, "COMMA");
        }

        // ModExprList 1
        if (expr_ret_12)
        {
          pl0_astnode_t* expr_ret_13 = NULL;
          dbg_enter(ctx, "IDENT", ctx->pos);
          if (ctx->tokens[ctx->pos].kind == PL0_TOK_IDENT) {
            // Capturing IDENT.
            expr_ret_13 = leaf(IDENT);
            ctx->pos++;
          } else {
            expr_ret_13 = NULL;
          }

          if (expr_ret_13) dbg_accept(ctx, "IDENT"); else dbg_reject(ctx, "IDENT");
          expr_ret_12 = expr_ret_13;
          i = expr_ret_13;
        }

        // ModExprList 2
        if (expr_ret_12)
        {
          // CodeExpr
          dbg_enter(ctx, "CodeExpr", ctx->pos);
          #define ret expr_ret_12
          ret = SUCC;

          add(rule, i);

          if (ret) dbg_accept(ctx, "CodeExpr"); else dbg_reject(ctx, "CodeExpr");
          #undef ret
        }

        // ModExprList end
        if (!expr_ret_12) rew(mod_12);
        expr_ret_11 = expr_ret_12 ? SUCC : NULL;
      }

      expr_ret_11 = SUCC;
      expr_ret_9 = expr_ret_11;
    }

    // ModExprList 5
    if (expr_ret_9)
    {
      dbg_enter(ctx, "SEMI", ctx->pos);
      if (ctx->tokens[ctx->pos].kind == PL0_TOK_SEMI) {
        expr_ret_9 = SUCC; // Not capturing SEMI.
        ctx->pos++;
      } else {
        expr_ret_9 = NULL;
      }

      if (expr_ret_9) dbg_accept(ctx, "SEMI"); else dbg_reject(ctx, "SEMI");
    }

    // ModExprList end
    if (!expr_ret_9) rew(mod_9);
    expr_ret_8 = expr_ret_9 ? SUCC : NULL;
  }

  // SlashExpr 1
  if (!expr_ret_8)
  {
    pl0_astnode_t* expr_ret_14 = NULL;
    rec(mod_14);
    // ModExprList 0
    {
      dbg_enter(ctx, "CONST", ctx->pos);
      if (ctx->tokens[ctx->pos].kind == PL0_TOK_CONST) {
        expr_ret_14 = SUCC; // Not capturing CONST.
        ctx->pos++;
      } else {
        expr_ret_14 = NULL;
      }

      if (expr_ret_14) dbg_accept(ctx, "CONST"); else dbg_reject(ctx, "CONST");
    }

    // ModExprList 1
    if (expr_ret_14)
    {
      // CodeExpr
      dbg_enter(ctx, "CodeExpr", ctx->pos);
      #define ret expr_ret_14
      ret = SUCC;

      rule=list(CONSTLIST);

      if (ret) dbg_accept(ctx, "CodeExpr"); else dbg_reject(ctx, "CodeExpr");
      #undef ret
    }

    // ModExprList 2
    if (expr_ret_14)
    {
      pl0_astnode_t* expr_ret_15 = NULL;
      dbg_enter(ctx, "IDENT", ctx->pos);
      if (ctx->tokens[ctx->pos].kind == PL0_TOK_IDENT) {
        // Capturing IDENT.
        expr_ret_15 = leaf(IDENT);
        ctx->pos++;
      } else {
        expr_ret_15 = NULL;
      }

      if (expr_ret_15) dbg_accept(ctx, "IDENT"); else dbg_reject(ctx, "IDENT");
      expr_ret_14 = expr_ret_15;
      i = expr_ret_15;
    }

    // ModExprList 3
    if (expr_ret_14)
    {
      dbg_enter(ctx, "EQ", ctx->pos);
      if (ctx->tokens[ctx->pos].kind == PL0_TOK_EQ) {
        expr_ret_14 = SUCC; // Not capturing EQ.
        ctx->pos++;
      } else {
        expr_ret_14 = NULL;
      }

      if (expr_ret_14) dbg_accept(ctx, "EQ"); else dbg_reject(ctx, "EQ");
    }

    // ModExprList 4
    if (expr_ret_14)
    {
      pl0_astnode_t* expr_ret_16 = NULL;
      dbg_enter(ctx, "NUM", ctx->pos);
      if (ctx->tokens[ctx->pos].kind == PL0_TOK_NUM) {
        // Capturing NUM.
        expr_ret_16 = leaf(NUM);
        ctx->pos++;
      } else {
        expr_ret_16 = NULL;
      }

      if (expr_ret_16) dbg_accept(ctx, "NUM"); else dbg_reject(ctx, "NUM");
      expr_ret_14 = expr_ret_16;
      n = expr_ret_16;
    }

    // ModExprList 5
    if (expr_ret_14)
    {
      // CodeExpr
      dbg_enter(ctx, "CodeExpr", ctx->pos);
      #define ret expr_ret_14
      ret = SUCC;

      add(rule, node(CONST, i, n));

      if (ret) dbg_accept(ctx, "CodeExpr"); else dbg_reject(ctx, "CodeExpr");
      #undef ret
    }

    // ModExprList 6
    if (expr_ret_14)
    {
      pl0_astnode_t* expr_ret_17 = NULL;
      expr_ret_17 = SUCC;
      while (expr_ret_17)
      {
        pl0_astnode_t* expr_ret_18 = NULL;
        rec(mod_18);
        // ModExprList 0
        {
          dbg_enter(ctx, "COMMA", ctx->pos);
          if (ctx->tokens[ctx->pos].kind == PL0_TOK_COMMA) {
            expr_ret_18 = SUCC; // Not capturing COMMA.
            ctx->pos++;
          } else {
            expr_ret_18 = NULL;
          }

          if (expr_ret_18) dbg_accept(ctx, "COMMA"); else dbg_reject(ctx, "COMMA");
        }

        // ModExprList 1
        if (expr_ret_18)
        {
          pl0_astnode_t* expr_ret_19 = NULL;
          dbg_enter(ctx, "IDENT", ctx->pos);
          if (ctx->tokens[ctx->pos].kind == PL0_TOK_IDENT) {
            // Capturing IDENT.
            expr_ret_19 = leaf(IDENT);
            ctx->pos++;
          } else {
            expr_ret_19 = NULL;
          }

          if (expr_ret_19) dbg_accept(ctx, "IDENT"); else dbg_reject(ctx, "IDENT");
          expr_ret_18 = expr_ret_19;
          i = expr_ret_19;
        }

        // ModExprList 2
        if (expr_ret_18)
        {
          pl0_astnode_t* expr_ret_20 = NULL;
          dbg_enter(ctx, "EQ", ctx->pos);
          if (ctx->tokens[ctx->pos].kind == PL0_TOK_EQ) {
            // Capturing EQ.
            expr_ret_20 = leaf(EQ);
            ctx->pos++;
          } else {
            expr_ret_20 = NULL;
          }

          if (expr_ret_20) dbg_accept(ctx, "EQ"); else dbg_reject(ctx, "EQ");
          expr_ret_18 = expr_ret_20;
          e = expr_ret_20;
        }

        // ModExprList 3
        if (expr_ret_18)
        {
          pl0_astnode_t* expr_ret_21 = NULL;
          dbg_enter(ctx, "NUM", ctx->pos);
          if (ctx->tokens[ctx->pos].kind == PL0_TOK_NUM) {
            // Capturing NUM.
            expr_ret_21 = leaf(NUM);
            ctx->pos++;
          } else {
            expr_ret_21 = NULL;
          }

          if (expr_ret_21) dbg_accept(ctx, "NUM"); else dbg_reject(ctx, "NUM");
          expr_ret_18 = expr_ret_21;
          n = expr_ret_21;
        }

        // ModExprList 4
        if (expr_ret_18)
        {
          // CodeExpr
          dbg_enter(ctx, "CodeExpr", ctx->pos);
          #define ret expr_ret_18
          ret = SUCC;

          add(rule, node(CONST, i, n));

          if (ret) dbg_accept(ctx, "CodeExpr"); else dbg_reject(ctx, "CodeExpr");
          #undef ret
        }

        // ModExprList end
        if (!expr_ret_18) rew(mod_18);
        expr_ret_17 = expr_ret_18 ? SUCC : NULL;
      }

      expr_ret_17 = SUCC;
      expr_ret_14 = expr_ret_17;
    }

    // ModExprList 7
    if (expr_ret_14)
    {
      dbg_enter(ctx, "SEMI", ctx->pos);
      if (ctx->tokens[ctx->pos].kind == PL0_TOK_SEMI) {
        expr_ret_14 = SUCC; // Not capturing SEMI.
        ctx->pos++;
      } else {
        expr_ret_14 = NULL;
      }

      if (expr_ret_14) dbg_accept(ctx, "SEMI"); else dbg_reject(ctx, "SEMI");
    }

    // ModExprList end
    if (!expr_ret_14) rew(mod_14);
    expr_ret_8 = expr_ret_14 ? SUCC : NULL;
  }

  // SlashExpr end
  if (!expr_ret_8) rew(slash_8);
  expr_ret_7 = expr_ret_8;

  if (expr_ret_7) dbg_accept(ctx, "vdef"); else dbg_reject(ctx, "vdef");
  return expr_ret_7 ? rule : NULL;
  #undef rule
}

static inline pl0_astnode_t* pl0_parse_block(pl0_parser_ctx* ctx) {
  pl0_astnode_t* v = NULL;
  pl0_astnode_t* i = NULL;
  pl0_astnode_t* s = NULL;
  pl0_astnode_t* expr_ret_23 = NULL;
  pl0_astnode_t* expr_ret_22 = NULL;
  #define rule expr_ret_22

  dbg_enter(ctx, "block", ctx->pos);
  pl0_astnode_t* expr_ret_24 = NULL;

  rec(slash_24);

  // SlashExpr 0
  if (!expr_ret_24)
  {
    pl0_astnode_t* expr_ret_25 = NULL;
    rec(mod_25);
    // ModExprList 0
    {
      pl0_astnode_t* expr_ret_26 = NULL;
      expr_ret_26 = pl0_parse_vdef(ctx);
      expr_ret_25 = expr_ret_26;
      v = expr_ret_26;
    }

    // ModExprList 1
    if (expr_ret_25)
    {
      // CodeExpr
      dbg_enter(ctx, "CodeExpr", ctx->pos);
      #define ret expr_ret_25
      ret = SUCC;

      rule=v;

      if (ret) dbg_accept(ctx, "CodeExpr"); else dbg_reject(ctx, "CodeExpr");
      #undef ret
    }

    // ModExprList end
    if (!expr_ret_25) rew(mod_25);
    expr_ret_24 = expr_ret_25 ? SUCC : NULL;
  }

  // SlashExpr 1
  if (!expr_ret_24)
  {
    pl0_astnode_t* expr_ret_27 = NULL;
    rec(mod_27);
    // ModExprList 0
    {
      // CodeExpr
      dbg_enter(ctx, "CodeExpr", ctx->pos);
      #define ret expr_ret_27
      ret = SUCC;

      rule=list(PROCLIST);

      if (ret) dbg_accept(ctx, "CodeExpr"); else dbg_reject(ctx, "CodeExpr");
      #undef ret
    }

    // ModExprList 1
    if (expr_ret_27)
    {
      pl0_astnode_t* expr_ret_28 = NULL;
      expr_ret_28 = SUCC;
      while (expr_ret_28)
      {
        pl0_astnode_t* expr_ret_29 = NULL;
        rec(mod_29);
        // ModExprList 0
        {
          dbg_enter(ctx, "PROC", ctx->pos);
          if (ctx->tokens[ctx->pos].kind == PL0_TOK_PROC) {
            expr_ret_29 = SUCC; // Not capturing PROC.
            ctx->pos++;
          } else {
            expr_ret_29 = NULL;
          }

          if (expr_ret_29) dbg_accept(ctx, "PROC"); else dbg_reject(ctx, "PROC");
        }

        // ModExprList 1
        if (expr_ret_29)
        {
          pl0_astnode_t* expr_ret_30 = NULL;
          dbg_enter(ctx, "IDENT", ctx->pos);
          if (ctx->tokens[ctx->pos].kind == PL0_TOK_IDENT) {
            // Capturing IDENT.
            expr_ret_30 = leaf(IDENT);
            ctx->pos++;
          } else {
            expr_ret_30 = NULL;
          }

          if (expr_ret_30) dbg_accept(ctx, "IDENT"); else dbg_reject(ctx, "IDENT");
          expr_ret_29 = expr_ret_30;
          i = expr_ret_30;
        }

        // ModExprList 2
        if (expr_ret_29)
        {
          dbg_enter(ctx, "SEMI", ctx->pos);
          if (ctx->tokens[ctx->pos].kind == PL0_TOK_SEMI) {
            expr_ret_29 = SUCC; // Not capturing SEMI.
            ctx->pos++;
          } else {
            expr_ret_29 = NULL;
          }

          if (expr_ret_29) dbg_accept(ctx, "SEMI"); else dbg_reject(ctx, "SEMI");
        }

        // ModExprList 3
        if (expr_ret_29)
        {
          pl0_astnode_t* expr_ret_31 = NULL;
          expr_ret_31 = pl0_parse_vdef(ctx);
          expr_ret_29 = expr_ret_31;
          v = expr_ret_31;
        }

        // ModExprList 4
        if (expr_ret_29)
        {
          dbg_enter(ctx, "SEMI", ctx->pos);
          if (ctx->tokens[ctx->pos].kind == PL0_TOK_SEMI) {
            expr_ret_29 = SUCC; // Not capturing SEMI.
            ctx->pos++;
          } else {
            expr_ret_29 = NULL;
          }

          if (expr_ret_29) dbg_accept(ctx, "SEMI"); else dbg_reject(ctx, "SEMI");
        }

        // ModExprList 5
        if (expr_ret_29)
        {
          // CodeExpr
          dbg_enter(ctx, "CodeExpr", ctx->pos);
          #define ret expr_ret_29
          ret = SUCC;

          add(rule, node(PROC, i, v));

          if (ret) dbg_accept(ctx, "CodeExpr"); else dbg_reject(ctx, "CodeExpr");
          #undef ret
        }

        // ModExprList end
        if (!expr_ret_29) rew(mod_29);
        expr_ret_28 = expr_ret_29 ? SUCC : NULL;
      }

      expr_ret_28 = SUCC;
      expr_ret_27 = expr_ret_28;
    }

    // ModExprList 2
    if (expr_ret_27)
    {
      pl0_astnode_t* expr_ret_32 = NULL;
      expr_ret_32 = pl0_parse_statement(ctx);
      expr_ret_27 = expr_ret_32;
      s = expr_ret_32;
    }

    // ModExprList 3
    if (expr_ret_27)
    {
      // CodeExpr
      dbg_enter(ctx, "CodeExpr", ctx->pos);
      #define ret expr_ret_27
      ret = SUCC;

      add(rule, s);

      if (ret) dbg_accept(ctx, "CodeExpr"); else dbg_reject(ctx, "CodeExpr");
      #undef ret
    }

    // ModExprList 4
    if (expr_ret_27)
    {
      dbg_enter(ctx, "SEMI", ctx->pos);
      if (ctx->tokens[ctx->pos].kind == PL0_TOK_SEMI) {
        expr_ret_27 = SUCC; // Not capturing SEMI.
        ctx->pos++;
      } else {
        expr_ret_27 = NULL;
      }

      if (expr_ret_27) dbg_accept(ctx, "SEMI"); else dbg_reject(ctx, "SEMI");
    }

    // ModExprList end
    if (!expr_ret_27) rew(mod_27);
    expr_ret_24 = expr_ret_27 ? SUCC : NULL;
  }

  // SlashExpr end
  if (!expr_ret_24) rew(slash_24);
  expr_ret_23 = expr_ret_24;

  if (expr_ret_23) dbg_accept(ctx, "block"); else dbg_reject(ctx, "block");
  return expr_ret_23 ? rule : NULL;
  #undef rule
}

static inline pl0_astnode_t* pl0_parse_statement(pl0_parser_ctx* ctx) {
  pl0_astnode_t* id = NULL;
  pl0_astnode_t* ceq = NULL;
  pl0_astnode_t* e = NULL;
  pl0_astnode_t* c = NULL;
  pl0_astnode_t* smt = NULL;
  pl0_astnode_t* expr_ret_34 = NULL;
  pl0_astnode_t* expr_ret_33 = NULL;
  #define rule expr_ret_33

  dbg_enter(ctx, "statement", ctx->pos);
  pl0_astnode_t* expr_ret_35 = NULL;

  rec(slash_35);

  // SlashExpr 0
  if (!expr_ret_35)
  {
    pl0_astnode_t* expr_ret_36 = NULL;
    rec(mod_36);
    // ModExprList 0
    {
      pl0_astnode_t* expr_ret_37 = NULL;
      dbg_enter(ctx, "IDENT", ctx->pos);
      if (ctx->tokens[ctx->pos].kind == PL0_TOK_IDENT) {
        // Capturing IDENT.
        expr_ret_37 = leaf(IDENT);
        ctx->pos++;
      } else {
        expr_ret_37 = NULL;
      }

      if (expr_ret_37) dbg_accept(ctx, "IDENT"); else dbg_reject(ctx, "IDENT");
      expr_ret_36 = expr_ret_37;
      id = expr_ret_37;
    }

    // ModExprList 1
    if (expr_ret_36)
    {
      pl0_astnode_t* expr_ret_38 = NULL;
      dbg_enter(ctx, "CEQ", ctx->pos);
      if (ctx->tokens[ctx->pos].kind == PL0_TOK_CEQ) {
        // Capturing CEQ.
        expr_ret_38 = leaf(CEQ);
        ctx->pos++;
      } else {
        expr_ret_38 = NULL;
      }

      if (expr_ret_38) dbg_accept(ctx, "CEQ"); else dbg_reject(ctx, "CEQ");
      expr_ret_36 = expr_ret_38;
      ceq = expr_ret_38;
    }

    // ModExprList 2
    if (expr_ret_36)
    {
      pl0_astnode_t* expr_ret_39 = NULL;
      expr_ret_39 = pl0_parse_expression(ctx);
      expr_ret_36 = expr_ret_39;
      e = expr_ret_39;
    }

    // ModExprList 3
    if (expr_ret_36)
    {
      // CodeExpr
      dbg_enter(ctx, "CodeExpr", ctx->pos);
      #define ret expr_ret_36
      ret = SUCC;

      rule=node(IDENT, id, e);

      if (ret) dbg_accept(ctx, "CodeExpr"); else dbg_reject(ctx, "CodeExpr");
      #undef ret
    }

    // ModExprList end
    if (!expr_ret_36) rew(mod_36);
    expr_ret_35 = expr_ret_36 ? SUCC : NULL;
  }

  // SlashExpr 1
  if (!expr_ret_35)
  {
    pl0_astnode_t* expr_ret_40 = NULL;
    rec(mod_40);
    // ModExprList 0
    {
      pl0_astnode_t* expr_ret_41 = NULL;
      dbg_enter(ctx, "CALL", ctx->pos);
      if (ctx->tokens[ctx->pos].kind == PL0_TOK_CALL) {
        // Capturing CALL.
        expr_ret_41 = leaf(CALL);
        ctx->pos++;
      } else {
        expr_ret_41 = NULL;
      }

      if (expr_ret_41) dbg_accept(ctx, "CALL"); else dbg_reject(ctx, "CALL");
      expr_ret_40 = expr_ret_41;
      c = expr_ret_41;
    }

    // ModExprList 1
    if (expr_ret_40)
    {
      pl0_astnode_t* expr_ret_42 = NULL;
      dbg_enter(ctx, "IDENT", ctx->pos);
      if (ctx->tokens[ctx->pos].kind == PL0_TOK_IDENT) {
        // Capturing IDENT.
        expr_ret_42 = leaf(IDENT);
        ctx->pos++;
      } else {
        expr_ret_42 = NULL;
      }

      if (expr_ret_42) dbg_accept(ctx, "IDENT"); else dbg_reject(ctx, "IDENT");
      expr_ret_40 = expr_ret_42;
      id = expr_ret_42;
    }

    // ModExprList 2
    if (expr_ret_40)
    {
      // CodeExpr
      dbg_enter(ctx, "CodeExpr", ctx->pos);
      #define ret expr_ret_40
      ret = SUCC;

      rule=node(CALL, id);

      if (ret) dbg_accept(ctx, "CodeExpr"); else dbg_reject(ctx, "CodeExpr");
      #undef ret
    }

    // ModExprList end
    if (!expr_ret_40) rew(mod_40);
    expr_ret_35 = expr_ret_40 ? SUCC : NULL;
  }

  // SlashExpr 2
  if (!expr_ret_35)
  {
    pl0_astnode_t* expr_ret_43 = NULL;
    rec(mod_43);
    // ModExprList 0
    {
      // CodeExpr
      dbg_enter(ctx, "CodeExpr", ctx->pos);
      #define ret expr_ret_43
      ret = SUCC;

      rule=list(BEGIN);

      if (ret) dbg_accept(ctx, "CodeExpr"); else dbg_reject(ctx, "CodeExpr");
      #undef ret
    }

    // ModExprList 1
    if (expr_ret_43)
    {
      dbg_enter(ctx, "BEGIN", ctx->pos);
      if (ctx->tokens[ctx->pos].kind == PL0_TOK_BEGIN) {
        expr_ret_43 = SUCC; // Not capturing BEGIN.
        ctx->pos++;
      } else {
        expr_ret_43 = NULL;
      }

      if (expr_ret_43) dbg_accept(ctx, "BEGIN"); else dbg_reject(ctx, "BEGIN");
    }

    // ModExprList 2
    if (expr_ret_43)
    {
      pl0_astnode_t* expr_ret_44 = NULL;
      expr_ret_44 = pl0_parse_statement(ctx);
      expr_ret_43 = expr_ret_44;
      smt = expr_ret_44;
    }

    // ModExprList 3
    if (expr_ret_43)
    {
      // CodeExpr
      dbg_enter(ctx, "CodeExpr", ctx->pos);
      #define ret expr_ret_43
      ret = SUCC;

      add(rule, node(STATEMENT, smt));

      if (ret) dbg_accept(ctx, "CodeExpr"); else dbg_reject(ctx, "CodeExpr");
      #undef ret
    }

    // ModExprList 4
    if (expr_ret_43)
    {
      pl0_astnode_t* expr_ret_45 = NULL;
      expr_ret_45 = SUCC;
      while (expr_ret_45)
      {
        pl0_astnode_t* expr_ret_46 = NULL;
        rec(mod_46);
        // ModExprList 0
        {
          dbg_enter(ctx, "SEMI", ctx->pos);
          if (ctx->tokens[ctx->pos].kind == PL0_TOK_SEMI) {
            expr_ret_46 = SUCC; // Not capturing SEMI.
            ctx->pos++;
          } else {
            expr_ret_46 = NULL;
          }

          if (expr_ret_46) dbg_accept(ctx, "SEMI"); else dbg_reject(ctx, "SEMI");
        }

        // ModExprList 1
        if (expr_ret_46)
        {
          pl0_astnode_t* expr_ret_47 = NULL;
          expr_ret_47 = pl0_parse_statement(ctx);
          expr_ret_46 = expr_ret_47;
          smt = expr_ret_47;
        }

        // ModExprList 2
        if (expr_ret_46)
        {
          // CodeExpr
          dbg_enter(ctx, "CodeExpr", ctx->pos);
          #define ret expr_ret_46
          ret = SUCC;

          add(rule, node(STATEMENT, smt));

          if (ret) dbg_accept(ctx, "CodeExpr"); else dbg_reject(ctx, "CodeExpr");
          #undef ret
        }

        // ModExprList end
        if (!expr_ret_46) rew(mod_46);
        expr_ret_45 = expr_ret_46 ? SUCC : NULL;
      }

      expr_ret_45 = SUCC;
      expr_ret_43 = expr_ret_45;
    }

    // ModExprList 5
    if (expr_ret_43)
    {
      dbg_enter(ctx, "END", ctx->pos);
      if (ctx->tokens[ctx->pos].kind == PL0_TOK_END) {
        expr_ret_43 = SUCC; // Not capturing END.
        ctx->pos++;
      } else {
        expr_ret_43 = NULL;
      }

      if (expr_ret_43) dbg_accept(ctx, "END"); else dbg_reject(ctx, "END");
    }

    // ModExprList end
    if (!expr_ret_43) rew(mod_43);
    expr_ret_35 = expr_ret_43 ? SUCC : NULL;
  }

  // SlashExpr 3
  if (!expr_ret_35)
  {
    pl0_astnode_t* expr_ret_48 = NULL;
    rec(mod_48);
    // ModExprList 0
    {
      dbg_enter(ctx, "IF", ctx->pos);
      if (ctx->tokens[ctx->pos].kind == PL0_TOK_IF) {
        expr_ret_48 = SUCC; // Not capturing IF.
        ctx->pos++;
      } else {
        expr_ret_48 = NULL;
      }

      if (expr_ret_48) dbg_accept(ctx, "IF"); else dbg_reject(ctx, "IF");
    }

    // ModExprList 1
    if (expr_ret_48)
    {
      pl0_astnode_t* expr_ret_49 = NULL;
      expr_ret_49 = pl0_parse_condition(ctx);
      expr_ret_48 = expr_ret_49;
      c = expr_ret_49;
    }

    // ModExprList 2
    if (expr_ret_48)
    {
      dbg_enter(ctx, "THEN", ctx->pos);
      if (ctx->tokens[ctx->pos].kind == PL0_TOK_THEN) {
        expr_ret_48 = SUCC; // Not capturing THEN.
        ctx->pos++;
      } else {
        expr_ret_48 = NULL;
      }

      if (expr_ret_48) dbg_accept(ctx, "THEN"); else dbg_reject(ctx, "THEN");
    }

    // ModExprList 3
    if (expr_ret_48)
    {
      pl0_astnode_t* expr_ret_50 = NULL;
      expr_ret_50 = pl0_parse_statement(ctx);
      expr_ret_48 = expr_ret_50;
      smt = expr_ret_50;
    }

    // ModExprList 4
    if (expr_ret_48)
    {
      // CodeExpr
      dbg_enter(ctx, "CodeExpr", ctx->pos);
      #define ret expr_ret_48
      ret = SUCC;

      rule=node(IF, c, smt);

      if (ret) dbg_accept(ctx, "CodeExpr"); else dbg_reject(ctx, "CodeExpr");
      #undef ret
    }

    // ModExprList end
    if (!expr_ret_48) rew(mod_48);
    expr_ret_35 = expr_ret_48 ? SUCC : NULL;
  }

  // SlashExpr 4
  if (!expr_ret_35)
  {
    pl0_astnode_t* expr_ret_51 = NULL;
    rec(mod_51);
    // ModExprList 0
    {
      dbg_enter(ctx, "WHILE", ctx->pos);
      if (ctx->tokens[ctx->pos].kind == PL0_TOK_WHILE) {
        expr_ret_51 = SUCC; // Not capturing WHILE.
        ctx->pos++;
      } else {
        expr_ret_51 = NULL;
      }

      if (expr_ret_51) dbg_accept(ctx, "WHILE"); else dbg_reject(ctx, "WHILE");
    }

    // ModExprList 1
    if (expr_ret_51)
    {
      pl0_astnode_t* expr_ret_52 = NULL;
      expr_ret_52 = pl0_parse_condition(ctx);
      expr_ret_51 = expr_ret_52;
      c = expr_ret_52;
    }

    // ModExprList 2
    if (expr_ret_51)
    {
      dbg_enter(ctx, "DO", ctx->pos);
      if (ctx->tokens[ctx->pos].kind == PL0_TOK_DO) {
        expr_ret_51 = SUCC; // Not capturing DO.
        ctx->pos++;
      } else {
        expr_ret_51 = NULL;
      }

      if (expr_ret_51) dbg_accept(ctx, "DO"); else dbg_reject(ctx, "DO");
    }

    // ModExprList 3
    if (expr_ret_51)
    {
      pl0_astnode_t* expr_ret_53 = NULL;
      expr_ret_53 = pl0_parse_statement(ctx);
      expr_ret_51 = expr_ret_53;
      smt = expr_ret_53;
    }

    // ModExprList 4
    if (expr_ret_51)
    {
      // CodeExpr
      dbg_enter(ctx, "CodeExpr", ctx->pos);
      #define ret expr_ret_51
      ret = SUCC;

      rule=node(WHILE, c, smt);

      if (ret) dbg_accept(ctx, "CodeExpr"); else dbg_reject(ctx, "CodeExpr");
      #undef ret
    }

    // ModExprList end
    if (!expr_ret_51) rew(mod_51);
    expr_ret_35 = expr_ret_51 ? SUCC : NULL;
  }

  // SlashExpr end
  if (!expr_ret_35) rew(slash_35);
  expr_ret_34 = expr_ret_35;

  if (expr_ret_34) dbg_accept(ctx, "statement"); else dbg_reject(ctx, "statement");
  return expr_ret_34 ? rule : NULL;
  #undef rule
}

static inline pl0_astnode_t* pl0_parse_condition(pl0_parser_ctx* ctx) {
  pl0_astnode_t* ex = NULL;
  pl0_astnode_t* op = NULL;
  pl0_astnode_t* ex_ = NULL;
  pl0_astnode_t* expr_ret_55 = NULL;
  pl0_astnode_t* expr_ret_54 = NULL;
  #define rule expr_ret_54

  dbg_enter(ctx, "condition", ctx->pos);
  pl0_astnode_t* expr_ret_56 = NULL;

  rec(slash_56);

  // SlashExpr 0
  if (!expr_ret_56)
  {
    pl0_astnode_t* expr_ret_57 = NULL;
    rec(mod_57);
    // ModExprList 0
    {
      dbg_enter(ctx, "ODD", ctx->pos);
      if (ctx->tokens[ctx->pos].kind == PL0_TOK_ODD) {
        expr_ret_57 = SUCC; // Not capturing ODD.
        ctx->pos++;
      } else {
        expr_ret_57 = NULL;
      }

      if (expr_ret_57) dbg_accept(ctx, "ODD"); else dbg_reject(ctx, "ODD");
    }

    // ModExprList 1
    if (expr_ret_57)
    {
      pl0_astnode_t* expr_ret_58 = NULL;
      expr_ret_58 = pl0_parse_expression(ctx);
      expr_ret_57 = expr_ret_58;
      ex = expr_ret_58;
    }

    // ModExprList 2
    if (expr_ret_57)
    {
      // CodeExpr
      dbg_enter(ctx, "CodeExpr", ctx->pos);
      #define ret expr_ret_57
      ret = SUCC;

      rule = node(UNEXPR, ex);;

      if (ret) dbg_accept(ctx, "CodeExpr"); else dbg_reject(ctx, "CodeExpr");
      #undef ret
    }

    // ModExprList end
    if (!expr_ret_57) rew(mod_57);
    expr_ret_56 = expr_ret_57 ? SUCC : NULL;
  }

  // SlashExpr 1
  if (!expr_ret_56)
  {
    pl0_astnode_t* expr_ret_59 = NULL;
    rec(mod_59);
    // ModExprList 0
    {
      pl0_astnode_t* expr_ret_60 = NULL;
      expr_ret_60 = pl0_parse_expression(ctx);
      expr_ret_59 = expr_ret_60;
      ex = expr_ret_60;
    }

    // ModExprList 1
    if (expr_ret_59)
    {
      pl0_astnode_t* expr_ret_61 = NULL;
      pl0_astnode_t* expr_ret_62 = NULL;

      rec(slash_62);

      // SlashExpr 0
      if (!expr_ret_62)
      {
        pl0_astnode_t* expr_ret_63 = NULL;
        rec(mod_63);
        // ModExprList Forwarding
        dbg_enter(ctx, "EQ", ctx->pos);
        if (ctx->tokens[ctx->pos].kind == PL0_TOK_EQ) {
          // Capturing EQ.
          expr_ret_63 = leaf(EQ);
          ctx->pos++;
        } else {
          expr_ret_63 = NULL;
        }

        if (expr_ret_63) dbg_accept(ctx, "EQ"); else dbg_reject(ctx, "EQ");
        // ModExprList end
        if (!expr_ret_63) rew(mod_63);
        expr_ret_62 = expr_ret_63;
      }

      // SlashExpr 1
      if (!expr_ret_62)
      {
        pl0_astnode_t* expr_ret_64 = NULL;
        rec(mod_64);
        // ModExprList Forwarding
        dbg_enter(ctx, "HASH", ctx->pos);
        if (ctx->tokens[ctx->pos].kind == PL0_TOK_HASH) {
          // Capturing HASH.
          expr_ret_64 = leaf(HASH);
          ctx->pos++;
        } else {
          expr_ret_64 = NULL;
        }

        if (expr_ret_64) dbg_accept(ctx, "HASH"); else dbg_reject(ctx, "HASH");
        // ModExprList end
        if (!expr_ret_64) rew(mod_64);
        expr_ret_62 = expr_ret_64;
      }

      // SlashExpr 2
      if (!expr_ret_62)
      {
        pl0_astnode_t* expr_ret_65 = NULL;
        rec(mod_65);
        // ModExprList Forwarding
        dbg_enter(ctx, "LT", ctx->pos);
        if (ctx->tokens[ctx->pos].kind == PL0_TOK_LT) {
          // Capturing LT.
          expr_ret_65 = leaf(LT);
          ctx->pos++;
        } else {
          expr_ret_65 = NULL;
        }

        if (expr_ret_65) dbg_accept(ctx, "LT"); else dbg_reject(ctx, "LT");
        // ModExprList end
        if (!expr_ret_65) rew(mod_65);
        expr_ret_62 = expr_ret_65;
      }

      // SlashExpr 3
      if (!expr_ret_62)
      {
        pl0_astnode_t* expr_ret_66 = NULL;
        rec(mod_66);
        // ModExprList Forwarding
        dbg_enter(ctx, "LEQ", ctx->pos);
        if (ctx->tokens[ctx->pos].kind == PL0_TOK_LEQ) {
          // Capturing LEQ.
          expr_ret_66 = leaf(LEQ);
          ctx->pos++;
        } else {
          expr_ret_66 = NULL;
        }

        if (expr_ret_66) dbg_accept(ctx, "LEQ"); else dbg_reject(ctx, "LEQ");
        // ModExprList end
        if (!expr_ret_66) rew(mod_66);
        expr_ret_62 = expr_ret_66;
      }

      // SlashExpr 4
      if (!expr_ret_62)
      {
        pl0_astnode_t* expr_ret_67 = NULL;
        rec(mod_67);
        // ModExprList Forwarding
        dbg_enter(ctx, "GT", ctx->pos);
        if (ctx->tokens[ctx->pos].kind == PL0_TOK_GT) {
          // Capturing GT.
          expr_ret_67 = leaf(GT);
          ctx->pos++;
        } else {
          expr_ret_67 = NULL;
        }

        if (expr_ret_67) dbg_accept(ctx, "GT"); else dbg_reject(ctx, "GT");
        // ModExprList end
        if (!expr_ret_67) rew(mod_67);
        expr_ret_62 = expr_ret_67;
      }

      // SlashExpr 5
      if (!expr_ret_62)
      {
        pl0_astnode_t* expr_ret_68 = NULL;
        rec(mod_68);
        // ModExprList Forwarding
        dbg_enter(ctx, "GEQ", ctx->pos);
        if (ctx->tokens[ctx->pos].kind == PL0_TOK_GEQ) {
          // Capturing GEQ.
          expr_ret_68 = leaf(GEQ);
          ctx->pos++;
        } else {
          expr_ret_68 = NULL;
        }

        if (expr_ret_68) dbg_accept(ctx, "GEQ"); else dbg_reject(ctx, "GEQ");
        // ModExprList end
        if (!expr_ret_68) rew(mod_68);
        expr_ret_62 = expr_ret_68;
      }

      // SlashExpr end
      if (!expr_ret_62) rew(slash_62);
      expr_ret_61 = expr_ret_62;

      expr_ret_59 = expr_ret_61;
      op = expr_ret_61;
    }

    // ModExprList 2
    if (expr_ret_59)
    {
      pl0_astnode_t* expr_ret_69 = NULL;
      expr_ret_69 = pl0_parse_expression(ctx);
      expr_ret_59 = expr_ret_69;
      ex_ = expr_ret_69;
    }

    // ModExprList 3
    if (expr_ret_59)
    {
      // CodeExpr
      dbg_enter(ctx, "CodeExpr", ctx->pos);
      #define ret expr_ret_59
      ret = SUCC;

      rule=node(BINEXPR, op, ex, ex_);

      if (ret) dbg_accept(ctx, "CodeExpr"); else dbg_reject(ctx, "CodeExpr");
      #undef ret
    }

    // ModExprList end
    if (!expr_ret_59) rew(mod_59);
    expr_ret_56 = expr_ret_59 ? SUCC : NULL;
  }

  // SlashExpr end
  if (!expr_ret_56) rew(slash_56);
  expr_ret_55 = expr_ret_56;

  if (expr_ret_55) dbg_accept(ctx, "condition"); else dbg_reject(ctx, "condition");
  return expr_ret_55 ? rule : NULL;
  #undef rule
}

static inline pl0_astnode_t* pl0_parse_expression(pl0_parser_ctx* ctx) {
  pl0_astnode_t* pm = NULL;
  pl0_astnode_t* t = NULL;
  pl0_astnode_t* expr_ret_71 = NULL;
  pl0_astnode_t* expr_ret_70 = NULL;
  #define rule expr_ret_70

  dbg_enter(ctx, "expression", ctx->pos);
  pl0_astnode_t* expr_ret_72 = NULL;
  rec(mod_72);
  // ModExprList 0
  {
    // CodeExpr
    dbg_enter(ctx, "CodeExpr", ctx->pos);
    #define ret expr_ret_72
    ret = SUCC;

    rule=list(EXPRS);

    if (ret) dbg_accept(ctx, "CodeExpr"); else dbg_reject(ctx, "CodeExpr");
    #undef ret
  }

  // ModExprList 1
  if (expr_ret_72)
  {
    pl0_astnode_t* expr_ret_73 = NULL;
    pl0_astnode_t* expr_ret_74 = NULL;

    rec(slash_74);

    // SlashExpr 0
    if (!expr_ret_74)
    {
      pl0_astnode_t* expr_ret_75 = NULL;
      rec(mod_75);
      // ModExprList Forwarding
      dbg_enter(ctx, "PLUS", ctx->pos);
      if (ctx->tokens[ctx->pos].kind == PL0_TOK_PLUS) {
        // Capturing PLUS.
        expr_ret_75 = leaf(PLUS);
        ctx->pos++;
      } else {
        expr_ret_75 = NULL;
      }

      if (expr_ret_75) dbg_accept(ctx, "PLUS"); else dbg_reject(ctx, "PLUS");
      // ModExprList end
      if (!expr_ret_75) rew(mod_75);
      expr_ret_74 = expr_ret_75;
    }

    // SlashExpr 1
    if (!expr_ret_74)
    {
      pl0_astnode_t* expr_ret_76 = NULL;
      rec(mod_76);
      // ModExprList Forwarding
      dbg_enter(ctx, "MINUS", ctx->pos);
      if (ctx->tokens[ctx->pos].kind == PL0_TOK_MINUS) {
        // Capturing MINUS.
        expr_ret_76 = leaf(MINUS);
        ctx->pos++;
      } else {
        expr_ret_76 = NULL;
      }

      if (expr_ret_76) dbg_accept(ctx, "MINUS"); else dbg_reject(ctx, "MINUS");
      // ModExprList end
      if (!expr_ret_76) rew(mod_76);
      expr_ret_74 = expr_ret_76;
    }

    // SlashExpr end
    if (!expr_ret_74) rew(slash_74);
    expr_ret_73 = expr_ret_74;

    // optional
    if (!expr_ret_73)
      expr_ret_73 = SUCC;
    expr_ret_72 = expr_ret_73;
    pm = expr_ret_73;
  }

  // ModExprList 2
  if (expr_ret_72)
  {
    pl0_astnode_t* expr_ret_77 = NULL;
    expr_ret_77 = pl0_parse_term(ctx);
    expr_ret_72 = expr_ret_77;
    t = expr_ret_77;
  }

  // ModExprList 3
  if (expr_ret_72)
  {
    // CodeExpr
    dbg_enter(ctx, "CodeExpr", ctx->pos);
    #define ret expr_ret_72
    ret = SUCC;

    add(rule, pm==SUCC ? t : node(UNEXPR, pm, t));

    if (ret) dbg_accept(ctx, "CodeExpr"); else dbg_reject(ctx, "CodeExpr");
    #undef ret
  }

  // ModExprList 4
  if (expr_ret_72)
  {
    pl0_astnode_t* expr_ret_78 = NULL;
    expr_ret_78 = SUCC;
    while (expr_ret_78)
    {
      pl0_astnode_t* expr_ret_79 = NULL;
      rec(mod_79);
      // ModExprList 0
      {
        pl0_astnode_t* expr_ret_80 = NULL;
        pl0_astnode_t* expr_ret_81 = NULL;

        rec(slash_81);

        // SlashExpr 0
        if (!expr_ret_81)
        {
          pl0_astnode_t* expr_ret_82 = NULL;
          rec(mod_82);
          // ModExprList Forwarding
          dbg_enter(ctx, "PLUS", ctx->pos);
          if (ctx->tokens[ctx->pos].kind == PL0_TOK_PLUS) {
            // Capturing PLUS.
            expr_ret_82 = leaf(PLUS);
            ctx->pos++;
          } else {
            expr_ret_82 = NULL;
          }

          if (expr_ret_82) dbg_accept(ctx, "PLUS"); else dbg_reject(ctx, "PLUS");
          // ModExprList end
          if (!expr_ret_82) rew(mod_82);
          expr_ret_81 = expr_ret_82;
        }

        // SlashExpr 1
        if (!expr_ret_81)
        {
          pl0_astnode_t* expr_ret_83 = NULL;
          rec(mod_83);
          // ModExprList Forwarding
          dbg_enter(ctx, "MINUS", ctx->pos);
          if (ctx->tokens[ctx->pos].kind == PL0_TOK_MINUS) {
            // Capturing MINUS.
            expr_ret_83 = leaf(MINUS);
            ctx->pos++;
          } else {
            expr_ret_83 = NULL;
          }

          if (expr_ret_83) dbg_accept(ctx, "MINUS"); else dbg_reject(ctx, "MINUS");
          // ModExprList end
          if (!expr_ret_83) rew(mod_83);
          expr_ret_81 = expr_ret_83;
        }

        // SlashExpr end
        if (!expr_ret_81) rew(slash_81);
        expr_ret_80 = expr_ret_81;

        expr_ret_79 = expr_ret_80;
        pm = expr_ret_80;
      }

      // ModExprList 1
      if (expr_ret_79)
      {
        pl0_astnode_t* expr_ret_84 = NULL;
        expr_ret_84 = pl0_parse_term(ctx);
        expr_ret_79 = expr_ret_84;
        t = expr_ret_84;
      }

      // ModExprList 2
      if (expr_ret_79)
      {
        // CodeExpr
        dbg_enter(ctx, "CodeExpr", ctx->pos);
        #define ret expr_ret_79
        ret = SUCC;

        add(rule, pm==SUCC ? t : node(BINEXPR, pm, t));

        if (ret) dbg_accept(ctx, "CodeExpr"); else dbg_reject(ctx, "CodeExpr");
        #undef ret
      }

      // ModExprList end
      if (!expr_ret_79) rew(mod_79);
      expr_ret_78 = expr_ret_79 ? SUCC : NULL;
    }

    expr_ret_78 = SUCC;
    expr_ret_72 = expr_ret_78;
  }

  // ModExprList end
  if (!expr_ret_72) rew(mod_72);
  expr_ret_71 = expr_ret_72 ? SUCC : NULL;
  if (expr_ret_71) dbg_accept(ctx, "expression"); else dbg_reject(ctx, "expression");
  return expr_ret_71 ? rule : NULL;
  #undef rule
}

static inline pl0_astnode_t* pl0_parse_term(pl0_parser_ctx* ctx) {
  pl0_astnode_t* f = NULL;
  pl0_astnode_t* sd = NULL;
  pl0_astnode_t* expr_ret_86 = NULL;
  pl0_astnode_t* expr_ret_85 = NULL;
  #define rule expr_ret_85

  dbg_enter(ctx, "term", ctx->pos);
  pl0_astnode_t* expr_ret_87 = NULL;
  rec(mod_87);
  // ModExprList 0
  {
    // CodeExpr
    dbg_enter(ctx, "CodeExpr", ctx->pos);
    #define ret expr_ret_87
    ret = SUCC;

    rule=list(EXPRS);

    if (ret) dbg_accept(ctx, "CodeExpr"); else dbg_reject(ctx, "CodeExpr");
    #undef ret
  }

  // ModExprList 1
  if (expr_ret_87)
  {
    pl0_astnode_t* expr_ret_88 = NULL;
    expr_ret_88 = pl0_parse_factor(ctx);
    expr_ret_87 = expr_ret_88;
    f = expr_ret_88;
  }

  // ModExprList 2
  if (expr_ret_87)
  {
    // CodeExpr
    dbg_enter(ctx, "CodeExpr", ctx->pos);
    #define ret expr_ret_87
    ret = SUCC;

    add(rule, f);

    if (ret) dbg_accept(ctx, "CodeExpr"); else dbg_reject(ctx, "CodeExpr");
    #undef ret
  }

  // ModExprList 3
  if (expr_ret_87)
  {
    pl0_astnode_t* expr_ret_89 = NULL;
    expr_ret_89 = SUCC;
    while (expr_ret_89)
    {
      pl0_astnode_t* expr_ret_90 = NULL;
      rec(mod_90);
      // ModExprList 0
      {
        pl0_astnode_t* expr_ret_91 = NULL;
        pl0_astnode_t* expr_ret_92 = NULL;

        rec(slash_92);

        // SlashExpr 0
        if (!expr_ret_92)
        {
          pl0_astnode_t* expr_ret_93 = NULL;
          rec(mod_93);
          // ModExprList Forwarding
          dbg_enter(ctx, "STAR", ctx->pos);
          if (ctx->tokens[ctx->pos].kind == PL0_TOK_STAR) {
            // Capturing STAR.
            expr_ret_93 = leaf(STAR);
            ctx->pos++;
          } else {
            expr_ret_93 = NULL;
          }

          if (expr_ret_93) dbg_accept(ctx, "STAR"); else dbg_reject(ctx, "STAR");
          // ModExprList end
          if (!expr_ret_93) rew(mod_93);
          expr_ret_92 = expr_ret_93;
        }

        // SlashExpr 1
        if (!expr_ret_92)
        {
          pl0_astnode_t* expr_ret_94 = NULL;
          rec(mod_94);
          // ModExprList Forwarding
          dbg_enter(ctx, "DIV", ctx->pos);
          if (ctx->tokens[ctx->pos].kind == PL0_TOK_DIV) {
            // Capturing DIV.
            expr_ret_94 = leaf(DIV);
            ctx->pos++;
          } else {
            expr_ret_94 = NULL;
          }

          if (expr_ret_94) dbg_accept(ctx, "DIV"); else dbg_reject(ctx, "DIV");
          // ModExprList end
          if (!expr_ret_94) rew(mod_94);
          expr_ret_92 = expr_ret_94;
        }

        // SlashExpr end
        if (!expr_ret_92) rew(slash_92);
        expr_ret_91 = expr_ret_92;

        expr_ret_90 = expr_ret_91;
        sd = expr_ret_91;
      }

      // ModExprList 1
      if (expr_ret_90)
      {
        pl0_astnode_t* expr_ret_95 = NULL;
        expr_ret_95 = pl0_parse_factor(ctx);
        expr_ret_90 = expr_ret_95;
        f = expr_ret_95;
      }

      // ModExprList 2
      if (expr_ret_90)
      {
        // CodeExpr
        dbg_enter(ctx, "CodeExpr", ctx->pos);
        #define ret expr_ret_90
        ret = SUCC;

        add(rule, node(STAR, sd, f));

        if (ret) dbg_accept(ctx, "CodeExpr"); else dbg_reject(ctx, "CodeExpr");
        #undef ret
      }

      // ModExprList end
      if (!expr_ret_90) rew(mod_90);
      expr_ret_89 = expr_ret_90 ? SUCC : NULL;
    }

    expr_ret_89 = SUCC;
    expr_ret_87 = expr_ret_89;
  }

  // ModExprList end
  if (!expr_ret_87) rew(mod_87);
  expr_ret_86 = expr_ret_87 ? SUCC : NULL;
  if (expr_ret_86) dbg_accept(ctx, "term"); else dbg_reject(ctx, "term");
  return expr_ret_86 ? rule : NULL;
  #undef rule
}

static inline pl0_astnode_t* pl0_parse_factor(pl0_parser_ctx* ctx) {
  pl0_astnode_t* i = NULL;
  pl0_astnode_t* n = NULL;
  pl0_astnode_t* e = NULL;
  pl0_astnode_t* expr_ret_97 = NULL;
  pl0_astnode_t* expr_ret_96 = NULL;
  #define rule expr_ret_96

  dbg_enter(ctx, "factor", ctx->pos);
  pl0_astnode_t* expr_ret_98 = NULL;

  rec(slash_98);

  // SlashExpr 0
  if (!expr_ret_98)
  {
    pl0_astnode_t* expr_ret_99 = NULL;
    rec(mod_99);
    // ModExprList 0
    {
      pl0_astnode_t* expr_ret_100 = NULL;
      dbg_enter(ctx, "IDENT", ctx->pos);
      if (ctx->tokens[ctx->pos].kind == PL0_TOK_IDENT) {
        // Capturing IDENT.
        expr_ret_100 = leaf(IDENT);
        ctx->pos++;
      } else {
        expr_ret_100 = NULL;
      }

      if (expr_ret_100) dbg_accept(ctx, "IDENT"); else dbg_reject(ctx, "IDENT");
      expr_ret_99 = expr_ret_100;
      i = expr_ret_100;
    }

    // ModExprList 1
    if (expr_ret_99)
    {
      // CodeExpr
      dbg_enter(ctx, "CodeExpr", ctx->pos);
      #define ret expr_ret_99
      ret = SUCC;

      rule=node(IDENT, i);

      if (ret) dbg_accept(ctx, "CodeExpr"); else dbg_reject(ctx, "CodeExpr");
      #undef ret
    }

    // ModExprList end
    if (!expr_ret_99) rew(mod_99);
    expr_ret_98 = expr_ret_99 ? SUCC : NULL;
  }

  // SlashExpr 1
  if (!expr_ret_98)
  {
    pl0_astnode_t* expr_ret_101 = NULL;
    rec(mod_101);
    // ModExprList 0
    {
      pl0_astnode_t* expr_ret_102 = NULL;
      dbg_enter(ctx, "NUM", ctx->pos);
      if (ctx->tokens[ctx->pos].kind == PL0_TOK_NUM) {
        // Capturing NUM.
        expr_ret_102 = leaf(NUM);
        ctx->pos++;
      } else {
        expr_ret_102 = NULL;
      }

      if (expr_ret_102) dbg_accept(ctx, "NUM"); else dbg_reject(ctx, "NUM");
      expr_ret_101 = expr_ret_102;
      n = expr_ret_102;
    }

    // ModExprList 1
    if (expr_ret_101)
    {
      // CodeExpr
      dbg_enter(ctx, "CodeExpr", ctx->pos);
      #define ret expr_ret_101
      ret = SUCC;

      rule=node(NUM, n);

      if (ret) dbg_accept(ctx, "CodeExpr"); else dbg_reject(ctx, "CodeExpr");
      #undef ret
    }

    // ModExprList end
    if (!expr_ret_101) rew(mod_101);
    expr_ret_98 = expr_ret_101 ? SUCC : NULL;
  }

  // SlashExpr 2
  if (!expr_ret_98)
  {
    pl0_astnode_t* expr_ret_103 = NULL;
    rec(mod_103);
    // ModExprList 0
    {
      dbg_enter(ctx, "OPEN", ctx->pos);
      if (ctx->tokens[ctx->pos].kind == PL0_TOK_OPEN) {
        expr_ret_103 = SUCC; // Not capturing OPEN.
        ctx->pos++;
      } else {
        expr_ret_103 = NULL;
      }

      if (expr_ret_103) dbg_accept(ctx, "OPEN"); else dbg_reject(ctx, "OPEN");
    }

    // ModExprList 1
    if (expr_ret_103)
    {
      pl0_astnode_t* expr_ret_104 = NULL;
      expr_ret_104 = pl0_parse_expression(ctx);
      expr_ret_103 = expr_ret_104;
      e = expr_ret_104;
    }

    // ModExprList 2
    if (expr_ret_103)
    {
      dbg_enter(ctx, "CLOSE", ctx->pos);
      if (ctx->tokens[ctx->pos].kind == PL0_TOK_CLOSE) {
        expr_ret_103 = SUCC; // Not capturing CLOSE.
        ctx->pos++;
      } else {
        expr_ret_103 = NULL;
      }

      if (expr_ret_103) dbg_accept(ctx, "CLOSE"); else dbg_reject(ctx, "CLOSE");
    }

    // ModExprList 3
    if (expr_ret_103)
    {
      // CodeExpr
      dbg_enter(ctx, "CodeExpr", ctx->pos);
      #define ret expr_ret_103
      ret = SUCC;

      rule=e;

      if (ret) dbg_accept(ctx, "CodeExpr"); else dbg_reject(ctx, "CodeExpr");
      #undef ret
    }

    // ModExprList end
    if (!expr_ret_103) rew(mod_103);
    expr_ret_98 = expr_ret_103 ? SUCC : NULL;
  }

  // SlashExpr end
  if (!expr_ret_98) rew(slash_98);
  expr_ret_97 = expr_ret_98;

  if (expr_ret_97) dbg_accept(ctx, "factor"); else dbg_reject(ctx, "factor");
  return expr_ret_97 ? rule : NULL;
  #undef rule
}



#endif /* PGEN_PL0_ASTNODE_INCLUDE */

