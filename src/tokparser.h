#ifndef PGEN_INCLUDE_PARSER
#define PGEN_INCLUDE_PARSER
#include "ast.h"
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
#define RECORD(id) size_t _rew_to_##id = ctx->pos
#define REWIND(id) (ctx->pos = _rew_to_##id)
#define WS() tok_parse_ws(ctx)
#define INIT(name) ASTNode *node = ASTNode_new(name)

#define ADVANCED_BY(id1, id2) (MAX(_rew_to##id1, _rew_to##id2) \
                             - MIN(_rew_to##id1, _rew_to##id2))


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

static inline void tok_parse_ws(tokparser_ctx *ctx) {
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

// The same as codepoint_atoi, but also advances by the amount read.
static inline int tok_parse_num(tokparser_ctx *ctx, size_t* read) {
  int i = codepoint_atoi(&CURRENT(), REMAINING(), read);
  ADVANCE(*read)
  return i;
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

static inline codepoint_t tok_parse_char(tokparser_ctx *ctx) {
  const char escstart = '\\';
  const char escends[] = "nrt'\"\\v]";
  codepoint_t escs[] = {'\n', '\r', '\t', '\'', '"', '\\', '\v', ']'};

  // Escape sequences
  if (IS_NEXT("\\")) {
    ADVANCE(strlen("\\"));

    if (!HAS_CURRENT())
      return 0;
    for (size_t i = 0; i < strlen(escends); i++) {
      if (CURRENT() == escends[i]) {
        return escs[i];
      }
    }
    ERROR("Invalid escape sequence \"\\%c\" (%lld) in tokenizer file.",
          (char)CURRENT(), (long long)CURRENT());
    return 0;
  }

  // Normal characters
  if (HAS_CURRENT()){
    codepoint_t ret = CURRENT();
    NEXT();
    return ret;
  }

  // EOF
  return 0;
}

static inline ASTNode* tok_parse_numset(tokparser_ctx *ctx) {

  RECORD(begin);

  // Try to parse a number
  size_t advanced_by;
  int simple = tok_parse_num(ctx, &advanced_by);
  if (advanced_by) {
    INIT("num");
    node->extra = malloc(sizeof(codepoint_t));
    if (!node->extra)
      OOM();
    *(int*)(node->extra) = simple;
    return node;
  }

  // Parse the first bit of either of the next two options.
  if (!IS_NEXT("(")) {
    return NULL;
  }
  NEXT();

  WS();

  RECORD(checkpoint);

  // Try to parse a num range
  int range1 = tok_parse_num(ctx, &advanced_by);
  if (advanced_by) {
    WS();

    if (!IS_NEXT("-")) {
      return NULL;
    }
    NEXT();

    WS();

    int range2 = tok_parse_num(ctx, &advanced_by);
    if (advanced_by) {
      WS();

      if (!IS_NEXT(")")) {
        REWIND(begin);
        return NULL;
      }
      NEXT();

      INIT("numrange");
      int* iptr;
      node->extra = iptr = (int*)malloc(sizeof(int) * 2);
      *iptr = range1;
      *(iptr + 1) = range2;
      return node;
    }
  }

  REWIND(checkpoint);

  // Try to parse a numset list
  ASTNode* first = tok_parse_numset(ctx);
  if (!first) {
    REWIND(begin);
    return NULL;
  }

  INIT("numsetlist");
  ASTNode_addChild(first);

  WS();

  while(1) {

    RECORD(kleene);

    if (!IS_NEXT(",")
      break;

    NEXT();

    WS();

    ASTNode* next = tok_parse_numset(ctx);
    if (!next) {
      REWIND(kleene);
      ASTNode_destroy(node);
      return node;
    }
    ASTNode_addChild(next);

    WS();
  }

  if (!IS_NEXT(")")) {
    REWIND(begin);
    ASTNode_destroy(node);
    return NULL;
  }
  NEXT();

  return node;
}

static inline ASTNode* tok_parse_charset(tokparser_ctx *ctx) {

  RECORD(begin);

  if (IS_NEXT("'")) {
    NEXT();

    codepoint_t c = tok_parse_char(ctx);
    if (!c) {
      REWIND(begin);
      return NULL;
    }

    if (!IS_NEXT("'")) {
      REWIND(begin);
      return NULL;
    }
    NEXT();

    INIT("char");
    return NULL;
  }


}

static inline ASTNode* tok_parse_pair(tokparser_ctx *ctx) {
  ASTNode* left_numset;
  ASTNode* right_charset;
  RECORD(begin);

  if (!IS_NEXT("(")) {
    return NULL;
  }
  NEXT();

  WS();

  left_numset = tok_parse_numset();
  if (!left_numset) {
    REWIND(begin);
    return NULL;
  }

  WS();

  if (!IS_NEXT(",")) {
    REWIND(begin);
    return NULL;
  }
  NEXT();

  WS();

  right_charset = tok_parse_charset();
  if (!right_charset) {
    REWIND(begin);
    return NULL;
  }

  WS();

  if (!IS_NEXT(")")) {
    REWIND(begin);
    return NULL;
  }
  NEXT();


  INIT("pair");
  ASTNode_addChild(left_numset);
  ASTNode_addChild(right_charset);

  return node;
}

static inline ASTNode* tok_parse_LitDef(tokparser_ctx *ctx) {
  RECORD(begin);

  // Consume starting "
  if (!IS_NEXT("\"")) {
    REWIND(begin);
    return NULL;
  }
  NEXT();

  // Read the contents
  list_codepoint_t cps = list_codepoint_t_new();
  while (!IS_NEXT("\"")) {
    if (!HAS_CURRENT())
      return NULL;

    codepoint_t c = tok_parse_char(ctx);
    if (!c) {
      list_codepoint_t_clear(cps);
      REWIND(begin);
      return NULL;
    }

    list_codepoint_t_append(&cps, c);

  }

  NEXT(); // "

  WS();

  if (!IS_NEXT(";")) {
    list_codepoint_t_clear(cps);
    REWIND(begin);
    return NULL;
  }

  // Return a node with the list of codepoints
  INIT("LitDef");
  node->extra = malloc(sizeof(list_codepoint_t));
  if (!node->extra)
    OOM();
  *(list_codepoint_t*)(node->extra) = cps;
  return node;
}

static inline ASTNode* tok_parse_SMDef(tokparser_ctx *ctx) {

}

static inline ASTNode* tok_parse_TokenDef(tokparser_ctx *ctx) {

  RECORD(begin);

  ASTNode* id = tok_parse_ident(ctx);
  if (!id) {
    REWIND(begin);
    return NULL;
  }

  WS();

  if (!IS_NEXT(":")) {
    REWIND(begin);
    return NULL;
  }
  NEXT();

  WS();

  ASTNode* rule = tok_parse_LitDef(ctx);
  if (rule) {
    INIT("tokendef");
    node->extra = rule;
    return node;
  }

  rule = tok_parse_SMDef(ctx);
  if (rule) {
    INIT("tokendef");
    node->extra = rule;
    return node;
  }

  REWIND(begin);
  return NULL;
}

static inline ASTNode* tok_parse_TokenFile(tokparser_ctx *ctx) {

  INIT();

  while(1) {
    WS();

    ASTNode* def = tok_parse_TokenDef(ctx);
    if (!def)
      break;

    ASTNode_addChild(node, def);
  }

  WS();

  if (HAS_CURRENT())
    ERROR("Could not consume all the input.");

  return node;
}


#endif /* PGEN_INCLUDE_PARSER */
