
/* START OF UTF8 LIBRARY */

#ifndef PGEN_UTF8
#define PGEN_UTF8
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>


#define UTF8_END -1 // 1111 1111
#define UTF8_ERR -2 // 1111 1110

/* The functions in this file convert between
 * Unicode_Codepoint_String_View and
 * UTF8_String_View.
 */

typedef int32_t codepoint_t;
#define PRI_CODEPOINT PRIu32 


typedef struct {
  char *str;
  size_t len;
} String_View;

typedef struct {
  codepoint_t *str;
  size_t len;
} Codepoint_String_View;

typedef struct {
  size_t idx;
  size_t len;
  size_t chr;
  size_t byte;
  char *inp;
} UTF8State;

/***********/
/* Helpers */
/***********/

/* Get the next byte. Returns UTF8_END if there are no more bytes. */
static inline char UTF8_nextByte(UTF8State *state) {
  if (state->idx >= state->len)
    return UTF8_END;
  char c = (state->inp[state->idx] & 0xFF);
  state->idx += 1;
  return c;
}

/*
 * Get the 6-bit payload of the next continuation byte.
 * Return UTF8_ERR if it is not a contination byte.
 */
static inline char UTF8_contByte(UTF8State *state) {
  char c = UTF8_nextByte(state);
  return ((c & 0xC0) == 0x80) ? (c & 0x3F) : UTF8_ERR;
}

/**************/
/* Public API */
/**************/

/**********/
/* DECODE */
/**********/

/* Initialize the UTF-8 decoder. The decoder is not reentrant. */
static inline void UTF8_decoder_init(UTF8State *state, char *str, size_t len) {
  state->idx = 0;
  state->inp = str;
  state->len = len;
  state->chr = 0;
  state->byte = 0;
}

/* Get the current byte offset. This is generally used in error reporting. */
static inline size_t UTF8_atByte(UTF8State *state) { return state->byte; }

/*
 * Get the current character offset. This is generally used in error reporting.
 * The character offset matches the byte offset if the text is strictly ASCII.
 */
static inline size_t UTF8_atCharacter(UTF8State *state) {
  return (state->chr > 0) ? state->chr - 1 : 0;
}

/*
 * Extract the next unicode code point.
 * Returns the character, or UTF8_END, or UTF8_ERR.
 */
static inline codepoint_t UTF8_decodeNext(UTF8State *state) {
  char c;        /* the first byte of the character */
  char c1;       /* the first continuation character */
  char c2;       /* the second continuation character */
  char c3;       /* the third continuation character */
  codepoint_t r; /* the result */

  if (state->idx >= state->len)
    return state->idx == state->len ? UTF8_END : UTF8_ERR;

  state->byte = state->idx;
  state->chr += 1;
  c = UTF8_nextByte(state);

  /* Zero continuation (0 to 127) */
  if ((c & 0x80) == 0) {
    return (codepoint_t)c;
  }
  /* One continuation (128 to 2047) */
  else if ((c & 0xE0) == 0xC0) {
    c1 = UTF8_contByte(state);
    if (c1 >= 0) {
      r = ((c & 0x1F) << 6) | c1;
      if (r >= 128)
        return r;
    }
  }
  /* Two continuations (2048 to 55295 and 57344 to 65535) */
  else if ((c & 0xF0) == 0xE0) {
    c1 = UTF8_contByte(state);
    c2 = UTF8_contByte(state);
    if ((c1 | c2) >= 0) {
      r = ((c & 0x0F) << 12) | (c1 << 6) | c2;
      if (r >= 2048 && (r < 55296 || r > 57343))
        return r;
    }
  }
  /* Three continuations (65536 to 1114111) */
  else if ((c & 0xF8) == 0xF0) {
    c1 = UTF8_contByte(state);
    c2 = UTF8_contByte(state);
    c3 = UTF8_contByte(state);
    if ((c1 | c2 | c3) >= 0) {
      r = ((c & 0x07) << 18) | (c1 << 12) | (c2 << 6) | c3;
      if (r >= 65536 && r <= 1114111)
        return r;
    }
  }
  return UTF8_ERR;
}

/**********/
/* ENCODE */
/**********/

/*
 * Encodes the given UTF8 code point into the given buffer.
 * Returns the number of characters in the buffer used.
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

static inline String_View UTF8_encode(codepoint_t *codepoints, size_t len) {
  String_View ret;
  ret.str = NULL;
  ret.len = 0;

  // Allocate at least enough memory.
  char buf4[4];
  char *out_buf = (char *)malloc(len * sizeof(codepoint_t) + 1);
  if (!out_buf)
    return ret;

  size_t characters_used = 0;

  // For each unicode codepoint
  for (size_t i = 0; i < len; i++) {
    // Decode it, handle error
    size_t used = UTF8_encodeNext(codepoints[i], buf4);
    if (!used)
      return ret;

    // Copy the result onto the end of the buffer
    for (size_t j = 0; j < used; j++)
      out_buf[characters_used++] = buf4[j];
  }

  // Add the null terminator, (optionally) shrink to size
  out_buf[characters_used] = '\0';
  char* new_obuf = (char *)realloc(out_buf, characters_used + 1);

  // Return the result
  ret.str = new_obuf ? new_obuf : out_buf;
  ret.len = characters_used;
  return ret;
}

static inline Codepoint_String_View UTF8_decode_view(String_View view) {

  /* Over-allocate enough space for the UTF32 translation. */
  codepoint_t *cps = (codepoint_t *)malloc(sizeof(codepoint_t) * view.len);
  Codepoint_String_View cpsv;
  if (!cps) {
      cpsv.str = NULL;
      cpsv.len = 0;
      return cpsv;
  }

  /* Parse */
  size_t cps_read = 0;
  codepoint_t cp;
  UTF8State state;
  UTF8_decoder_init(&state, view.str, view.len);
  for (;;) {
    cp = UTF8_decodeNext(&state);

    if (cp == UTF8_ERR) {
      free(view.str);
      free(cps);
      cpsv.str = NULL;
      cpsv.len = 0;
      return cpsv;
    } else if (cp == UTF8_END) {
      break;
    } else {
      cps[cps_read++] = cp;
    }
  };

  cpsv.str = cps;
  cpsv.len = cps_read;
  return cpsv;
}

static inline String_View UTF8_encode_view(Codepoint_String_View view) {
  return UTF8_encode(view.str, view.len);
}

#endif /* PGEN_UTF8 */

/* END OF UTF8 LIBRARY */


#ifndef PL0_TOKENIZER_INCLUDE
#define PL0_TOKENIZER_INCLUDE

#ifndef PL0_TOKENIZER_SOURCEINFO
#define PL0_TOKENIZER_SOURCEINFO 1
#endif

#define PL0_TOKENS PL0_TOK_STREAMEND, PL0_TOK_EQ, PL0_TOK_CEQ, PL0_TOK_SEMI, PL0_TOK_DOT, PL0_TOK_COMMA, PL0_TOK_OPEN, PL0_TOK_CLOSE, PL0_TOK_HASH, PL0_TOK_LT, PL0_TOK_LEQ, PL0_TOK_GT, PL0_TOK_GEQ, PL0_TOK_PLUS, PL0_TOK_MINUS, PL0_TOK_STAR, PL0_TOK_DIV, PL0_TOK_VAR, PL0_TOK_PROC, PL0_TOK_CONST, PL0_TOK_BEGIN, PL0_TOK_END, PL0_TOK_IF, PL0_TOK_THEN, PL0_TOK_WHILE, PL0_TOK_DO, PL0_TOK_ODD, PL0_TOK_CALL, PL0_TOK_IDENT, PL0_TOK_NUM, 

typedef enum { PL0_TOKENS } pl0_token_id;
static pl0_token_id _pl0_num_tokids[] = { PL0_TOKENS };
static size_t pl0_num_tokens = (sizeof(_pl0_num_tokids) / sizeof(pl0_token_id)) - 1;

typedef struct {
  pl0_token_id lexeme;
  codepoint_t* start;
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
  int smaut_states[2] = {0, 0};
  size_t trie_munch_size = 0;
  size_t smaut_munch_size[2] = {0, 0};

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
    if (smaut_states[0] != -1) {
      all_dead = 0;
      if ((c == 95) | ((c >= 97) & (c <= 122)) | ((c >= 65) & (c <= 90))) {
        if (smaut_states[0] == 0) smaut_states[0] = 1;
        else smaut_states[0] = -1;
      }
      else if ((c == 95) | ((c >= 97) & (c <= 122)) | ((c >= 65) & (c <= 90)) | ((c >= 48) & (c <= 57))) {
        if (smaut_states[0] == 1) smaut_states[0] = 2;
        else if (smaut_states[0] == 2) smaut_states[0] = 2;
        else smaut_states[0] = -1;
      }
      else {
        smaut_states[0] = -1;
      }

      int accept = ((smaut_states[0] == 1) | (smaut_states[0] == 2));
      if (accept)
        smaut_munch_size[0] = iidx + 1;
    }

    // Transition State Machine 1
    if (smaut_states[1] != -1) {
      all_dead = 0;
      if ((c == 45) | (c == 43)) {
        if (smaut_states[1] == 0) smaut_states[1] = 1;
        else smaut_states[1] = -1;
      }
      else if (((c >= 48) & (c <= 57))) {
        if (smaut_states[1] == 0) smaut_states[1] = 2;
        else if (smaut_states[1] == 1) smaut_states[1] = 2;
        else if (smaut_states[1] == 2) smaut_states[1] = 2;
        else smaut_states[1] = -1;
      }
      else {
        smaut_states[1] = -1;
      }

      int accept = (smaut_states[1] == 2);
      if (accept)
        smaut_munch_size[1] = iidx + 1;
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
