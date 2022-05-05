#ifndef PGEN_INCLUDE_PARSER
#define PGEN_INCLUDE_PARSER
#include "parserctx.h"

static inline ASTNode* tok_parse_GrammarFile(parser_ctx* ctx);
static inline ASTNode* tok_parse_Definition(parser_ctx* ctx);
static inline ASTNode* tok_parse_SlashExpr(parser_ctx* ctx);
static inline ASTNode* tok_parse_ModExpr(parser_ctx* ctx);
static inline ASTNode* tok_parse_MatchExpr(parser_ctx* ctx);
static inline ASTNode* tok_parse_BaseExpr(parser_ctx* ctx);
static inline ASTNode* tok_parse_CodeExpr(parser_ctx* ctx);
static inline ASTNode* tok_parse_GrammarFile(parser_ctx* ctx);

static inline ASTNode* peg_parse_TokIdent(parser_ctx* ctx) {
  return tok_parse_ident(ctx);
}

static inline ASTNode* peg_parse_RuleIdent(parser_ctx* ctx) {

  // This is a lot like tok_parse_ident().
  RULE_BEGIN("RuleIdent")

  size_t startpos = ctx->pos;

  while (1) {
    if (!HAS_CURRENT)
      break;
    codepoint_t c = CURRENT();
    if (((c < 'a') || (c > 'z')) & (c != '_'))
      break;
    NEXT();
  }

  // Unlike tok_parse_ident(), check here if a <- is next up.
  // If it is, don't match.
  WS();

  if (IS_CURRENT("<-")) {
    REWIND(begin);
    RETURN(NULL);
  }


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

}


#endif /* PGEN_INCLUDE_PARSER */
