#ifndef PGEN_INCLUDE_PARSER
#define PGEN_INCLUDE_PARSER
#include "ast.h"
#include "util.h"

LIST_DECLARE(codepoint_t);
LIST_DEFINE(codepoint_t);

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
#define IS_CURRENT(str) tok_is_current(ctx, str)
#define RECORD(id) size_t _rew_to_##id = ctx->pos
#define REWIND(id) (ctx->pos = _rew_to_##id)
#define WS() tok_parse_ws(ctx)
#define INIT(name) ASTNode *node = ASTNode_new(name)

#define RULE_BEGIN(rulename)                                                   \
  /* Can't be in a do block. Oh well. */                                       \
  tok_rule_debug(0, (rulename), ctx);                                          \
  RECORD(begin);                                                               \
  const char *_rulename = (rulename)
#define RULE_SUCCESS() tok_rule_debug(1, _rulename, ctx)
#define RULE_FAIL() tok_rule_debug(-1, _rulename, ctx)

#define RETURN(ret)                                                            \
  do {                                                                         \
    if (ret)                                                                   \
      RULE_SUCCESS();                                                          \
    else                                                                       \
      RULE_FAIL();                                                             \
    return (ret);                                                              \
  } while (0)

#define DEBUG 1
#if DEBUG
static inline void print_unconsumed(tokparser_ctx *ctx) {
  Codepoint_String_View cpsv;
  cpsv.str = ctx->str + ctx->pos;
  cpsv.len = ctx->len - ctx->pos;
  printCodepointStringView(cpsv);
}

static inline void tok_rule_debug(int status, const char *rulename,
                                  tokparser_ctx *ctx) {

  if (strcmp(rulename, "ws") == 0) return;

  if (status == 0) {
    printf("\x1b[34m"); // Blue
  } else if (status == 1) {
    printf("\x1b[32m"); // Green
  } else {
    printf("\x1b[31m"); // Red
  }
  printf("%s\x1b[0m", rulename); // Rule name, clear coloring.

  puts("");

#if 0
  print_unconsumed(ctx);
  getchar();
  printf("\x1b[2J"); // clear screen
  printf("\x1b[H"); // cursor to top left corner
#endif
}
#else
static inline void tok_rule_debug(int status, const char *rulename,
                                  tokparser_ctx *ctx) {
  (void)status;
  (void)rulename;
  (void)ctx;
}
#endif

static inline bool tok_is_current(tokparser_ctx *ctx, const char *s) {
  size_t len = strlen(s);
  if (!HAS_REMAINING(len))
    return 0;

  size_t idx = ctx->pos;
  for (size_t i = 0; i < len; i++)
    if ((codepoint_t)s[i] != ctx->str[idx++])
      return 0;

  return 1;
}

/*************************/
/* Parser Implementation */
/*************************/

static inline void tok_parse_ws(tokparser_ctx *ctx) {
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
      // Found "*/"
      if (HAS_REMAINING(strlen(ecom))) {
        ADVANCE(strlen(ecom));
      } else { // EOF
        ERROR("Unterminated multi-line comment.");
      }
    } else // No more whitespace to munch
      break;
  }

  RULE_SUCCESS();
}

// The same as codepoint_atoi, but also advances by the amount read.
static inline int tok_parse_num(tokparser_ctx *ctx, size_t *read) {

  RULE_BEGIN("num");

  int i = codepoint_atoi(&CURRENT(), REMAINING(), read);
  ADVANCE(*read);

  if (read)
    RULE_SUCCESS();
  else
    RULE_FAIL();

  return i;
}

static inline ASTNode *tok_parse_ident(tokparser_ctx *ctx) {

  RULE_BEGIN("ident");

  codepoint_t *beginning = &CURRENT();

  while (1) {
    if (!HAS_CURRENT()) {
      break;
    }
    codepoint_t c = CURRENT();
    if ((c < 'A' || c > 'Z') && c != '_')
      break;
    else
      NEXT();
  }

  if (&CURRENT() == beginning)
    RETURN(NULL);

  INIT("ident");
  Codepoint_String_View *cpsv;
  node->extra = cpsv =
      (Codepoint_String_View *)malloc(sizeof(Codepoint_String_View));
  if (!cpsv)
    OOM();
  cpsv->len = &CURRENT() - beginning;
  cpsv->str = beginning;
  RETURN(node);
}

static inline codepoint_t tok_parse_char(tokparser_ctx *ctx) {

  RULE_BEGIN("char");

  // Escape sequences
  if (IS_CURRENT("\\"))
    ADVANCE(strlen("\\"));

  // Normal characters
  if (HAS_CURRENT()) {
    codepoint_t ret = CURRENT();
    NEXT();
    RETURN(ret);
  }

  // EOF
  RETURN(0);
}

static inline ASTNode *tok_parse_numset(tokparser_ctx *ctx) {

  RULE_BEGIN("numset");

  // Try to parse a number
  size_t advanced_by;
  int simple = tok_parse_num(ctx, &advanced_by);
  if (advanced_by) {
    INIT("num");
    node->extra = malloc(sizeof(codepoint_t));
    if (!node->extra)
      OOM();
    *(int *)(node->extra) = simple;
    RETURN(node);
  }

  // Parse the first bit of either of the next two options.
  if (!IS_CURRENT("("))
    RETURN(NULL);
  NEXT();

  WS();

  RECORD(checkpoint);

  // Try to parse a num range
  int range1 = tok_parse_num(ctx, &advanced_by);
  if (advanced_by) {
    WS();

    if (IS_CURRENT("-")) {
      NEXT();

      WS();

      int range2 = tok_parse_num(ctx, &advanced_by);
      if (!advanced_by) {
          REWIND(begin);
          RETURN(NULL);
      }

      WS();

      if (!IS_CURRENT(")")) {
        REWIND(begin);
        RETURN(NULL);
      }
      NEXT();

      INIT("numrange");
      int *iptr;
      node->extra = iptr = (int *)malloc(sizeof(int) * 2);
      if (!iptr) OOM();
      *iptr = range1;
      *(iptr + 1) = range2;
      RETURN(node);
    }
  }

  REWIND(checkpoint);

  // Try to parse a numset list
  ASTNode *first = tok_parse_numset(ctx);
  if (!first) {
    REWIND(begin);
    RETURN(NULL);
  }

  INIT("numsetlist");
  ASTNode_addChild(node, first);

  WS();

  while (1) {

    RECORD(kleene);

    if (!IS_CURRENT(","))
      break;

    NEXT();

    WS();

    ASTNode *next = tok_parse_numset(ctx);
    if (!next) {
      REWIND(kleene);
      ASTNode_destroy(node);
      RETURN(NULL); // TODO check this.
    }
    ASTNode_addChild(node, next);

    WS();
  }

  if (!IS_CURRENT(")")) {
    REWIND(begin);
    ASTNode_destroy(node);
    RETURN(NULL);
  }
  NEXT();

  RETURN(node);
}

static inline ASTNode *tok_parse_charset(tokparser_ctx *ctx) {

  RULE_BEGIN("charset");

  // Try to parse a normal char
  if (IS_CURRENT("'")) {
    NEXT();

    codepoint_t c = tok_parse_char(ctx);
    if (!c) {
      REWIND(begin);
      RETURN(NULL);
    }

    if (!IS_CURRENT("'")) {
      REWIND(begin);
      RETURN(NULL);
    }
    NEXT();

    INIT("char");
    codepoint_t *cpptr;
    node->extra = cpptr = (codepoint_t *)malloc(sizeof(codepoint_t));
    if (!cpptr)
      OOM();
    *cpptr = c;
    RETURN(node);
  }

  // Otherwise, parse a charset.
  if (!IS_CURRENT("[")) {
    RETURN(NULL);
  }
  NEXT();

  bool is_inverted = IS_CURRENT("^");
  if (is_inverted)
    NEXT();

  INIT("charset");
  bool* bptr;
  node->extra = bptr = (bool*)malloc(sizeof(bool));
  if (!bptr) OOM();
  *bptr = is_inverted;

  int times = 0;
  while (1) {

    if (IS_CURRENT("]"))
      break;

    // Parse a char.
    codepoint_t c1 = tok_parse_char(ctx);
    codepoint_t c2 = 0;
    if (!c1) {
      REWIND(begin);
      RETURN(NULL);
    }

    // Try to make it a range.
    if (IS_CURRENT("-")) {
      NEXT();

      if (IS_CURRENT("]")) {
        REWIND(begin);
        RETURN(NULL);
      }

      c2 = tok_parse_char(ctx);
      if (!c2) {
        REWIND(begin);
        RETURN(NULL);
      }
    }

    // Parsed a single char
    ASTNode* cld = ASTNode_new("char");
    codepoint_t* cpptr;
    cld->extra = cpptr = (codepoint_t*)malloc(sizeof(codepoint_t) * (c2 ? 2 : 1));
    if (!cpptr)
      OOM();
    *cpptr = c1;
    if (c2)
      *(cpptr + 1) = c2;
    ASTNode_addChild(node, cld);

    times++;
  }

  printf("Exited. Parsed %i times.\n", times);

  if (!times) {
    REWIND(begin);
    RETURN(NULL);
  }

  if (!IS_CURRENT("]")) {
    REWIND(begin);
    RETURN(NULL);
  }
  NEXT();

  RETURN(node);
}

static inline ASTNode *tok_parse_pair(tokparser_ctx *ctx) {
  ASTNode *left_numset;
  ASTNode *right_charset;
  RULE_BEGIN("pair");

  if (!IS_CURRENT("(")) {
    RETURN(NULL);
  }
  NEXT();

  WS();

  left_numset = tok_parse_numset(ctx);
  if (!left_numset) {
    REWIND(begin);
    RETURN(NULL);
  }

  WS();

  if (!IS_CURRENT(",")) {
    REWIND(begin);
    RETURN(NULL);
  }
  NEXT();

  WS();

  right_charset = tok_parse_charset(ctx);
  if (!right_charset) {
    REWIND(begin);
    RETURN(NULL);
  }

  WS();

  if (!IS_CURRENT(")")) {
    REWIND(begin);
    RETURN(NULL);
  }
  NEXT();

  INIT("pair");
  ASTNode_addChild(node, left_numset);
  ASTNode_addChild(node, right_charset);

  RETURN(node);
}

static inline ASTNode *tok_parse_LitDef(tokparser_ctx *ctx) {

  RULE_BEGIN("litdef");

  // Consume starting "
  if (!IS_CURRENT("\"")) {
    REWIND(begin);
    RETURN(NULL);
  }
  NEXT();

  // Read the contents
  list_codepoint_t cps = list_codepoint_t_new();
  while (!IS_CURRENT("\"")) {
    if (!HAS_CURRENT())
      RETURN(NULL);

    codepoint_t c = tok_parse_char(ctx);
    if (!c) {
      list_codepoint_t_clear(&cps);
      REWIND(begin);
      RETURN(NULL);
    }

    list_codepoint_t_add(&cps, c);
  }
  NEXT(); // '\"' b/c out of while

  WS();

  if (!IS_CURRENT(";")) {
    list_codepoint_t_clear(&cps);
    REWIND(begin);
    RETURN(NULL);
  }

  // Return a node with the list of codepoints
  INIT("litdef");
  node->extra = malloc(sizeof(list_codepoint_t));
  if (!node->extra)
    OOM();
  *(list_codepoint_t *)(node->extra) = cps;
  RETURN(node);
}

static inline ASTNode *tok_parse_SMDef(tokparser_ctx *ctx) {

  RULE_BEGIN("smdef");

  ASTNode *accepting_states = tok_parse_numset(ctx);
  if (!accepting_states)
    RETURN(NULL);

  WS();

  if (!IS_CURRENT("{")) {
    REWIND(begin);
    RETURN(NULL);
  }
  NEXT();

  INIT("smdef");

  int times = 0;
  while (1) {
    WS();

    ASTNode *rule = ASTNode_new("rule");

    // Parse the transition conditons of the rule
    ASTNode *pair = tok_parse_pair(ctx);
    if (!pair) {
      ASTNode_destroy(rule);
      break;
    }
    ASTNode_addChild(rule, pair);

    WS();

    if (!IS_CURRENT("->")) {
      ASTNode_destroy(node);
      REWIND(begin);
      RETURN(NULL);
    }
    ADVANCE(2);

    WS();

    // Parse the next state of the rule
    size_t advanced_by;
    int next_state = tok_parse_num(ctx, &advanced_by);
    if (!advanced_by) {
      ASTNode_destroy(node);
      REWIND(begin);
      RETURN(NULL);
    }
    int *iptr;
    rule->extra = iptr = (int *)malloc(sizeof(int));
    if (!iptr)
      OOM();
    *iptr = next_state;

    // Consume spaces
    while (1) {
      if (IS_CURRENT(" "))
        NEXT();
      else if (IS_CURRENT("\t"))
        NEXT();
      else
        break;
    }

    // Consume either a semicolon or a newline.
    if (!IS_CURRENT(";")) {
      if (!IS_CURRENT("\n")) {
        ASTNode_destroy(node);
        REWIND(begin);
        RETURN(NULL);
      }
    }
    NEXT(); // One char, either ; or \n.

    WS();

    times++;
  }
  // Make sure we parsed at least one rule.
  if (!times) {
    ASTNode_destroy(node);
    REWIND(begin);
    RETURN(NULL);
  }

  WS();

  if (!IS_CURRENT("}")) {
    ASTNode_destroy(node);
    REWIND(begin);
    RETURN(NULL);
  }
  NEXT();

  RETURN(node);
}

static inline ASTNode *tok_parse_TokenDef(tokparser_ctx *ctx) {

  RULE_BEGIN("tokendef");

  ASTNode *id = tok_parse_ident(ctx);
  if (!id) {
    REWIND(begin);
    RETURN(NULL);
  }

  WS(); // TODO leaks

  if (!IS_CURRENT(":")) {
    REWIND(begin);
    RETURN(NULL);
  }
  NEXT();

  WS();

  ASTNode *rule = tok_parse_LitDef(ctx);
  if (!rule) {
    rule = tok_parse_SMDef(ctx);
    if (!rule) {
      REWIND(begin);
      RETURN(NULL);
    }
  }

  WS();

  if (!IS_CURRENT(";")) {
    REWIND(begin);
    RETURN(NULL);
  }
  NEXT();

  INIT("tokendef");
  ASTNode_addChild(node, id);
  ASTNode_addChild(node, rule);
  RETURN(node);
}

static inline ASTNode *tok_parse_TokenFile(tokparser_ctx *ctx) {

  RULE_BEGIN("TokenFile");

  INIT("TokenFile");

  while (1) {
    WS();

    ASTNode *def = tok_parse_TokenDef(ctx);
    if (!def)
      break;

    ASTNode_addChild(node, def);
  }

  WS();

  RETURN(node);
}

#endif /* PGEN_INCLUDE_PARSER */
