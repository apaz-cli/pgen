#ifndef PARSERCTX_INCLUDE
#define PARSERCTX_INCLUDE

#include "ast.h"
#include "util.h"
#include "argparse.h"

LIST_DECLARE(codepoint_t)
LIST_DEFINE(codepoint_t)

typedef struct {
  codepoint_t *str;
  size_t len;
  size_t pos;
  size_t line_nbr;
  bool debug;
} parser_ctx;

static inline void parser_ctx_init(parser_ctx *ctx,
                                   Args args, Codepoint_String_View cpsv) {
  ctx->str = cpsv.str;
  ctx->len = cpsv.len;
  ctx->pos = 0;
  ctx->line_nbr = 1;
  ctx->debug = args.g;
}

/*****************/
/* Helper Macros */
/*****************/


#define CURRENT() (ctx->str[ctx->pos])
#define NEXT() (ctx->pos++)
#define REMAINING() (ctx->len - ctx->pos) // Includes CURRENT()
#define HAS_CURRENT() (REMAINING() >= 1)
#define HAS_NEXT() (REMAINING() >= 2)
#define HAS_REMAINING(num) (REMAINING() >= (num))
#define ADVANCE(num) (ctx->pos += (num))
#define IS_CURRENT(str) ctx_is_current(ctx, str)
#define RECORD(id)                                                             \
  size_t _rew_to_##id = ctx->pos, _rew_to_line_nbr_##id = ctx->line_nbr
#define REWIND(id)                                                             \
  (ctx->line_nbr = _rew_to_line_nbr_##id, ctx->pos = _rew_to_##id)
#define WS() parse_ws(ctx)
#define INIT(name) ASTNode *node = ASTNode_new(name)

#define RULE_BEGIN(rulename)                                                   \
  /* Can't be in a do block. Oh well. */                                       \
  ctx_rule_debug(0, (rulename), ctx);                                          \
  RECORD(begin);                                                               \
  const char *_rulename = (rulename)
#define RULE_SUCCESS() ctx_rule_debug(1, _rulename, ctx)
#define RULE_FAIL() ctx_rule_debug(-1, _rulename, ctx)

#define RETURN(ret)                                                            \
  do {                                                                         \
    if (ret)                                                                   \
      RULE_SUCCESS();                                                          \
    else                                                                       \
      RULE_FAIL();                                                             \
    return (ret);                                                              \
  } while (0)

#ifdef _POSIX_C_SOURCE
#include <sys/ioctl.h>
#include <unistd.h>
#endif
static inline void print_unconsumed(parser_ctx *ctx) {
#ifdef _POSIX_C_SOURCE
  struct winsize w;
  ioctl(STDOUT_FILENO, TIOCGWINSZ, &w);
  // unsigned short cols = w.ws_col;
  unsigned short rows = w.ws_row;

  Codepoint_String_View cpsv;
  cpsv.str = ctx->str + ctx->pos;
  cpsv.len = ctx->len - ctx->pos;

  String_View sv = UTF8_encode_view(cpsv);
  if (!sv.str)
    return;
  size_t current = 0;
  unsigned short printable_lines = rows - 2;
  for (unsigned short i = 0; i < printable_lines; i++) {
    int done = 0;
    size_t last = current;
    if (!sv.str)
      exit(1);
    for (;;) {
      if (sv.str[current] == '\n') {
        current++;
        break;
      } else if (sv.str[current] == '\0') {
        done = 1;
        break;
      } else {
        current++;
      }
    }
    if (done)
      break;

    fwrite_unlocked(sv.str + last, current - last, 1, stdout);
  }
  free(sv.str);
#else
  Codepoint_String_View cpsv;
  cpsv.str = ctx->str + ctx->pos;
  cpsv.len = ctx->len - ctx->pos;
  printCodepointStringView(stdout, cpsv);
#endif
}

static inline void ctx_rule_debug(int status, const char *rulename,
                                  parser_ctx *ctx) {
  if (ctx->debug) {
    if (strcmp(rulename, "ws") == 0)
      return;

    printf("\x1b[2J"); // clear screen
    printf("\x1b[H");  // cursor to top left corner

    if (status == 0) {
      printf("\x1b[34m"); // Blue
    } else if (status == 1) {
      printf("\x1b[32m"); // Green
    } else {
      printf("\x1b[31m"); // Red
    }
    printf("%s\x1b[0m\n", rulename); // Rule name, clear coloring.

    print_unconsumed(ctx);
    getchar();
  }
}

static inline int ctx_is_current(parser_ctx *ctx, const char *s) {
  size_t len = strlen(s);
  if (!HAS_REMAINING(len))
    return 0;

  size_t idx = ctx->pos;
  for (size_t i = 0; i < len; i++)
    if ((codepoint_t)s[i] != ctx->str[idx++])
      return 0;

  return 1;
}

// Advances past any whitespace at the start of the context.
static inline void parse_ws(parser_ctx *ctx) {
  const char dslsh[3] = {'/', '/', '\0'};
  const char fcom[3] = {'/', '*', '\0'};
  const char ecom[3] = {'*', '/', '\0'};
  const char bsnl[3] = {'\\', '\n', '\0'};

  RULE_BEGIN("ws");

  // Eat all the whitespace you can.
  while (1) {
    if (!HAS_CURRENT())
      break;
    codepoint_t c = CURRENT();

    // Whitespace
    if ((c == ' ') | (c == '\t') | (c == '\r')) {
      NEXT();
    } else if (c == '\n') {
      ctx->line_nbr++;
      NEXT();
    } else if (IS_CURRENT(bsnl)) {
      ctx->line_nbr++;
      NEXT();
      NEXT();
    } else if (IS_CURRENT(dslsh)) { // Single line comment
      while (HAS_CURRENT() && CURRENT() != '\n')
        NEXT();
    } else if (IS_CURRENT(fcom)) { // Multi line comment
      ADVANCE(strlen(fcom));
      while (!IS_CURRENT(ecom)) {
        if (HAS_CURRENT() && CURRENT() != '\n')
          ctx->line_nbr++;
        NEXT();
      }
      // Found "*/" or EOF
      if (HAS_REMAINING(strlen(ecom))) { // */
        ADVANCE(strlen(ecom));
      }
      // ON EOF, we're fine running off the end.
    } else // No more whitespace to munch
      break;
  }

  RULE_SUCCESS();
}

// returns 0 on EOF.
// In the context of this parser, it should never be 0 otherwise.
static inline codepoint_t peg_parse_Char(parser_ctx *ctx) {

  // Escape sequences
  if (IS_CURRENT("\\")) {
    NEXT();
    if (!HAS_CURRENT())
      return 0;
    codepoint_t ret = CURRENT();
    NEXT();
    if (ret == 'n') // newline
      return '\n';
    else if (ret == 'r') // carriage return
      return '\r';
    else if (ret == 't') // tab
      return '\t';
    else if (ret == '\\') // backslash
      return '\\';
    else if (ret == '\'') // single quote
      return '\'';
    else if (ret == '\"') // double quote
      return '\"';
    else if (ret == 'b') // backspace
      return '\b';
    else if (ret == 'v') // vertical tab
      return '\v';
    else if (ret == 'a') // alert
      return '\a';
    else if (ret == 'f') // form feed
      return '\f';
    else if (ret == '?') // question mark
      return '\?';
    else
      return ret;
  }

  // Normal character
  else if (HAS_CURRENT()) {
    codepoint_t ret = CURRENT();
    NEXT();
    return ret;
  }

  // EOF
  return 0;
}

// Sets *err = 1 when fails to parse a string, *err = 0 when a string is parsed.
// Doesn't modify parser state on failure.
static inline list_codepoint_t parse_codepoint_string(parser_ctx *ctx, int *err) {

  list_codepoint_t cps = list_codepoint_t_new();

  RECORD(begin);

  // Consume starting "
  if (!IS_CURRENT("\"")) {
    return *err = 1, cps;
  }
  NEXT();

  // Read the contents
  while (HAS_CURRENT() && !IS_CURRENT("\"")) {

    codepoint_t c = peg_parse_Char(ctx);
    if (!c) {
      list_codepoint_t_clear(&cps);
      REWIND(begin);
      return *err = 1, cps;
    }

    list_codepoint_t_add(&cps, c);
  }

  // Consume ending "
  if (HAS_CURRENT()) {
    NEXT();
  } else {
    list_codepoint_t_clear(&cps);
    REWIND(begin);
    return *err = 1, cps;
  }

  return *err = 0, cps;
}

#endif /* PARSERCTX_INCLUDE */
