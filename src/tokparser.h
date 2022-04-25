#ifndef PGEN_INCLUDE_PARSER
#define PGEN_INCLUDE_PARSER
#include "pccast.h"
#include "util.h"

typedef struct {
  codepoint_t *str;
  size_t len;
  size_t pos;
} tokparser_ctx;

static inline void tokparser_ctx_init(tokparser_ctx *ctx,
                                      Codepoint_String_View cpsv) {
  ctx->str = cpsv.str;
  ctx->len = cpsv.len;
  ctx->pos = 0;
}

#define CURRENT() (ctx->str[ctx->pos])
#define NEXT() (ctx->pos++)
#define REMAINING() (ctx->len - ctx->pos) // Includes CURRENT()
#define HAS_CURRENT() (REMAINING() >= 1)
#define HAS_NEXT() (REMAINING() >= 2)
#define HAS_REMAINING(num) (REMAINING() >= (num))
#define ADVANCE(num) (ctx->pos += (num))
#define IS_NEXT(str) comp_next_str(ctx, str)
#define RECORD(id) size_t _rewind_to_##id = ctx->pos
#define REWIND(id) (ctx->pos = _rewind_to_##id)
#define INIT(name) ASTNode *node = ASTNode_new(name)

static inline size_t comp_next_str(tokparser_ctx *ctx, const char *s) {
  size_t len = strlen(s);
  if (HAS_REMAINING(len))
    return 0;

  size_t idx = ctx->pos;
  for (size_t i = 0; i < len; i++)
    if ((codepoint_t)s[i] != ctx->str[idx++])
      return 0;
  return len;
}

static inline void tok_skip_comments_ws(tokparser_ctx *ctx) {
  const char dslsh[3] = {'/', '/', '\0'};
  const char fcom[3] = {'/', '*', '\0'};
  const char ecom[3] = {'*', '/', '\0'};

  // Eat all the whitespace you can.
  while (1) {
    if (!HAS_CURRENT())
      return;
    codepoint_t c = CURRENT();

    // Whitespace
    if ((c == ' ') | (c == '\t') | (c == '\r') | (c == '\n')) {
      NEXT();
    } else if (IS_NEXT(dslsh)) { // Single line comment
      while (HAS_CURRENT() && CURRENT() != '\n')
        NEXT();
    } else if (IS_NEXT(fcom)) { // Multi line comment
      ADVANCE(strlen(fcom));
      while (!IS_NEXT(ecom))
        NEXT();
      // Found "*/"
      if (HAS_REMAINING(strlen(ecom))) {
        ADVANCE(strlen(ecom));
      } else { // EOF
        ERROR("Unterminated multi-line comment.");
      }
    } else // No more whitespace to munch
      return;
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
  if (!HAS_CURRENT())
    return NULL;

  codepoint_t *beginning = &CURRENT();

  while (1) {
    codepoint_t c = CURRENT();
    if ((c != '_') & ('A' <= c) & (c <= 'Z'))
      break;
    else {
      if (!HAS_NEXT())
        break;
      NEXT();
    }
  }

  if (&CURRENT() == beginning)
    return NULL;

  INIT("ident");
  Codepoint_String_View *cpsv;
  node->extra = cpsv =
      (Codepoint_String_View *)malloc(sizeof(Codepoint_String_View));
  if (!cpsv)
    OOM();
  cpsv->len = &CURRENT() - beginning;
  cpsv->str = beginning;
  return node;
}

static inline ASTNode *tok_parse_char(tokparser_ctx *ctx) {
  const char escstart = '\\';
  const char escends[] = "nrt'\"\\v";
  codepoint_t escs[] = {'\n', '\r', '\t', '\'', '"', '\\', '\v'};

  codepoint_t parsed = 0;
  // Escape sequences
  if (IS_NEXT("\\")) {
    ADVANCE(strlen("\\"));

    if (!HAS_CURRENT())
      return NULL;
    for (size_t i = 0; i < strlen(escends); i++) {
      if (CURRENT() == escends[i]) {
        parsed = escs[i];
        goto end;
      }
    }
    ERROR("Invalid escape sequence \"\\%c\" (%lld) in tokenizer file.",
          (char)CURRENT(), (long long)CURRENT());
    return NULL;
  }
  // Normal characters
  else {
    if (!HAS_CURRENT())
      return NULL;
    parsed = CURRENT();
  }

end:
  NEXT();
  INIT("char");
  node->extra = malloc(sizeof(codepoint_t));
  if (!node->extra)
    OOM();
  *(codepoint_t *)node->extra = parsed;
  NEXT();

  return NULL;
}

static inline ASTNode* tok_parse_numset(tokparser_ctx *ctx) {

}

static inline ASTNode* tok_parse_charset(tokparser_ctx *ctx) {

}

static inline ASTNode* tok_parse_pair(tokparser_ctx *ctx) {
  ASTNode* left_numset;
  ASTNode* left_charset;
  RECORD(begin);

  if (!IS_NEXT("(")) {
    return NULL;
  }

  if (!IS_NEXT(",")) {
    return NULL;
  }

  return NULL;
}

static inline ASTNode* tok_parse_LitDef(tokparser_ctx *ctx) {}

static inline ASTNode* tok_parse_SMDef(tokparser_ctx *ctx) {}

static inline ASTNode* tok_parse_TokenDef(tokparser_ctx *ctx) {}

static inline ASTNode* tok_parse_TokenFile(tokparser_ctx *ctx) {}



#endif /* PGEN_INCLUDE_PARSER */
