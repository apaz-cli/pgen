#ifndef PGEN_INCLUDE_PARSER
#define PGEN_INCLUDE_PARSER

#include "parserctx.h"

/*************************/
/* Parser Implementation */
/*************************/

// The same as codepoint_atoi, but also advances by the amount read and has
// debug info.
static inline int tok_parse_num(parser_ctx *ctx, size_t *read) {

  RULE_BEGIN("num");

  int i = codepoint_atoi(&CURRENT(), REMAINING(), read);
  ADVANCE(*read);

  if (read)
    RULE_SUCCESS();
  else
    RULE_FAIL();

  return i;
}

// ident->extra is a codepoint string view of the identifier.
static inline ASTNode *tok_parse_ident(parser_ctx *ctx) {

  // This is a lot like peg_parse_ruleident().
  RULE_BEGIN("tokident");

  size_t startpos = ctx->pos;

  while (1) {
    if (!HAS_CURRENT()) {
      break;
    }
    codepoint_t c = CURRENT();
    if (((c < 'A') || (c > 'Z')) & (c != '_'))
      break;
    NEXT();
  }

  if (ctx->pos == startpos)
    RETURN(NULL);

  INIT("tokident");
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

// returns 0 on EOF.
static inline codepoint_t tok_parse_char(parser_ctx *ctx) {

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

// num->extra = int
// or
// num->children
static inline ASTNode *tok_parse_numset(parser_ctx *ctx) {

  RULE_BEGIN("numset");

  // Try to parse a number
  size_t advanced_by;
  int simple = tok_parse_num(ctx, &advanced_by);
  if (advanced_by) {
    INIT("num");
    int *iptr;
    node->extra = iptr = (int *)malloc(sizeof(int));
    if (!node->extra)
      OOM();
    *iptr = simple;
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
      if (!iptr)
        OOM();
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
      RETURN(NULL);
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

// If the charset is a single quoted char, char->extra is that character's
// codepoint. If the charset is of the form [^? ...] then charset->extra is a
// bool of whether the ^ is present, and charset->children has the range's
// contents. The children are of the form char->extra = codepoint or
// charrange->extra = codepoint[2].
static inline ASTNode *tok_parse_charset(parser_ctx *ctx) {

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
  bool *bptr;
  node->extra = bptr = (bool *)malloc(sizeof(bool));
  if (!bptr)
    OOM();
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
    ASTNode *cld = !c2 ? ASTNode_new("char") : ASTNode_new("charrange");
    codepoint_t *cpptr;
    cld->extra = cpptr =
        (codepoint_t *)malloc(sizeof(codepoint_t) * (c2 ? 2 : 1));
    if (!cpptr)
      OOM();
    *cpptr = c1;
    if (c2)
      *(cpptr + 1) = c2;
    ASTNode_addChild(node, cld);

    times++;
  }

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

// pair->children[0] is the numset of starting states.
// pair->children[1] is the charset of consumable characters.
static inline ASTNode *tok_parse_pair(parser_ctx *ctx) {
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

// litdef->extra is a list of codepoints.
static inline ASTNode *tok_parse_LitDef(parser_ctx *ctx) {

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

// smdef->children is a list of rules.
// rule->children[0] is a pair.
static inline ASTNode *tok_parse_SMDef(parser_ctx *ctx) {

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
    ASTNode_addChild(node, rule);

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

// tokendef->children[0] is an ident
// tokendef->children[1] is a litdef or smdef.
static inline ASTNode *tok_parse_TokenDef(parser_ctx *ctx) {

  RULE_BEGIN("tokendef");

  ASTNode *id = tok_parse_ident(ctx);
  if (!id) {
    REWIND(begin);
    RETURN(NULL);
  }

  WS();

  if (!IS_CURRENT(":")) {
    ASTNode_destroy(id);
    REWIND(begin);
    RETURN(NULL);
  }
  NEXT();

  WS();

  ASTNode *rule = tok_parse_LitDef(ctx);
  if (!rule) {
    rule = tok_parse_SMDef(ctx);
    if (!rule) {
      ASTNode_destroy(id);
      REWIND(begin);
      RETURN(NULL);
    }
  }

  WS();

  if (!IS_CURRENT(";")) {
    ASTNode_destroy(id);
    ASTNode_destroy(rule);
    REWIND(begin);
    RETURN(NULL);
  }
  NEXT();

  INIT("tokendef");
  ASTNode_addChild(node, id);
  ASTNode_addChild(node, rule);
  RETURN(node);
}

// TokenFile->children are tokendefs.
static inline ASTNode *tok_parse_TokenFile(parser_ctx *ctx) {

  RULE_BEGIN("tokenfile");

  INIT("tokenfile");

  while (1) {
    WS();

    ASTNode *def = tok_parse_TokenDef(ctx);
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

#endif /* PGEN_INCLUDE_PARSER */
