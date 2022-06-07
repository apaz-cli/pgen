#ifndef PGEN_INCLUDE_PEGPARSER
#define PGEN_INCLUDE_PEGPARSER
#include "ast.h"
#include "parserctx.h"

static inline ASTNode *peg_parse_GrammarFile(parser_ctx *ctx);
static inline ASTNode *peg_parse_Definition(parser_ctx *ctx);
static inline ASTNode *peg_parse_SlashExpr(parser_ctx *ctx);
static inline ASTNode *peg_parse_ModExprList(parser_ctx *ctx);
static inline ASTNode *peg_parse_ModExpr(parser_ctx *ctx);
static inline ASTNode *peg_parse_MatchExpr(parser_ctx *ctx);
static inline ASTNode *peg_parse_BaseExpr(parser_ctx *ctx);
static inline ASTNode *peg_parse_CodeExpr(parser_ctx *ctx);
static inline ASTNode *peg_parse_TokIdent(parser_ctx *ctx);
static inline ASTNode *peg_parse_RuleIdent(parser_ctx *ctx);

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

    ASTNode *def = peg_parse_Definition(ctx);
    if (!def)
      break;

    ASTNode_addChild(node, def);
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

// definition->children[0] is a ruleident.
// definition->children[1] is a slashexpr.
static inline ASTNode *peg_parse_Definition(parser_ctx *ctx) {

  RULE_BEGIN("Definition");

  ASTNode *id = peg_parse_RuleIdent(ctx);
  if (!id) {
    RETURN(NULL);
  }

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
  RETURN(node);
}

// Each
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

  size_t times = 0;
  while (1) {
    WS();

    ASTNode *me = peg_parse_ModExpr(ctx);
    if (!me)
      break;

    ASTNode_addChild(node, me);
    times++;
  }

  if (!times) {
    ASTNode_destroy(node);
    REWIND(begin);
    RETURN(NULL);
  }

  RETURN(node);
}

static inline ASTNode *peg_parse_ModExpr(parser_ctx *ctx) {

  RULE_BEGIN("ModExpr");
  INIT("ModExpr");

  ASTNode *labelident = peg_parse_RuleIdent(ctx);
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

  char prefix = '\0';
  if (HAS_CURRENT()) {
    if ((CURRENT() == '&') | (CURRENT() == '!')) {
      prefix = (char)CURRENT();
      NEXT();
      WS();
    }
  }

  ASTNode *bex = peg_parse_BaseExpr(ctx);
  if (!bex) {
    ASTNode_destroy(node);
    REWIND(begin);
    RETURN(NULL);
  }
  ASTNode_addChild(node, bex);

  WS();

  char suffix = '\0';
  if (HAS_CURRENT()) {
    if ((CURRENT() == '?') | (CURRENT() == '*') | (CURRENT() == '+')) {
      suffix = (char)CURRENT();
      NEXT();
    }
  }

  if (prefix | suffix) {
    char *cptr;
    node->extra = cptr = (char *)malloc(sizeof(char) * 2);
    *(cptr + 0) = prefix;
    *(cptr + 1) = suffix;
  }

  // Make the BaseExpr appear before the optional RuleIdent
  if (node->num_children == 2) {
    ASTNode *tmp = node->children[0];
    node->children[0] = node->children[1];
    node->children[1] = tmp;
  }

  RETURN(node);
}

static inline ASTNode *peg_parse_BaseExpr(parser_ctx *ctx) {

  RULE_BEGIN("BaseExpr");

  ASTNode *n = peg_parse_TokIdent(ctx);
  if (n) {
    INIT("BaseExpr");
    ASTNode_addChild(node, n);
    RETURN(node);
  }

  n = peg_parse_RuleIdent(ctx);
  if (n) {
    RECORD(before_ws);

    WS();

    if (IS_CURRENT("<-")) {
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

  size_t num_opens = 1;
  list_codepoint_t content = list_codepoint_t_new();

  while (num_opens & HAS_CURRENT()) {

    // Escape anything starting with \.
    // If a { or } is escaped, don't count it.
    codepoint_t c;
    if (CURRENT() == '\\') {
      ADVANCE(strlen("\\"));
      if (!HAS_CURRENT()) {
        list_codepoint_t_clear(&content);
        REWIND(begin);
        RETURN(NULL);
      }

      c = CURRENT();
    } else {
      if (CURRENT() == '{')
        num_opens++;
      else if (CURRENT() == '}') {
        num_opens--;
        if (!num_opens)
          break;
      }

      c = CURRENT();
    }
    NEXT();

    list_codepoint_t_add(&content, c);
  }

  INIT("CodeExpr");
  list_codepoint_t *lcpp;
  node->extra = lcpp = (list_codepoint_t *)malloc(sizeof(list_codepoint_t));
  if (!lcpp)
    OOM();
  *lcpp = content;
  RETURN(node);
}

static inline ASTNode *peg_parse_TokIdent(parser_ctx *ctx) {
  // This is a lot like peg_parse_ruleident().
  RULE_BEGIN("TokIdent");

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

  INIT("TokIdent");
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

static inline ASTNode *peg_parse_RuleIdent(parser_ctx *ctx) {

  // This is a lot like tok_parse_ident().
  RULE_BEGIN("RuleIdent");

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

  INIT("RuleIdent");
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
