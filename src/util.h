#ifndef PGEN_INCLUDE_UTIL
#define PGEN_INCLUDE_UTIL
#include <errno.h>
#include <limits.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

#include "list.h"
#include "utf8.h"

typedef char *cstr;
LIST_DECLARE(cstr)
LIST_DEFINE(cstr)

typedef struct {
  char *str;
  size_t len;
} String_View;

typedef struct {
  codepoint_t *str;
  size_t len;
} Codepoint_String_View;

static inline String_View UTF8_encode_view(Codepoint_String_View view) {
  String_View sv = {NULL, 0};
  UTF8_encode(view.str, view.len, &sv.str, &sv.len);
  return sv; // returns {NULL, 0} on failure since encode doesn't write.
}

static inline Codepoint_String_View UTF8_decode_view(String_View view) {
  Codepoint_String_View sv = {NULL, 0};
  UTF8_decode(view.str, view.len, &sv.str, &sv.len);
  return sv; // returns {NULL, 0} on failure since encode doesn't write.
}

LIST_DECLARE(String_View)
LIST_DEFINE(String_View)
LIST_DECLARE(Codepoint_String_View)
LIST_DEFINE(Codepoint_String_View)
LIST_DECLARE(size_t)
LIST_DEFINE(size_t)

/* Open the file and returns its contents in a string. */
/* Returns {.str = NULL, .len = 0} on error, and errno is set to indicate the
 * error. */
/* Returns {.str = NULL, .len = 1} on OOM. */
/* {.str} must be freed. */
/* Note: Determining the size of a file using ftell() is technically UB,
 * but it works on every OS and architecture under the sun. */
static inline String_View readFile(char *filePath) {
  String_View err;
  err.str = NULL;
  err.len = 0;
  String_View oom;
  oom.str = NULL;
  oom.len = 1;

  long inputFileLen;
  FILE *inputFile;
  char *filestr;

  if (!(inputFile = fopen(filePath, "r")))
    return err;
  if (fseek(inputFile, 0, SEEK_END) == -1)
    return fclose(inputFile), err;
  if ((inputFileLen = ftell(inputFile)) == -1)
    return fclose(inputFile), err;
  if (fseek(inputFile, 0, SEEK_SET) == -1)
    return fclose(inputFile), err;
  if (!(filestr = (char *)malloc((size_t)inputFileLen)))
    return fclose(inputFile), oom;
  if (!fread(filestr, 1, (size_t)inputFileLen, inputFile))
    return fclose(inputFile), free(filestr), err;
  fclose(inputFile);

  String_View ret;
  ret.str = filestr;
  ret.len = (size_t)inputFileLen;
  return ret;
}

static inline size_t cpstrlen(codepoint_t *cpstr) {
  size_t cnt = 0;
  while (*cpstr != '\0') {
    cnt++;
    cpstr++;
  }
  return cnt;
}

static inline bool cpstr_equals(codepoint_t *str1, codepoint_t *str2) {
  while (*str1 && (*str1 == *str2))
    str1++, str2++;
  return *str1 == *str2;
}

static inline void printStringView(FILE *f, String_View sv) {
  fwrite(sv.str, sv.len, 1, f);
}

static inline void printCodepointStringView(FILE *f,
                                            Codepoint_String_View cpsv) {
  String_View sv = UTF8_encode_view(cpsv);
  printStringView(f, sv);
  free(sv.str);
}

/* Open the file as utf8, returning a string of utf32 codepoints. */
/* Returns {.str = NULL, .len = 0} on UTF8 parsing error. */
/* Returns {.str = NULL, .len = 1} on OOM. */
/* {.str} must be freed. */
static inline Codepoint_String_View readFileCodepoints(char *filePath) {
  /* Read the file. */
  String_View view = readFile(filePath);
  Codepoint_String_View cpsv;
  if (!view.str) {
    cpsv.str = NULL;
    cpsv.len = view.len;
    return cpsv;
  } else {
    cpsv = UTF8_decode_view(view);
    free(view.str);
    return cpsv;
  }
}

/* Open the file as utf8, converting to utf32 and returning each line in a list.
 */
/* Returns an empty list on UTF8 parsing error or OOM. */
/* The list must be destroyed, and list.get(0).str must be freed. */
static inline list_Codepoint_String_View
readFileCodepointLines(char *filePath) {

  // Read the file as codepoints
  Codepoint_String_View target = readFileCodepoints(filePath);

  list_Codepoint_String_View lines = list_Codepoint_String_View_new();
  if (!target.str)
    return lines; // return empty list on error

  // Split into lines.
  for (size_t i = 0, last = 0; i < target.len; i++) {
    if ((target.str[i] == '\n') | (i == target.len - 1)) {
      Codepoint_String_View sv;
      sv.str = target.str + last;
      sv.len = i - last;
      list_Codepoint_String_View_add(&lines, sv);

      last = i;
    }
  }

  return lines;
}

static inline list_size_t find_sv_newlines(String_View sv) {
  list_size_t l = list_size_t_new();
  for (size_t i = 0; i < sv.len; i++) {
    if (sv.str[i] == '\n')
      list_size_t_add(&l, i);
  }
  return l;
}

static inline list_size_t find_cpsv_newlines(Codepoint_String_View cpsv) {
  list_size_t l = list_size_t_new();
  for (size_t i = 0; i < cpsv.len; i++) {
    if (cpsv.str[i] == '\n')
      list_size_t_add(&l, i);
  }
  return l;
}

// Assigns SIZE_MAX to *read on error when the number is larger than ULLONG_MAX.
// Assigns 0 to *read on error when no digits are parsed.
// Returns the number parsed on success, 0 on failure.
// Note that 0 is a potential success value, so check *read.
static inline unsigned long long
codepoint_atoull_nosigns(const codepoint_t *a, size_t len, size_t *read) {

  unsigned long long parsed = 0;
  size_t chars = 0;
  for (size_t i = 0; i < len; i++) {
    codepoint_t c = a[i];
    if (!((c >= 48) && (c <= 57)))
      break;
    else
      chars++;

    // Error if shifting the place would overflow
    if ((parsed > ULLONG_MAX / 10)) {
      *read = SIZE_MAX;
      return 0;
    }
    parsed *= 10;

    // Error if adding would overflow
    unsigned long long toadd = ((unsigned long long)c - 48ull);
    if (toadd > ULLONG_MAX - parsed) {
      *read = SIZE_MAX;
      return 0;
    }
    parsed += toadd;
  }

  *read = chars;
  return parsed;
}

// Assigns SIZE_MAX to *read on error when the number is
// larger than INT_MAX or lower than INT_MIN.
// Assigns 0 to *read on error when no digits are parsed.
// Returns the number parsed on success, 0 on failure.
// Note that 0 is a potential success value, so check *read.
static inline int codepoint_atoi(const codepoint_t *a, size_t len,
                                 size_t *read) {
  size_t neg = a[0] == '-';
  size_t sign_parsed = (neg | (a[0] == '+'));
  a += sign_parsed;
  len -= sign_parsed;

  size_t ullread;
  unsigned long long ull = codepoint_atoull_nosigns(a, len, &ullread);
  if (ullread == 0)
    return *read = 0, 0;
  if (ullread == ULLONG_MAX)
    return *read = SIZE_MAX, 0;

  if (neg ? ull > (unsigned long long)-(long long)INT_MIN
          : ull > (unsigned long long)INT_MAX)
    return *read = 0, 0;

  return *read = ullread + sign_parsed, (int)(neg ? -ull : ull);
}

// Aspi is a beautiful SIMD sorceress.

#if defined(__has_include)
#  if __has_include (<emmintrin.h>) && defined(__x86_64__)
#    include <emmintrin.h>
#    define SIMD_MERGE 1
#    ifdef __GNUC__
#     define LIKELY(x) __builtin_expect(!!(x), 1)
#    else
#     define LIKELY(x) (x)
#    endif
#  else
#    define SIMD_MERGE 0
#  endif
#endif

#if SIMD_MERGE
// Merge lines ending with '\'. Remove both the '\' and the newline.
// If the \ is at end of file, remove that too.
static inline void mergeLines(Codepoint_String_View *__restrict cpsv) {
  size_t newpos = 0;
  size_t pos = 0;
  if (cpsv->len >= 16) {
    const __m128i backslash = _mm_set1_epi8('\\');
    const __m128i newline = _mm_set1_epi8('\n');
    while (pos <= cpsv->len - 16) {
      // Load and pack 4 vectors
      __m128i a = _mm_loadu_si128((__m128i *)(cpsv->str + pos + 0));
      __m128i b = _mm_loadu_si128((__m128i *)(cpsv->str + pos + 4));
      __m128i c = _mm_loadu_si128((__m128i *)(cpsv->str + pos + 8));
      __m128i d = _mm_loadu_si128((__m128i *)(cpsv->str + pos + 12));
      __m128i packed_ab = _mm_packs_epi32(a, b);
      __m128i packed_cd = _mm_packs_epi32(c, d);
      __m128i packed = _mm_packs_epi16(packed_ab, packed_cd);

      // Test
      __m128i cmp_backslash = _mm_cmpeq_epi8(packed, backslash);
      __m128i cmp_newline = _mm_cmpeq_epi8(packed, newline);
      __m128i cmp = _mm_and_si128(cmp_backslash, _mm_bsrli_si128(cmp_newline, 1));
      int mask = _mm_movemask_epi8(cmp);
      if (LIKELY(mask == 0)) {
        _mm_storeu_si128((__m128i *)(cpsv->str + newpos + 0), a);
        _mm_storeu_si128((__m128i *)(cpsv->str + newpos + 4), b);
        _mm_storeu_si128((__m128i *)(cpsv->str + newpos + 8), c);
        _mm_storeu_si128((__m128i *)(cpsv->str + newpos + 12), d);
        // We can only advance 15. The 16th might be a backslash.
        newpos += 15;
        pos += 15;
      } else {
        // Either we got a match or a false positive from non-ASCII.
        size_t end = pos + 15;
        while (pos < end) {
          if (cpsv->str[pos] != '\\' || cpsv->str[pos + 1] != '\n') {
            cpsv->str[newpos++] = cpsv->str[pos++];
          } else {
            pos += 2;
          }
        }
      }
    }
  }
  while (pos < cpsv->len - 1) {
    if (cpsv->str[pos] != '\\' || cpsv->str[pos + 1] != '\n') {
      cpsv->str[newpos++] = cpsv->str[pos++];
    } else {
      pos += 2;
    }
  }
  if (cpsv->str[cpsv->len - 1] != '\\') {
    cpsv->str[newpos++] = cpsv->str[cpsv->len - 1];
  }

  cpsv->len = newpos;
}

#else

// Merge lines ending with '\'. Remove both the '\' and the newline.
// If the \ is at end of file, remove that too.
static inline void mergeLines(Codepoint_String_View *cpsv) {
  if (!cpsv->len)
    return;
  size_t newlen = 0;
  for (size_t i = 0; i < cpsv->len - 1;) {
    if ((cpsv->str[i] != '\\') | (cpsv->str[i + 1] != '\n'))
      cpsv->str[newlen++] = cpsv->str[i++];
    else
      i += 2;
  }
  if (cpsv->str[cpsv->len - 1] != '\\')
    cpsv->str[newlen++] = cpsv->str[cpsv->len - 1];

  cpsv->len = newlen;
}
#endif

#endif /* PGEN_INCLUDE_UTIL */
