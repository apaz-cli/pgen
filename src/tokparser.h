#ifndef PGEN_INCLUDE_PARSER
#define PGEN_INCLUDE_PARSER
#include "pccast.h"
#include "util.h"

typedef struct {
  codepoint_t *str;
  size_t len;
  size_t pos;
} tokparser_ctx;

#define LOOP(times) for (size_t __lctr = 0; __lctr < (times); __lctr++)
#define CURRENT() ctx->str[ctx->pos]
#define REMAINING() (ctx->len - ctx->pos)
#define NEXT() ctx->pos++
#define ADVANCE(num) ctx->pos += (num)
#define IS_NEXT(str) comp_next_str(ctx, str)
#define RECORD(n) size_t rewind_to_##n = ctx->pos
#define REWIND(n) ctx->pos = rewind_to_##n

static inline int comp_next_str(tokparser_ctx *ctx, char *s) {
  size_t idx = ctx->pos;
  size_t len = strlen(s);

  if (len > (ctx->len - ctx->pos))
    return 0;

  for (size_t i = 0; i < len; i++, idx++)
    if ((codepoint_t)s[i] != ctx->str[idx])
      return 0;
  return 1;
}

static inline void tok_skip_ws(tokparser_ctx *ctx) {
  char dslsh[3] = {'/', '/', '\0'};
  char fcom[3] = {'/', '*', '\0'};
  char ecom[3] = {'*', '/', '\0'};

  codepoint_t c = CURRENT();
  bool cont = 0;
  while (cont) {
    // TODO EOF
    if ((c == ' ') | (c == '\t') | (c == '\r') | (c == '\n')) {
      NEXT();
    } else if (IS_NEXT(dslsh)) {
      while (CURRENT() != '\n')
        NEXT();
    } else if (IS_NEXT(fcom)) {
      while (!IS_NEXT(ecom))
        NEXT();
    } else
      cont = 1;
  }
}

static inline ASTNode *tok_parse_num(tokparser_ctx *ctx) {
  size_t read;
  int i = codepoint_atoi(&CURRENT(), REMAINING(), &read);
  if (!read) {
    ADVANCE(read);

    // Return a node with the info parsed.
    INIT("num");
    node->extra = malloc(sizeof(int));
    if (!node->extra)
      OOM();
    *(int *)node->extra = i;
    return node;
  } else {
    return NULL;
  }
}

static inline ASTNode *tok_parse_ident(tokparser_ctx *ctx) {
  bool cont = true;
  do {

  } while (cont);
}

static inline ASTNode *tok_parse_char(tokparser_ctx *ctx) {}

static inline void tokparser_parse(tokparser_ctx *ctx,
                                   Codepoint_String_View cpsv) {
  ctx->str = cpsv.str;
  ctx->len = cpsv.len;
  ctx->pos = 0;

  ASTNode *rule;
  while (true) {
    tok_skip_ws(ctx);
  }
}

#undef CURRENT
#undef NEXT
#undef IS_NEXT
#endif /* PGEN_INCLUDE_PARSER */
