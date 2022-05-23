#ifndef PARSERCTX_INCLUDE
#define PARSERCTX_INCLUDE

#include "ast.h"
#include "util.h"

LIST_DECLARE(codepoint_t)
LIST_DEFINE(codepoint_t)

typedef struct {
  codepoint_t *str;
  size_t len;
  size_t pos;
} parser_ctx;

static inline void parser_ctx_init(parser_ctx *ctx,
                                   Codepoint_String_View cpsv) {
  ctx->str = cpsv.str;
  ctx->len = cpsv.len;
  ctx->pos = 0;
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
#define RECORD(id) size_t _rew_to_##id = ctx->pos
#define REWIND(id) (ctx->pos = _rew_to_##id)
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

#define DEBUG 0
#if DEBUG
static inline void print_unconsumed(parser_ctx *ctx) {
  Codepoint_String_View cpsv;
  cpsv.str = ctx->str + ctx->pos;
  cpsv.len = ctx->len - ctx->pos;
  printCodepointStringView(cpsv);
}

static inline void ctx_rule_debug(int status, const char *rulename,
                                  parser_ctx *ctx) {

  if (strcmp(rulename, "ws") == 0)
    return;

  if (status == 0) {
    printf("\x1b[34m"); // Blue
  } else if (status == 1) {
    printf("\x1b[32m"); // Green
  } else {
    printf("\x1b[31m"); // Red
  }
  printf("%s\x1b[0m\n", rulename); // Rule name, clear coloring.

#if 1
  print_unconsumed(ctx);
  getchar();
  printf("\x1b[2J"); // clear screen
  printf("\x1b[H");  // cursor to top left corner
#endif
}
#else
static inline void ctx_rule_debug(int status, const char *rulename,
                                  parser_ctx *ctx) {
  (void)status;
  (void)rulename;
  (void)ctx;
}
#endif

static inline bool ctx_is_current(parser_ctx *ctx, const char *s) {
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

  RULE_BEGIN("ws");

  // Eat all the whitespace you can.
  while (1) {
    if (!HAS_CURRENT())
      break;
    codepoint_t c = CURRENT();

    // Whitespace
    if ((c == ' ') | (c == '\t') | (c == '\r') | (c == '\n')) {
      NEXT();
    } else if (IS_CURRENT(dslsh)) { // Single line comment
      while (HAS_CURRENT() && CURRENT() != '\n')
        NEXT();
    } else if (IS_CURRENT(fcom)) { // Multi line comment
      ADVANCE(strlen(fcom));
      while (!IS_CURRENT(ecom))
        NEXT();
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

#endif /* PARSERCTX_INCLUDE */
