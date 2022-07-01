
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
#include <stdlib.h>

#define PGEN_ALIGNMENT _Alignof(max_align_t)
#define PGEN_BUFFER_SIZE (PGEN_PAGESIZE * 1024)
#define NUM_ARENAS 256
#define NUM_FREELIST 256

#ifndef PGEN_PAGESIZE
#define PGEN_PAGESIZE 4096
#endif

#ifndef PGEN_OOM
#include <stdio.h>
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

#define PGEN_REWIND_START ((pgen_allocator_rewind_t){0, 0})

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

  for (size_t i = 0; i < allocator->freelist.len; i++) {
    void *ptr = allocator->freelist.entries[i].ptr;
    if (ptr == old_ptr) {
      allocator->freelist.entries[i].ptr = new_ptr;
      allocator->freelist.entries[i].freefn = new_free_fn;
      allocator->freelist.entries[i].rew = new_rew;
      return;
    }
  }
}

static inline void pgen_defer(pgen_allocator *allocator, void (*freefn)(void *),
                              void *ptr, pgen_allocator_rewind_t rew) {
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
}

static inline void pgen_allocator_rewind(pgen_allocator *allocator,
                                         pgen_allocator_rewind_t rew) {
  allocator->rew = rew;
  // Free all the objects associated with nodes implicitly destroyed.
  size_t i = allocator->freelist.len;
  while (1) {
    i--;

    pgen_freelist_entry_t entry = allocator->freelist.entries[i];
    uint32_t arena_idx = entry.rew.arena_idx;
    uint32_t filled = entry.rew.filled;
    if ((i == SIZE_MAX) |             // Relies on unsigned wrapping
        (rew.arena_idx < arena_idx) | //
        (rew.filled < filled)) {
      break;
    }

    entry.freefn(entry.ptr);
  }
  allocator->freelist.len = i + 1;
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
static size_t pl0_num_tokens = 33;
static const char* pl0_kind_name[] = {
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

struct pl0_astnode_t;
typedef struct pl0_astnode_t pl0_astnode_t;
struct pl0_astnode_t {
  // No %extra directives.

  const char* kind;
  size_t num_children;
  size_t max_children;
  pl0_astnode_t** children;
  pgen_allocator_rewind_t rew;
};

static inline pl0_astnode_t* pl0_astnode_list(
                             pgen_allocator* alloc,
                             const char* kind,
                             size_t initial_size) {
  pgen_allocator_ret_t ret = pgen_alloc(alloc,
                                        sizeof(pl0_astnode_t),
                                        _Alignof(pl0_astnode_t));
  pl0_astnode_t *node = (pl0_astnode_t*)ret.buf;

  pl0_astnode_t *children;
  if (initial_size) {
    children = (pl0_astnode_t*)malloc(sizeof(pl0_astnode_t) * initial_size);
    if (!children)
      PGEN_OOM();
    pgen_defer(alloc, free, children, ret.rew);
  } else {
    children = NULL;
  }

  node->kind = kind;
  node->max_children = initial_size;
  node->num_children = 0;
  node->children = NULL;
  node->rew = ret.rew;
  return node;
}

static inline pl0_astnode_t* pl0_astnode_leaf(
                             pgen_allocator* alloc,
                             const char* kind) {
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
                             const char* kind,
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
                             const char* kind,
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
                             const char* kind,
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
                             const char* kind,
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
                             const char* kind,
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

static inline pl0_astnode_t* pl0_astnode_fixed_6(
                             pgen_allocator* alloc,
                             const char* kind,
                             pl0_astnode_t* n0,
                             pl0_astnode_t* n1,
                             pl0_astnode_t* n2,
                             pl0_astnode_t* n3,
                             pl0_astnode_t* n4,
                             pl0_astnode_t* n5) {
  pgen_allocator_ret_t ret = pgen_alloc(alloc,
                                        sizeof(pl0_astnode_t) +
                                        sizeof(pl0_astnode_t *) * 6,
                                        _Alignof(pl0_astnode_t));
  pl0_astnode_t *node = (pl0_astnode_t *)ret.buf;
  pl0_astnode_t **children = (pl0_astnode_t **)(node + 1);
  node->kind = kind;
  node->max_children = 0;
  node->num_children = 6;
  node->children = children;
  node->rew = ret.rew;
  children[0] = n0;
  children[1] = n1;
  children[2] = n2;
  children[3] = n3;
  children[4] = n4;
  children[5] = n5;
  return node;
}

static inline pl0_astnode_t* pl0_astnode_fixed_7(
                             pgen_allocator* alloc,
                             const char* kind,
                             pl0_astnode_t* n0,
                             pl0_astnode_t* n1,
                             pl0_astnode_t* n2,
                             pl0_astnode_t* n3,
                             pl0_astnode_t* n4,
                             pl0_astnode_t* n5,
                             pl0_astnode_t* n6) {
  pgen_allocator_ret_t ret = pgen_alloc(alloc,
                                        sizeof(pl0_astnode_t) +
                                        sizeof(pl0_astnode_t *) * 7,
                                        _Alignof(pl0_astnode_t));
  pl0_astnode_t *node = (pl0_astnode_t *)ret.buf;
  pl0_astnode_t **children = (pl0_astnode_t **)(node + 1);
  node->kind = kind;
  node->max_children = 0;
  node->num_children = 7;
  node->children = children;
  node->rew = ret.rew;
  children[0] = n0;
  children[1] = n1;
  children[2] = n2;
  children[3] = n3;
  children[4] = n4;
  children[5] = n5;
  children[6] = n6;
  return node;
}

static inline pl0_astnode_t* pl0_astnode_fixed_8(
                             pgen_allocator* alloc,
                             const char* kind,
                             pl0_astnode_t* n0,
                             pl0_astnode_t* n1,
                             pl0_astnode_t* n2,
                             pl0_astnode_t* n3,
                             pl0_astnode_t* n4,
                             pl0_astnode_t* n5,
                             pl0_astnode_t* n6,
                             pl0_astnode_t* n7) {
  pgen_allocator_ret_t ret = pgen_alloc(alloc,
                                        sizeof(pl0_astnode_t) +
                                        sizeof(pl0_astnode_t *) * 8,
                                        _Alignof(pl0_astnode_t));
  pl0_astnode_t *node = (pl0_astnode_t *)ret.buf;
  pl0_astnode_t **children = (pl0_astnode_t **)(node + 1);
  node->kind = kind;
  node->max_children = 0;
  node->num_children = 8;
  node->children = children;
  node->rew = ret.rew;
  children[0] = n0;
  children[1] = n1;
  children[2] = n2;
  children[3] = n3;
  children[4] = n4;
  children[5] = n5;
  children[6] = n6;
  children[7] = n7;
  return node;
}

static inline pl0_astnode_t* pl0_astnode_fixed_9(
                             pgen_allocator* alloc,
                             const char* kind,
                             pl0_astnode_t* n0,
                             pl0_astnode_t* n1,
                             pl0_astnode_t* n2,
                             pl0_astnode_t* n3,
                             pl0_astnode_t* n4,
                             pl0_astnode_t* n5,
                             pl0_astnode_t* n6,
                             pl0_astnode_t* n7,
                             pl0_astnode_t* n8) {
  pgen_allocator_ret_t ret = pgen_alloc(alloc,
                                        sizeof(pl0_astnode_t) +
                                        sizeof(pl0_astnode_t *) * 9,
                                        _Alignof(pl0_astnode_t));
  pl0_astnode_t *node = (pl0_astnode_t *)ret.buf;
  pl0_astnode_t **children = (pl0_astnode_t **)(node + 1);
  node->kind = kind;
  node->max_children = 0;
  node->num_children = 9;
  node->children = children;
  node->rew = ret.rew;
  children[0] = n0;
  children[1] = n1;
  children[2] = n2;
  children[3] = n3;
  children[4] = n4;
  children[5] = n5;
  children[6] = n6;
  children[7] = n7;
  children[8] = n8;
  return node;
}

static inline pl0_astnode_t* pl0_astnode_fixed_10(
                             pgen_allocator* alloc,
                             const char* kind,
                             pl0_astnode_t* n0,
                             pl0_astnode_t* n1,
                             pl0_astnode_t* n2,
                             pl0_astnode_t* n3,
                             pl0_astnode_t* n4,
                             pl0_astnode_t* n5,
                             pl0_astnode_t* n6,
                             pl0_astnode_t* n7,
                             pl0_astnode_t* n8,
                             pl0_astnode_t* n9) {
  pgen_allocator_ret_t ret = pgen_alloc(alloc,
                                        sizeof(pl0_astnode_t) +
                                        sizeof(pl0_astnode_t *) * 10,
                                        _Alignof(pl0_astnode_t));
  pl0_astnode_t *node = (pl0_astnode_t *)ret.buf;
  pl0_astnode_t **children = (pl0_astnode_t **)(node + 1);
  node->kind = kind;
  node->max_children = 0;
  node->num_children = 10;
  node->children = children;
  node->rew = ret.rew;
  children[0] = n0;
  children[1] = n1;
  children[2] = n2;
  children[3] = n3;
  children[4] = n4;
  children[5] = n5;
  children[6] = n6;
  children[7] = n7;
  children[8] = n8;
  children[9] = n9;
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
  
  list->children[list->num_children++] = node;
}

static inline void pl0_parser_rewind(pl0_parser_ctx *ctx, pgen_parser_rewind_t rew) {
  pgen_allocator_rewind(ctx->alloc, rew.arew);  ctx->pos = rew.prew;}

#define _LIN #__LINE__

#define node(kind, ...)          PGEN_CAT(pl0_astnode_fixed_, PGEN_NARG(__VA_ARGS__))(ctx->alloc, kind, __VA_ARGS__)
#define rec(label)               pgen_parser_rewind_t _rew_##label = {ctx->alloc->rew, ctx->pos};
#define rew(to)                  pl0_parser_rewind(ctx, to)
#define list(kind)               pl0_astnode_list(ctx->alloc, kind, 32)
#define leaf(kind)               pl0_astnode_leaf(ctx->alloc, kind)
#define add(to, node)            pl0_astnode_add(ctx->alloc, to, node)
#define defer(node, freefn, ptr) pgen_defer(ctx->alloc, freefn, ptr, node->rew)

static inline pl0_astnode_t* pl0_parse_program(pl0_parser_ctx* ctx);
static inline pl0_astnode_t* pl0_parse_block(pl0_parser_ctx* ctx);
static inline pl0_astnode_t* pl0_parse_statement(pl0_parser_ctx* ctx);
static inline pl0_astnode_t* pl0_parse_condition(pl0_parser_ctx* ctx);
static inline pl0_astnode_t* pl0_parse_expression(pl0_parser_ctx* ctx);
static inline pl0_astnode_t* pl0_parse_term(pl0_parser_ctx* ctx);
static inline pl0_astnode_t* pl0_parse_factor(pl0_parser_ctx* ctx);


static inline pl0_astnode_t* pl0_parse_program(pl0_parser_ctx* ctx) {
  pl0_astnode_t* label = NULL;
  #define rule expr_ret_0;
  pl0_astnode_t _succ;
  pl0_astnode_t* SUCC = &_succ;
  pl0_astnode_t* expr_ret_0 = NULL;

  pl0_astnode_t* expr_ret_1 = NULL;
  {
    pl0_astnode_t* expr_ret_2 = NULL;
    {
      // LowerIdent
      expr_ret_2 = pl0_parse_block(ctx);
    }

    expr_ret_1 = expr_ret_2;
    label = expr_ret_2;
  }

  if (expr_ret_1)
  {
    // UpperIdent
    if (ctx->tokens[ctx->pos].kind == PL0_TOK_DOT)
    {
      ctx->pos++;
      // Allocate?
    }

  }

  if (expr_ret_1)
  {
    // CodeExpr
    expr_ret_1 = SUCC;
    #define ret expr_ret_1

    {
      ret = label;
    }

    #undef ret
  }

  expr_ret_0 = expr_ret_1;
  return expr_ret_0;
  #undef rule
}
static inline pl0_astnode_t* pl0_parse_block(pl0_parser_ctx* ctx) {
  #define rule expr_ret_3;
  pl0_astnode_t _succ;
  pl0_astnode_t* SUCC = &_succ;
  pl0_astnode_t* expr_ret_3 = NULL;

  pl0_astnode_t* expr_ret_4 = NULL;

  if (!expr_ret_4)
  {
    pl0_astnode_t* expr_ret_5 = NULL;
    {
      // UpperIdent
      if (ctx->tokens[ctx->pos].kind == PL0_TOK_CONST)
      {
        ctx->pos++;
        // Allocate?
      }

    }

    if (expr_ret_5)
    {
      // UpperIdent
      if (ctx->tokens[ctx->pos].kind == PL0_TOK_IDENT)
      {
        ctx->pos++;
        // Allocate?
      }

    }

    if (expr_ret_5)
    {
      // UpperIdent
      if (ctx->tokens[ctx->pos].kind == PL0_TOK_EQ)
      {
        ctx->pos++;
        // Allocate?
      }

    }

    if (expr_ret_5)
    {
      // UpperIdent
      if (ctx->tokens[ctx->pos].kind == PL0_TOK_NUM)
      {
        ctx->pos++;
        // Allocate?
      }

    }

    if (expr_ret_5)
    {
      pl0_astnode_t* expr_ret_6 = NULL;
      expr_ret_6 = SUCC;
      {
        pl0_astnode_t* expr_ret_7 = NULL;
        {
          // UpperIdent
          if (ctx->tokens[ctx->pos].kind == PL0_TOK_COMMA)
          {
            ctx->pos++;
            // Allocate?
          }

        }

        if (expr_ret_7)
        {
          // UpperIdent
          if (ctx->tokens[ctx->pos].kind == PL0_TOK_IDENT)
          {
            ctx->pos++;
            // Allocate?
          }

        }

        if (expr_ret_7)
        {
          // UpperIdent
          if (ctx->tokens[ctx->pos].kind == PL0_TOK_EQ)
          {
            ctx->pos++;
            // Allocate?
          }

        }

        if (expr_ret_7)
        {
          // UpperIdent
          if (ctx->tokens[ctx->pos].kind == PL0_TOK_NUM)
          {
            ctx->pos++;
            // Allocate?
          }

        }

        expr_ret_6 = expr_ret_7;
      }

      expr_ret_5 = expr_ret_6;
    }

    if (expr_ret_5)
    {
      // UpperIdent
      if (ctx->tokens[ctx->pos].kind == PL0_TOK_SEMI)
      {
        ctx->pos++;
        // Allocate?
      }

    }

    expr_ret_4 = expr_ret_5;
  }

  if (!expr_ret_4)
  {
    pl0_astnode_t* expr_ret_8 = NULL;
    {
      // UpperIdent
      if (ctx->tokens[ctx->pos].kind == PL0_TOK_VAR)
      {
        ctx->pos++;
        // Allocate?
      }

    }

    if (expr_ret_8)
    {
      // UpperIdent
      if (ctx->tokens[ctx->pos].kind == PL0_TOK_IDENT)
      {
        ctx->pos++;
        // Allocate?
      }

    }

    if (expr_ret_8)
    {
      pl0_astnode_t* expr_ret_9 = NULL;
      expr_ret_9 = SUCC;
      {
        pl0_astnode_t* expr_ret_10 = NULL;
        {
          // UpperIdent
          if (ctx->tokens[ctx->pos].kind == PL0_TOK_COMMA)
          {
            ctx->pos++;
            // Allocate?
          }

        }

        if (expr_ret_10)
        {
          // UpperIdent
          if (ctx->tokens[ctx->pos].kind == PL0_TOK_IDENT)
          {
            ctx->pos++;
            // Allocate?
          }

        }

        expr_ret_9 = expr_ret_10;
      }

      expr_ret_8 = expr_ret_9;
    }

    if (expr_ret_8)
    {
      // UpperIdent
      if (ctx->tokens[ctx->pos].kind == PL0_TOK_SEMI)
      {
        ctx->pos++;
        // Allocate?
      }

    }

    expr_ret_4 = expr_ret_8;
  }

  pl0_astnode_t* expr_ret_11 = NULL;
  {
    pl0_astnode_t* expr_ret_12 = NULL;
    expr_ret_12 = SUCC;
    {
      pl0_astnode_t* expr_ret_13 = NULL;
      {
        // UpperIdent
        if (ctx->tokens[ctx->pos].kind == PL0_TOK_PROC)
        {
          ctx->pos++;
          // Allocate?
        }

      }

      if (expr_ret_13)
      {
        // UpperIdent
        if (ctx->tokens[ctx->pos].kind == PL0_TOK_IDENT)
        {
          ctx->pos++;
          // Allocate?
        }

      }

      if (expr_ret_13)
      {
        // UpperIdent
        if (ctx->tokens[ctx->pos].kind == PL0_TOK_SEMI)
        {
          ctx->pos++;
          // Allocate?
        }

      }

      if (expr_ret_13)
      {
        // LowerIdent
        expr_ret_13 = pl0_parse_block(ctx);
      }

      if (expr_ret_13)
      {
        // UpperIdent
        if (ctx->tokens[ctx->pos].kind == PL0_TOK_SEMI)
        {
          ctx->pos++;
          // Allocate?
        }

      }

      expr_ret_12 = expr_ret_13;
    }

    expr_ret_11 = expr_ret_12;
  }

  if (expr_ret_11)
  {
    // LowerIdent
    expr_ret_11 = pl0_parse_statement(ctx);
  }

  expr_ret_4 = expr_ret_11;
  expr_ret_3 = expr_ret_4;
  return expr_ret_3;
  #undef rule
}
static inline pl0_astnode_t* pl0_parse_statement(pl0_parser_ctx* ctx) {
  #define rule expr_ret_14;
  pl0_astnode_t _succ;
  pl0_astnode_t* SUCC = &_succ;
  pl0_astnode_t* expr_ret_14 = NULL;

  pl0_astnode_t* expr_ret_15 = NULL;

  if (!expr_ret_15)
  {
    pl0_astnode_t* expr_ret_16 = NULL;
    {
      // UpperIdent
      if (ctx->tokens[ctx->pos].kind == PL0_TOK_IDENT)
      {
        ctx->pos++;
        // Allocate?
      }

    }

    if (expr_ret_16)
    {
      // UpperIdent
      if (ctx->tokens[ctx->pos].kind == PL0_TOK_CEQ)
      {
        ctx->pos++;
        // Allocate?
      }

    }

    if (expr_ret_16)
    {
      // LowerIdent
      expr_ret_16 = pl0_parse_expression(ctx);
    }

    expr_ret_15 = expr_ret_16;
  }

  if (!expr_ret_15)
  {
    pl0_astnode_t* expr_ret_17 = NULL;
    {
      // UpperIdent
      if (ctx->tokens[ctx->pos].kind == PL0_TOK_CALL)
      {
        ctx->pos++;
        // Allocate?
      }

    }

    if (expr_ret_17)
    {
      // UpperIdent
      if (ctx->tokens[ctx->pos].kind == PL0_TOK_IDENT)
      {
        ctx->pos++;
        // Allocate?
      }

    }

    expr_ret_15 = expr_ret_17;
  }

  if (!expr_ret_15)
  {
    pl0_astnode_t* expr_ret_18 = NULL;
    {
      // UpperIdent
      if (ctx->tokens[ctx->pos].kind == PL0_TOK_BEGIN)
      {
        ctx->pos++;
        // Allocate?
      }

    }

    if (expr_ret_18)
    {
      // LowerIdent
      expr_ret_18 = pl0_parse_statement(ctx);
    }

    if (expr_ret_18)
    {
      pl0_astnode_t* expr_ret_19 = NULL;
      expr_ret_19 = SUCC;
      {
        pl0_astnode_t* expr_ret_20 = NULL;
        {
          // UpperIdent
          if (ctx->tokens[ctx->pos].kind == PL0_TOK_SEMI)
          {
            ctx->pos++;
            // Allocate?
          }

        }

        if (expr_ret_20)
        {
          // LowerIdent
          expr_ret_20 = pl0_parse_statement(ctx);
        }

        expr_ret_19 = expr_ret_20;
      }

      expr_ret_18 = expr_ret_19;
    }

    if (expr_ret_18)
    {
      // UpperIdent
      if (ctx->tokens[ctx->pos].kind == PL0_TOK_END)
      {
        ctx->pos++;
        // Allocate?
      }

    }

    expr_ret_15 = expr_ret_18;
  }

  if (!expr_ret_15)
  {
    pl0_astnode_t* expr_ret_21 = NULL;
    {
      // UpperIdent
      if (ctx->tokens[ctx->pos].kind == PL0_TOK_IF)
      {
        ctx->pos++;
        // Allocate?
      }

    }

    if (expr_ret_21)
    {
      // LowerIdent
      expr_ret_21 = pl0_parse_condition(ctx);
    }

    if (expr_ret_21)
    {
      // UpperIdent
      if (ctx->tokens[ctx->pos].kind == PL0_TOK_THEN)
      {
        ctx->pos++;
        // Allocate?
      }

    }

    if (expr_ret_21)
    {
      // LowerIdent
      expr_ret_21 = pl0_parse_statement(ctx);
    }

    expr_ret_15 = expr_ret_21;
  }

  pl0_astnode_t* expr_ret_22 = NULL;
  {
    // UpperIdent
    if (ctx->tokens[ctx->pos].kind == PL0_TOK_WHILE)
    {
      ctx->pos++;
      // Allocate?
    }

  }

  if (expr_ret_22)
  {
    // LowerIdent
    expr_ret_22 = pl0_parse_condition(ctx);
  }

  if (expr_ret_22)
  {
    // UpperIdent
    if (ctx->tokens[ctx->pos].kind == PL0_TOK_DO)
    {
      ctx->pos++;
      // Allocate?
    }

  }

  if (expr_ret_22)
  {
    // LowerIdent
    expr_ret_22 = pl0_parse_statement(ctx);
  }

  expr_ret_15 = expr_ret_22;
  expr_ret_14 = expr_ret_15;
  return expr_ret_14;
  #undef rule
}
static inline pl0_astnode_t* pl0_parse_condition(pl0_parser_ctx* ctx) {
  #define rule expr_ret_23;
  pl0_astnode_t _succ;
  pl0_astnode_t* SUCC = &_succ;
  pl0_astnode_t* expr_ret_23 = NULL;

  pl0_astnode_t* expr_ret_24 = NULL;

  if (!expr_ret_24)
  {
    pl0_astnode_t* expr_ret_25 = NULL;
    {
      // UpperIdent
      if (ctx->tokens[ctx->pos].kind == PL0_TOK_ODD)
      {
        ctx->pos++;
        // Allocate?
      }

    }

    if (expr_ret_25)
    {
      // LowerIdent
      expr_ret_25 = pl0_parse_expression(ctx);
    }

    expr_ret_24 = expr_ret_25;
  }

  pl0_astnode_t* expr_ret_26 = NULL;
  {
    // LowerIdent
    expr_ret_26 = pl0_parse_expression(ctx);
  }

  if (expr_ret_26)
  {
    pl0_astnode_t* expr_ret_27 = NULL;

    if (!expr_ret_27)
    {
      // UpperIdent
      if (ctx->tokens[ctx->pos].kind == PL0_TOK_EQ)
      {
        ctx->pos++;
        // Allocate?
      }

    }

    if (!expr_ret_27)
    {
      // UpperIdent
      if (ctx->tokens[ctx->pos].kind == PL0_TOK_HASH)
      {
        ctx->pos++;
        // Allocate?
      }

    }

    if (!expr_ret_27)
    {
      // UpperIdent
      if (ctx->tokens[ctx->pos].kind == PL0_TOK_LT)
      {
        ctx->pos++;
        // Allocate?
      }

    }

    if (!expr_ret_27)
    {
      // UpperIdent
      if (ctx->tokens[ctx->pos].kind == PL0_TOK_LEQ)
      {
        ctx->pos++;
        // Allocate?
      }

    }

    if (!expr_ret_27)
    {
      // UpperIdent
      if (ctx->tokens[ctx->pos].kind == PL0_TOK_GT)
      {
        ctx->pos++;
        // Allocate?
      }

    }

    // UpperIdent
    if (ctx->tokens[ctx->pos].kind == PL0_TOK_GEQ)
    {
      ctx->pos++;
      // Allocate?
    }

    expr_ret_26 = expr_ret_27;
  }

  if (expr_ret_26)
  {
    // LowerIdent
    expr_ret_26 = pl0_parse_expression(ctx);
  }

  expr_ret_24 = expr_ret_26;
  expr_ret_23 = expr_ret_24;
  return expr_ret_23;
  #undef rule
}
static inline pl0_astnode_t* pl0_parse_expression(pl0_parser_ctx* ctx) {
  #define rule expr_ret_28;
  pl0_astnode_t _succ;
  pl0_astnode_t* SUCC = &_succ;
  pl0_astnode_t* expr_ret_28 = NULL;

  pl0_astnode_t* expr_ret_29 = NULL;
  {
    pl0_astnode_t* expr_ret_30 = NULL;
    {
      pl0_astnode_t* expr_ret_31 = NULL;

      if (!expr_ret_31)
      {
        // UpperIdent
        if (ctx->tokens[ctx->pos].kind == PL0_TOK_PLUS)
        {
          ctx->pos++;
          // Allocate?
        }

      }

      // UpperIdent
      if (ctx->tokens[ctx->pos].kind == PL0_TOK_MINUS)
      {
        ctx->pos++;
        // Allocate?
      }

      expr_ret_30 = expr_ret_31;
    }

    // optional
    if (!expr_ret_30)
      expr_ret_30 = SUCC;
    expr_ret_29 = expr_ret_30;
  }

  if (expr_ret_29)
  {
    // LowerIdent
    expr_ret_29 = pl0_parse_term(ctx);
  }

  if (expr_ret_29)
  {
    pl0_astnode_t* expr_ret_32 = NULL;
    expr_ret_32 = SUCC;
    {
      pl0_astnode_t* expr_ret_33 = NULL;
      {
        pl0_astnode_t* expr_ret_34 = NULL;

        if (!expr_ret_34)
        {
          // UpperIdent
          if (ctx->tokens[ctx->pos].kind == PL0_TOK_PLUS)
          {
            ctx->pos++;
            // Allocate?
          }

        }

        // UpperIdent
        if (ctx->tokens[ctx->pos].kind == PL0_TOK_MINUS)
        {
          ctx->pos++;
          // Allocate?
        }

        expr_ret_33 = expr_ret_34;
      }

      if (expr_ret_33)
      {
        // LowerIdent
        expr_ret_33 = pl0_parse_term(ctx);
      }

      expr_ret_32 = expr_ret_33;
    }

    expr_ret_29 = expr_ret_32;
  }

  expr_ret_28 = expr_ret_29;
  return expr_ret_28;
  #undef rule
}
static inline pl0_astnode_t* pl0_parse_term(pl0_parser_ctx* ctx) {
  #define rule expr_ret_35;
  pl0_astnode_t _succ;
  pl0_astnode_t* SUCC = &_succ;
  pl0_astnode_t* expr_ret_35 = NULL;

  pl0_astnode_t* expr_ret_36 = NULL;
  {
    // LowerIdent
    expr_ret_36 = pl0_parse_factor(ctx);
  }

  if (expr_ret_36)
  {
    pl0_astnode_t* expr_ret_37 = NULL;
    expr_ret_37 = SUCC;
    {
      pl0_astnode_t* expr_ret_38 = NULL;
      {
        pl0_astnode_t* expr_ret_39 = NULL;

        if (!expr_ret_39)
        {
          // UpperIdent
          if (ctx->tokens[ctx->pos].kind == PL0_TOK_STAR)
          {
            ctx->pos++;
            // Allocate?
          }

        }

        // UpperIdent
        if (ctx->tokens[ctx->pos].kind == PL0_TOK_DIV)
        {
          ctx->pos++;
          // Allocate?
        }

        expr_ret_38 = expr_ret_39;
      }

      if (expr_ret_38)
      {
        // LowerIdent
        expr_ret_38 = pl0_parse_factor(ctx);
      }

      expr_ret_37 = expr_ret_38;
    }

    expr_ret_36 = expr_ret_37;
  }

  expr_ret_35 = expr_ret_36;
  return expr_ret_35;
  #undef rule
}
static inline pl0_astnode_t* pl0_parse_factor(pl0_parser_ctx* ctx) {
  #define rule expr_ret_40;
  pl0_astnode_t _succ;
  pl0_astnode_t* SUCC = &_succ;
  pl0_astnode_t* expr_ret_40 = NULL;

  pl0_astnode_t* expr_ret_41 = NULL;

  if (!expr_ret_41)
  {
    // UpperIdent
    if (ctx->tokens[ctx->pos].kind == PL0_TOK_IDENT)
    {
      ctx->pos++;
      // Allocate?
    }

  }

  if (!expr_ret_41)
  {
    // UpperIdent
    if (ctx->tokens[ctx->pos].kind == PL0_TOK_NUM)
    {
      ctx->pos++;
      // Allocate?
    }

  }

  pl0_astnode_t* expr_ret_42 = NULL;
  {
    // UpperIdent
    if (ctx->tokens[ctx->pos].kind == PL0_TOK_OPEN)
    {
      ctx->pos++;
      // Allocate?
    }

  }

  if (expr_ret_42)
  {
    // LowerIdent
    expr_ret_42 = pl0_parse_expression(ctx);
  }

  if (expr_ret_42)
  {
    // UpperIdent
    if (ctx->tokens[ctx->pos].kind == PL0_TOK_CLOSE)
    {
      ctx->pos++;
      // Allocate?
    }

  }

  expr_ret_41 = expr_ret_42;
  expr_ret_40 = expr_ret_41;
  return expr_ret_40;
  #undef rule
}


#endif /* PGEN_PL0_ASTNODE_INCLUDE */

