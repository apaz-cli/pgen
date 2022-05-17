#ifndef PGEN_INCLUDE_PARSER
#define PGEN_INCLUDE_PARSER

#include "parserctx.h"

static inline int tok_parse_Num(parser_ctx *ctx, size_t *read);
static inline ASTNode *tok_parse_Ident(parser_ctx *ctx);
static inline codepoint_t tok_parse_Char(parser_ctx *ctx);
static inline ASTNode *tok_parse_NumSet(parser_ctx *ctx);
static inline ASTNode *tok_parse_CharSet(parser_ctx *ctx);
static inline ASTNode *tok_parse_Pair(parser_ctx *ctx);
static inline ASTNode *tok_parse_LitDef(parser_ctx *ctx);
static inline ASTNode *tok_parse_SMDef(parser_ctx *ctx);
static inline ASTNode *tok_parse_TokenDef(parser_ctx *ctx);
static inline ASTNode *tok_parse_TokenFile(parser_ctx *ctx);

/*****************************/
/* Tok Parser Implementation */
/*****************************/

// The same as codepoint_atoi, but also advances by the amount read and has
// debug info.
static inline int tok_parse_Num(parser_ctx *ctx, size_t *read) {

  RULE_BEGIN("Num");

  int i = codepoint_atoi(&CURRENT(), REMAINING(), read);
  ADVANCE(*read);

  if (read)
    RULE_SUCCESS();
  else
    RULE_FAIL();

  return i;
}

// ident->extra is a codepoint string view of the identifier.
static inline ASTNode *tok_parse_Ident(parser_ctx *ctx) {

  // This is a lot like peg_parse_ruleident().
  RULE_BEGIN("Ident");

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

  INIT("Ident");
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
static inline codepoint_t tok_parse_Char(parser_ctx *ctx) {

  RULE_BEGIN("Char");

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

// num->extra = int(int?)
// or
// num->children
static inline ASTNode *tok_parse_NumSet(parser_ctx *ctx) {

  RULE_BEGIN("NumSet");

  // Try to parse a number
  size_t advanced_by;
  int simple = tok_parse_Num(ctx, &advanced_by);
  if (advanced_by) {
    INIT("Num");
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
  int range1 = tok_parse_Num(ctx, &advanced_by);
  if (advanced_by) {
    WS();

    if (IS_CURRENT("-")) {
      NEXT();

      WS();

      int range2 = tok_parse_Num(ctx, &advanced_by);
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

      INIT("NumRange");
      int *iptr;
      node->extra = iptr = (int *)malloc(sizeof(int) * 2);
      if (!iptr)
        OOM();
      *iptr = MIN(range1, range2);
      *(iptr + 1) = MAX(range1, range2);
      RETURN(node);
    }
  }

  REWIND(checkpoint);

  // Try to parse a numset list
  ASTNode *first = tok_parse_NumSet(ctx);
  if (!first) {
    REWIND(begin);
    RETURN(NULL);
  }

  INIT("NumSetList");
  ASTNode_addChild(node, first);

  WS();

  while (1) {

    RECORD(kleene);

    if (!IS_CURRENT(","))
      break;

    NEXT();

    WS();

    ASTNode *next = tok_parse_NumSet(ctx);
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
static inline ASTNode *tok_parse_CharSet(parser_ctx *ctx) {

  RULE_BEGIN("CharSet");

  // Try to parse a normal char
  if (IS_CURRENT("'")) {
    NEXT();

    codepoint_t c = tok_parse_Char(ctx);
    if (!c) {
      REWIND(begin);
      RETURN(NULL);
    }

    if (!IS_CURRENT("'")) {
      REWIND(begin);
      RETURN(NULL);
    }
    NEXT();

    INIT("Char");
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

  INIT("CharSet");
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
    codepoint_t c1 = tok_parse_Char(ctx);
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

      c2 = tok_parse_Char(ctx);
      if (!c2) {
        REWIND(begin);
        RETURN(NULL);
      }
    }

    // Parsed a single char
    ASTNode *cld = !c2 ? ASTNode_new("Char") : ASTNode_new("CharRange");
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
static inline ASTNode *tok_parse_Pair(parser_ctx *ctx) {
  ASTNode *left_numset;
  ASTNode *right_charset;
  RULE_BEGIN("Pair");

  if (!IS_CURRENT("(")) {
    RETURN(NULL);
  }
  NEXT();

  WS();

  left_numset = tok_parse_NumSet(ctx);
  if (!left_numset) {
    REWIND(begin);
    RETURN(NULL);
  }

  WS();

  if (!IS_CURRENT(",")) {
    ASTNode_destroy(left_numset);
    REWIND(begin);
    RETURN(NULL);
  }
  NEXT();

  WS();

  right_charset = tok_parse_CharSet(ctx);
  if (!right_charset) {
    ASTNode_destroy(left_numset);
    REWIND(begin);
    RETURN(NULL);
  }

  WS();

  if (!IS_CURRENT(")")) {
    ASTNode_destroy(left_numset);
    ASTNode_destroy(right_charset);
    REWIND(begin);
    RETURN(NULL);
  }
  NEXT();

  INIT("Pair");
  ASTNode_addChild(node, left_numset);
  ASTNode_addChild(node, right_charset);

  RETURN(node);
}

// litdef->extra is a null terminated codepoint string.
static inline ASTNode *tok_parse_LitDef(parser_ctx *ctx) {

  RULE_BEGIN("LitDef");

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

    codepoint_t c = tok_parse_Char(ctx);
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

  // Return a null terminated codepoint string.
  INIT("LitDef");
  codepoint_t *cpstr;
  node->extra = cpstr = (codepoint_t *)malloc(sizeof(codepoint_t) +
                                              cps.len * sizeof(codepoint_t));
  if (!cpstr)
    OOM();

  // Copy it in
  for (size_t i = 0; i < cps.len; i++)
    cpstr[i] = list_codepoint_t_get(&cps, i);
  cpstr[cps.len] = 0;

  // Clean up the list
  list_codepoint_t_clear(&cps);

  RETURN(node);
}

// smdef->children[0] is the set of accepting states.
// smdef->children[1] and onward is a list of rules.
// rule->children[0] is a pair.
// rule->extra is the next state.
static inline ASTNode *tok_parse_SMDef(parser_ctx *ctx) {

  RULE_BEGIN("SMDef");

  ASTNode *accepting_states = tok_parse_NumSet(ctx);
  if (!accepting_states)
    RETURN(NULL);

  INIT("SMDef");
  ASTNode_addChild(node, accepting_states);

  WS();

  if (!IS_CURRENT("{")) {
    ASTNode_destroy(node);
    REWIND(begin);
    RETURN(NULL);
  }
  NEXT();

  int times = 0;
  while (1) {
    WS();

    ASTNode *rule = ASTNode_new("Rule");

    // Parse the transition conditons of the rule
    ASTNode *pair = tok_parse_Pair(ctx);
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
    int next_state = tok_parse_Num(ctx, &advanced_by);
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

  RULE_BEGIN("TokenDef");

  ASTNode *id = tok_parse_Ident(ctx);
  if (!id) {
    REWIND(begin);
    RETURN(NULL);
  }

  WS();

  if (!IS_CURRENT(":")) {
    printf("%c\n", (char)CURRENT());
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

  INIT("TokenDef");
  ASTNode_addChild(node, id);
  ASTNode_addChild(node, rule);
  RETURN(node);
}

// TokenFile->children are tokendefs.
static inline ASTNode *tok_parse_TokenFile(parser_ctx *ctx) {

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

  // Make sure we parsed at least one definition and all the
  // input has been consumed.
  if ((!node->num_children) | HAS_CURRENT()) {
    ASTNode_destroy(node);
    RETURN(NULL);
  }

  RETURN(node);
}

#endif /* PGEN_INCLUDE_PARSER */
