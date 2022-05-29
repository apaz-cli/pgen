
/* START OF UTF8 LIBRARY */

#ifndef PGEN_UTF8
#define PGEN_UTF8
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
static int UTF8_encode(codepoint_t *codepoints, size_t len, char **retstr,
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


#ifndef PL0_TOKENIZER_INCLUDE
#define PL0_TOKENIZER_INCLUDE

#ifndef PL0_TOKENIZER_SOURCEINFO
#define PL0_TOKENIZER_SOURCEINFO 1
#endif

typedef enum { PL0_TOK_STREAMEND, PL0_TOK_EQ, PL0_TOK_CEQ, PL0_TOK_SEMI, PL0_TOK_DOT, PL0_TOK_COMMA, PL0_TOK_OPEN, PL0_TOK_CLOSE, PL0_TOK_HASH, PL0_TOK_LT, PL0_TOK_LEQ, PL0_TOK_GT, PL0_TOK_GEQ, PL0_TOK_PLUS, PL0_TOK_MINUS, PL0_TOK_STAR, PL0_TOK_DIV, PL0_TOK_VAR, PL0_TOK_PROC, PL0_TOK_CONST, PL0_TOK_BEGIN, PL0_TOK_END, PL0_TOK_IF, PL0_TOK_THEN, PL0_TOK_WHILE, PL0_TOK_DO, PL0_TOK_ODD, PL0_TOK_CALL, PL0_TOK_IDENT, PL0_TOK_NUM, } pl0_token_id;

// The 0th token is end of stream.
// Tokens 1 - 29 are the ones you defined.
static size_t pl0_num_tokens = 30;
static const char* pl0_lexeme_name[] = { "PL0_TOK_STREAMEND", "PL0_TOK_EQ", "PL0_TOK_CEQ", "PL0_TOK_SEMI", "PL0_TOK_DOT", "PL0_TOK_COMMA", "PL0_TOK_OPEN", "PL0_TOK_CLOSE", "PL0_TOK_HASH", "PL0_TOK_LT", "PL0_TOK_LEQ", "PL0_TOK_GT", "PL0_TOK_GEQ", "PL0_TOK_PLUS", "PL0_TOK_MINUS", "PL0_TOK_STAR", "PL0_TOK_DIV", "PL0_TOK_VAR", "PL0_TOK_PROC", "PL0_TOK_CONST", "PL0_TOK_BEGIN", "PL0_TOK_END", "PL0_TOK_IF", "PL0_TOK_THEN", "PL0_TOK_WHILE", "PL0_TOK_DO", "PL0_TOK_ODD", "PL0_TOK_CALL", "PL0_TOK_IDENT", "PL0_TOK_NUM", };

typedef struct {
  pl0_token_id lexeme;
  size_t start;
  size_t len;
#if PL0_TOKENIZER_SOURCEINFO
  size_t line;
  size_t col;
  char* sourceFile;
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

static inline void pl0_tokenizer_init(pl0_tokenizer* tokenizer, char* sourceFile, codepoint_t* start, size_t len) {
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
  size_t trie_munch_size = 0;
  size_t smaut_munch_size_0 = 0;
  size_t smaut_munch_size_1 = 0;


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
    }

    // Transition State Machine 0
    if (smaut_state_0 != -1) {
      all_dead = 0;
      if ((c == 95) | ((c >= 97) & (c <= 122)) | ((c >= 65) & (c <= 90))) {
        if (smaut_state_0 == 0) smaut_state_0 = 1;
        else smaut_state_0 = -1;
      }
      else if ((c == 95) | ((c >= 97) & (c <= 122)) | ((c >= 65) & (c <= 90)) | ((c >= 48) & (c <= 57))) {
        if (smaut_state_0 == 1) smaut_state_0 = 2;
        else if (smaut_state_0 == 2) smaut_state_0 = 2;
        else smaut_state_0 = -1;
      }
      else {
        smaut_state_0 = -1;
      }

      int accept = ((smaut_state_0 == 1) | (smaut_state_0 == 2));
      if (accept)
        smaut_munch_size_0 = iidx + 1;
    }

    // Transition State Machine 1
    if (smaut_state_1 != -1) {
      all_dead = 0;
      if ((c == 45) | (c == 43)) {
        if (smaut_state_1 == 0) smaut_state_1 = 1;
        else smaut_state_1 = -1;
      }
      else if (((c >= 48) & (c <= 57))) {
        if (smaut_state_1 == 0) smaut_state_1 = 2;
        else if (smaut_state_1 == 1) smaut_state_1 = 2;
        else if (smaut_state_1 == 2) smaut_state_1 = 2;
        else smaut_state_1 = -1;
      }
      else {
        smaut_state_1 = -1;
      }

      int accept = (smaut_state_1 == 2);
      if (accept)
        smaut_munch_size_1 = iidx + 1;
    }

    if (all_dead)
      break;
  }

  // Determine what token was accepted, if any.
  pl0_token ret;
#if PL0_TOKENIZER_SOURCEINFO
  ret.line = tokenizer->pos_line;
  ret.col = tokenizer->pos_col;
  ret.sourceFile = tokenizer->pos_sourceFile;
#endif

  
  return ret;
}

#endif /* PL0_TOKENIZER_INCLUDE */
