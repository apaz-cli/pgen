#ifndef PGEN_INCLUDE_PARSER
#define PGEN_INCLUDE_PARSER
#include "util.h"
#include "pccast.h"

typedef struct {
  codepoint_t* str;
  size_t len;
  size_t pos;
} tokparser_ctx;

#define LOOP(times) for (size_t __lctr = 0; __lctr < (times); __lctr++)
#define CURRENT() ctx->str[ctx->pos]
#define NEXT() do { ctx->pos++; } while(0)
#define ADVANCE(num) do { ctx->pos += (num); } while(0)
#define IS_NEXT(str) comp_next_str(ctx, str)
#define RECORD(n) size_t rewind_to_##n = ctx->pos
#define REWIND(n) ctx->pos = rewind_to_##n

static inline int comp_next_str(tokparser_ctx* ctx, char* s) {
  size_t idx = ctx->pos;
  size_t len = strlen(s);

  if (len > (ctx->len - ctx->pos)) return 0;

  for (size_t i = 0; i < len; i++, idx++)
    if ((codepoint_t)s[i] != ctx->str[idx]) return 0;
  return 1;
}

static inline void tok_skip_ws(tokparser_ctx* ctx) {
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
      while (CURRENT() != '\n') NEXT();
    } else if (IS_NEXT(fcom)) {
      while (!IS_NEXT(ecom)) NEXT();
    } else cont = 1;
  }
}



static inline ASTNode* tok_parse_num(tokparser_ctx* ctx) {
  // Copy from codepoints into char*.
  #define BUFSIZE 21
  char ten[bufsize + 1];
  for (size_t i = 0; i < 11; i++) ten[i] = '\0';
  for (size_t i = 0; i < 10; i++) {
    size_t idx = ctx->pos + i;
    if (idx == ctx->len) break;
    if ((ctx->str[idx] < '0') & (ctx->str[idx] > '9')) break;
    ten[i] = (char)ctx->str[idx];
  }

  // Parse an int
  char* endptr;
  errno = 0;
  long i = strtol(ten, &endptr, 10);

  if (!errno) {
    // Advance the parser
    ptrdiff_t diff = endptr - CURRENT();
    ADVANCE(diff);

    // Return a node with the info parsed.
    INIT("num");
    node->extra = malloc(sizeof(long));
    if (!node->extra) OOM();
    *(long*)node->extra = (int)i;
  } else {
    return NULL;
  }
}

static inline void tokparser_parse(tokparser_ctx* ctx, Codepoint_String_View cpsv) {
  ctx->str = cpsv.str;
  ctx->len = cpsv.len;
  ctx->pos = 0;

  ASTNode* rule;
  while (true) {
    tok_skip_ws(ctx);

    tok_skip_ws(ctx);
  }
}

#undef CURRENT
#undef NEXT
#undef IS_NEXT
#endif /* PGEN_INCLUDE_PARSER */
