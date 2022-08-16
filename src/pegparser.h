#ifndef PGEN_INCLUDE_PEGPARSER
#define PGEN_INCLUDE_PEGPARSER
#include "ast.h"
#include "parserctx.h"
#include "utf8.h"
#include "util.h"
#include <limits.h>

typedef struct {
  char inverted;
  char rewinds;
  char optional;
  char kleene_plus; // 0 for nothing, 1 for plus, 2 for kleene.
} ModExprOpts;

static inline ASTNode *peg_parse_GrammarFile(parser_ctx *ctx);
static inline ASTNode *peg_parse_Directive(parser_ctx *ctx);
static inline ASTNode *peg_parse_Definition(parser_ctx *ctx);
static inline ASTNode *peg_parse_StructDef(parser_ctx *ctx);
static inline ASTNode *peg_parse_SlashExpr(parser_ctx *ctx);
static inline ASTNode *peg_parse_ModExprList(parser_ctx *ctx);
static inline ASTNode *peg_parse_ModExpr(parser_ctx *ctx);
static inline ASTNode *peg_parse_BaseExpr(parser_ctx *ctx);
static inline ASTNode *peg_parse_CodeExpr(parser_ctx *ctx);
static inline ASTNode *peg_parse_UpperIdent(parser_ctx *ctx);
static inline ASTNode *peg_parse_LowerIdent(parser_ctx *ctx);

/*****************************/
/* PEG Parser Implementation */
/*****************************/

// grammarfile->children where each child is a Definition.
static inline ASTNode *peg_parse_GrammarFile(parser_ctx *ctx) {

  // This rule looks a lot like tok_parse_TokenFile().
  RULE_BEGIN("GrammarFile");

  INIT("GrammarFile");

  while (1) {
    WS();

    ASTNode *dir = peg_parse_Directive(ctx);
    if (!dir) {
      ASTNode *def = peg_parse_Definition(ctx);
      if (!def)
        break;
      ASTNode_addChild(node, def);
    } else {
      ASTNode_addChild(node, dir);
    }
  }

  WS();

  // Make sure we parsed at least one definition and all the
  // input has been consumed.
  if ((!node->num_children) | HAS_CURRENT()) {
    ASTNode_destroy(node);
    RETURN(NULL);
  }

  RETURN(node);
}

static inline ASTNode *peg_parse_Directive(parser_ctx *ctx) {

  RULE_BEGIN("Directive");

  if (!HAS_CURRENT())
    RETURN(NULL);
  else if (CURRENT() == '%') {
    NEXT();

    WS();

    ASTNode *id = peg_parse_LowerIdent(ctx);
    if (!id) {
      REWIND(begin);
      RETURN(NULL);
    }

    WS();

    codepoint_t *cap_start = ctx->str + ctx->pos;
    size_t capture_size = 0;
    while (HAS_CURRENT() && CURRENT() != '\n') {
      capture_size++;
      NEXT();
    }
    if (HAS_CURRENT() && CURRENT() == '\n')
      NEXT();
    if (!capture_size) {
      REWIND(begin);
      RETURN(NULL);
    }

    char *cpbuf = (char *)malloc(capture_size + 1);
    if (!cpbuf) {
      REWIND(begin);
      RETURN(NULL);
    }
    for (size_t i = 0; i < capture_size; i++)
      cpbuf[i] = (char)cap_start[i];
    cpbuf[capture_size] = '\0';

    ASTNode *dir = ASTNode_new("Directive");
    dir->extra = cpbuf;
    ASTNode_addChild(dir, id);
    RETURN(dir);
  } else {
    RETURN(NULL);
  }
}

// definition->children[0] is a lowerident.
// definition->children[1] is a slashexpr.
// definitipn->children[2] is an (optional) list structdef.
static inline ASTNode *peg_parse_Definition(parser_ctx *ctx) {

  RULE_BEGIN("Definition");

  ASTNode *id = peg_parse_LowerIdent(ctx);
  if (!id) {
    RETURN(NULL);
  }

  WS();

  ASTNode *stdef = peg_parse_StructDef(ctx);

  WS();

  if (!IS_CURRENT("<-")) {
    ASTNode_destroy(id);
    REWIND(begin);
    RETURN(NULL);
  }
  ADVANCE(strlen("<-"));

  WS();

  ASTNode *slash = peg_parse_SlashExpr(ctx);
  if (!slash) {
    ASTNode_destroy(id);
    REWIND(begin);
    RETURN(NULL);
  }

  INIT("Definition");
  ASTNode_addChild(node, id);
  ASTNode_addChild(node, slash);
  if (stdef)
    ASTNode_addChild(node, stdef);
  RETURN(node);
}

static inline ASTNode *peg_parse_StructDef(parser_ctx *ctx) {

  RULE_BEGIN("StructDef");

  if (!IS_CURRENT("<")) {
    RETURN(NULL);
  }
  NEXT();

  INIT("StructDef");

  int first = 1;
  while (1) {
    WS();

    if (!first) {
      if (!IS_CURRENT(","))
        break;
      NEXT();
    }

    WS();

    ASTNode *ident = peg_parse_LowerIdent(ctx);
    if (!ident)
      break;
    ASTNode *member = ASTNode_new("Member");
    ASTNode_addChild(node, member);
    ASTNode_addChild(member, ident);
    first = 0;

    WS();

    char isarr = 0;
    char istok = 0;
    while (IS_CURRENT(".") | IS_CURRENT("[]")) {
      if (IS_CURRENT(".")) {
        istok = true;
      } else {
        isarr = true;
        NEXT();
      }
      NEXT();
      WS();
    }

    if (isarr | istok) {
      char *cptr;
      member->extra = cptr = (char *)malloc(2);
      cptr[0] = istok;
      cptr[1] = isarr;
    }
  }

  WS();

  if (!IS_CURRENT(">")) {
    ASTNode_destroy(node);
    REWIND(begin);
    RETURN(NULL);
  }
  NEXT();

  RETURN(node);
}

// Each child is a ModExprList.
static inline ASTNode *peg_parse_SlashExpr(parser_ctx *ctx) {

  RULE_BEGIN("SlashExpr");

  ASTNode *l1 = peg_parse_ModExprList(ctx);
  if (!l1)
    RETURN(NULL);

  INIT("SlashExpr");
  ASTNode_addChild(node, l1);

  while (1) {
    RECORD(kleene);

    WS();

    if (!IS_CURRENT("/")) {
      REWIND(kleene);
      break;
    }
    NEXT();

    // ModExprList consumes starting WS
    ASTNode *l2 = peg_parse_ModExprList(ctx);
    if (!l2) {
      REWIND(kleene);
      break;
    }

    ASTNode_addChild(node, l2);
  }

  RETURN(node);
}

// modexprlist->children is a list of ModExprs.
static inline ASTNode *peg_parse_ModExprList(parser_ctx *ctx) {

  RULE_BEGIN("ModExprList");

  INIT("ModExprList");

  while (1) {
    WS();

    ASTNode *me = peg_parse_ModExpr(ctx);
    if (!me)
      break;

    ASTNode_addChild(node, me);
  }

  RETURN(node);
}

// modexpr->children[1]
static inline ASTNode *peg_parse_ModExpr(parser_ctx *ctx) {

  RULE_BEGIN("ModExpr");
  INIT("ModExpr");
  ModExprOpts *opts;
  node->extra = opts = (ModExprOpts *)malloc(sizeof(ModExprOpts));
  opts->inverted = 0;
  opts->rewinds = 0;
  opts->optional = 0;
  opts->kleene_plus = 0;

  ASTNode *labelident = peg_parse_LowerIdent(ctx);
  if (labelident) {
    WS();

    if (!IS_CURRENT(":")) {
      ASTNode_destroy(labelident);
      REWIND(begin);
    } else {
      NEXT();

      WS();

      ASTNode_addChild(node, labelident);
    }
  }

  WS();

  while (HAS_CURRENT()) {
    if (CURRENT() == '&') {
      opts->rewinds = 1;
    } else if (CURRENT() == '!') {
      opts->inverted = !opts->inverted;
    } else
      break;

    NEXT();
    WS();
  }

  WS();

  ASTNode *bex = peg_parse_BaseExpr(ctx);
  if (!bex) {
    ASTNode_destroy(node);
    REWIND(begin);
    RETURN(NULL);
  }
  ASTNode_addChild(node, bex);

  WS();

  while (HAS_CURRENT()) {
    if (CURRENT() == '?') {
      opts->optional = 1;
    } else if (CURRENT() == '*') {
      opts->kleene_plus = 2;
    } else if (CURRENT() == '+') {
      if (!opts->kleene_plus)
        opts->kleene_plus = 2;
    } else
      break;

    NEXT();
    WS();
  }

  // Make the BaseExpr appear before the optional LowerIdent
  if (node->num_children == 2) {
    ASTNode *tmp = node->children[0];
    node->children[0] = node->children[1];
    node->children[1] = tmp;
  }

  RETURN(node);
}

static inline ASTNode *peg_parse_BaseExpr(parser_ctx *ctx) {

  RULE_BEGIN("BaseExpr");

  ASTNode *n = peg_parse_UpperIdent(ctx);
  if (n) {
    INIT("BaseExpr");
    ASTNode_addChild(node, n);
    RETURN(node);
  }

  n = peg_parse_LowerIdent(ctx);
  if (n) {
    RECORD(before_ws);

    WS();

    if (IS_CURRENT("<")) {
      ASTNode_destroy(n);
      REWIND(begin);
      RETURN(NULL);
    }

    REWIND(before_ws);
    INIT("BaseExpr");
    ASTNode_addChild(node, n);
    RETURN(node);
  }

  n = peg_parse_CodeExpr(ctx);
  if (n) {
    INIT("BaseExpr");
    ASTNode_addChild(node, n);
    RETURN(node);
  }

  if (!IS_CURRENT("(")) {
    RETURN(NULL);
  }
  NEXT();

  WS();

  n = peg_parse_SlashExpr(ctx);
  if (!n) {
    REWIND(begin);
    RETURN(NULL);
  }

  WS();

  if (!IS_CURRENT(")")) {
    ASTNode_destroy(n);
    REWIND(begin);
    RETURN(NULL);
  }
  NEXT();

  INIT("BaseExpr");
  ASTNode_addChild(node, n);
  RETURN(node);
}

static inline ASTNode *peg_parse_CodeExpr(parser_ctx *ctx) {

  RULE_BEGIN("CodeExpr");

  if (!IS_CURRENT("{"))
    RETURN(NULL);
  NEXT();

  RECORD(sv_start);

  size_t num_opens = 1;
  list_codepoint_t content = list_codepoint_t_new();

  while (num_opens && HAS_CURRENT()) {

    // Escape anything starting with \.
    // If a { or } is escaped, don't count it.
    codepoint_t c;
    if (CURRENT() == '\\') {
      NEXT();
      if (!HAS_CURRENT()) {
        list_codepoint_t_clear(&content);
        REWIND(begin);
        RETURN(NULL);
      }

      c = CURRENT();
      NEXT();
    } else {
      if (CURRENT() == '{') {
        num_opens++;
      } else if (CURRENT() == '}') {
        num_opens--;
        if (!num_opens) {
          NEXT();
          break;
        }
      }

      c = CURRENT();
      NEXT();
    }

    list_codepoint_t_add(&content, c);
  }

  size_t diff = ctx->pos - _rew_to_sv_start - 1;

  INIT("CodeExpr");
  char *str;
  node->extra = str = (char *)malloc(diff + 1);
  for (size_t i = 0; i < diff; i++) {
    codepoint_t c = content.buf[i];
    if (c > CHAR_MAX)
      ERROR("No non-ascii codepoints in code blocks.");
    str[i] = (char)c;
  }
  str[diff] = '\0';
  list_codepoint_t_clear(&content);

  RETURN(node);
}

static inline ASTNode *peg_parse_UpperIdent(parser_ctx *ctx) {
  // This is a lot like peg_parse_lowerident().
  RULE_BEGIN("UpperIdent");

  size_t startpos = ctx->pos;

  while (1) {
    if (!HAS_CURRENT()) {
      break;
    }
    codepoint_t c = CURRENT();
    if (((c < 'A') | (c > 'Z')) & (c != '_'))
      break;
    NEXT();
  }

  if (ctx->pos == startpos)
    RETURN(NULL);

  INIT("UpperIdent");
  char *idstr;
  size_t idstrsize = ctx->pos - startpos;
  node->extra = idstr = (char *)malloc(idstrsize + 1);
  if (!idstr)
    OOM();

  // Copy as cstring
  for (size_t i = 0, j = startpos; j < ctx->pos; i++, j++)
    idstr[i] = (char)ctx->str[j];
  idstr[idstrsize] = '\0';

  RETURN(node);
}

static inline ASTNode *peg_parse_LowerIdent(parser_ctx *ctx) {

  // This is a lot like tok_parse_ident().
  RULE_BEGIN("LowerIdent");

  size_t startpos = ctx->pos;

  while (1) {
    if (!HAS_CURRENT())
      break;
    codepoint_t c = CURRENT();
    if (((c < 'a') || (c > 'z')) & (c != '_'))
      break;
    NEXT();
  }

  if (ctx->pos == startpos)
    RETURN(NULL);

  INIT("LowerIdent");
  char *idstr;
  size_t idstrsize = ctx->pos - startpos;
  node->extra = idstr = (char *)malloc(idstrsize + 1);
  if (!idstr)
    OOM();

  // Copy as cstring
  for (size_t i = 0, j = startpos; j < ctx->pos; i++, j++)
    idstr[i] = (char)ctx->str[j];
  idstr[idstrsize] = '\0';

  RETURN(node);
}

#endif /* PGEN_INCLUDE_PEGPARSER */
