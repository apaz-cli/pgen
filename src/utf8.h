#pragma once
#ifndef STDLIB_UTF8
#define STDLIB_UTF8
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#define UTF8_END -1 // 1111 1111
#define UTF8_ERR -2 // 1111 1110

/* The functions in this file convert between
 * Unicode_Codepoint_String_View and
 * UTF8_String_View.
 */

typedef int32_t codepoint_t;

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

  // Add the null terminator, shrink to size
  out_buf[characters_used] = '\0';
  out_buf = (char *)realloc(out_buf, characters_used + 1);

  // Return the result
  ret.str = out_buf;
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

#endif /* STDLIB_UTF8 */
