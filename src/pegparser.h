#ifndef PGEN_INCLUDE_PEGPARSER
#define PGEN_INCLUDE_PEGPARSER
#include "ast.h"
#include "parserctx.h"
#include "utf8.h"
#include "util.h"
#include <limits.h>

typedef struct {
  ASTNode *label_name;
  unsigned char inverted : 1;
  unsigned char rewind : 1;
  unsigned char optional : 1;
  unsigned char kleene_plus : 2; // 0 for nothing, 1 for plus, 2 for kleene.
} ModExprOpts;

typedef struct {
  size_t line_nbr;
  char content[];
} CodeExprOpts;

static inline ASTNode *peg_parse_GrammarFile(parser_ctx *ctx);

static inline ASTNode *peg_parse_Directive(parser_ctx *ctx);

static inline ASTNode *peg_parse_Definition(parser_ctx *ctx);
static inline ASTNode *peg_parse_Variables(parser_ctx *ctx);
static inline ASTNode *peg_parse_SlashExpr(parser_ctx *ctx);
static inline ASTNode *peg_parse_ModExprList(parser_ctx *ctx);
static inline ASTNode *peg_parse_ModExpr(parser_ctx *ctx);
static inline ASTNode *peg_parse_BaseExpr(parser_ctx *ctx);
static inline ASTNode *peg_parse_CodeExpr(parser_ctx *ctx);

static inline ASTNode *peg_parse_TokenDef(parser_ctx *ctx);
static inline ASTNode *peg_parse_LitDef(parser_ctx *ctx);
static inline ASTNode *peg_parse_SMDef(parser_ctx *ctx);
static inline ASTNode *peg_parse_NumSet(parser_ctx *ctx);
static inline ASTNode *peg_parse_CharSet(parser_ctx *ctx);
static inline ASTNode *peg_parse_Pair(parser_ctx *ctx);

static inline ASTNode *peg_parse_UpperIdent(parser_ctx *ctx);
static inline ASTNode *peg_parse_LowerIdent(parser_ctx *ctx);
// TODO: Break out ErrString
static inline int peg_parse_Num(parser_ctx *ctx, size_t *read);
static inline codepoint_t peg_parse_Char(parser_ctx *ctx);

/*****************************/
/* PEG Parser Implementation */
/*****************************/

// grammarfile->children where each child is a Definition.
static inline ASTNode *peg_parse_GrammarFile(parser_ctx *ctx) {

  // This rule looks a lot like peg_parse_TokenFile().
  RULE_BEGIN("GrammarFile");

  INIT("GrammarFile");

  while (1) {
    WS();

    ASTNode *tld = NULL;
    if (!tld)
      tld = peg_parse_Directive(ctx);
    if (!tld)
      tld = peg_parse_Definition(ctx);
    if (!tld)
      tld = peg_parse_TokenDef(ctx);

    if (tld)
      ASTNode_addChild(node, tld);
    else
      break;
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
    while (HAS_CURRENT() &&
           (CURRENT() != '\n' ||
            (CURRENT() == '\n' && ctx->str[ctx->pos - 1] == '\\'))) {

      if (CURRENT() == '\n')
        ctx->line_nbr++;
      capture_size++;
      NEXT();
    }

    if (HAS_CURRENT() && CURRENT() == '\n') {
      NEXT();
      ctx->line_nbr++;
    }
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

    size_t newlen = 0;
    for (size_t i = 0; i < capture_size - 1;) {
      if ((cpbuf[i] != '\\') | (cpbuf[i + 1] != '\n'))
        cpbuf[newlen++] = cpbuf[i++];
      else
        i += 2;
    }
    if (cpbuf[capture_size - 1] != '\\')
      cpbuf[newlen++] = cpbuf[capture_size - 1];
    cpbuf[newlen] = '\0';

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

  ASTNode *stdef = peg_parse_Variables(ctx);

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

static inline ASTNode *peg_parse_Variables(parser_ctx *ctx) {

  RULE_BEGIN("Variables");

  if (!IS_CURRENT("<") || IS_CURRENT("<-")) {
    RETURN(NULL);
  }
  NEXT();

  INIT("Variables");

  while (1) {

    WS();

    int done = 0;
    RECORD(spos);
    (void)_rew_to_line_nbr_spos;
    while (HAS_CURRENT()) {
      char c = (char)CURRENT();

      if (c == ',') {
        done = 0;
        break;
      } else if (c == '>') {
        done = 1;
        break;
      }
      NEXT();
    }
    RECORD(epos);
    (void)_rew_to_line_nbr_epos;

    size_t diff = _rew_to_epos - _rew_to_spos;
    if (!diff) {
      NEXT();
      ASTNode_destroy(node);
      RETURN(NULL);
    }

    if (HAS_NEXT())
      NEXT();
    else
      ERROR("Unexpected end of input.");

    ASTNode *member = ASTNode_new("Member");
    ASTNode_addChild(node, member);
    char *str;
    member->extra = str = (char *)malloc(diff + 1);

    for (size_t i = 0; i < diff; i++) {
      codepoint_t c = ctx->str[_rew_to_spos + i];
      if (c > CHAR_MAX)
        ERROR("No non-ascii codepoints struct definitions.");
      str[i] = (char)c;
    }
    str[diff] = '\0';

    WS();

    if (done)
      break;
  }

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

// modexpr->children[0] is a base expression.
// Then comes the label (if present), then the error string or action (if
// present). modexpr->extra points to ModExprOpts.
static inline ASTNode *peg_parse_ModExpr(parser_ctx *ctx) {

  RULE_BEGIN("ModExpr");
  INIT("ModExpr");
  ModExprOpts *opts;
  node->extra = opts = (ModExprOpts *)malloc(sizeof(ModExprOpts));
  if (!opts)
    OOM();
  opts->inverted = 0;
  opts->rewind = 0;
  opts->optional = 0;
  opts->kleene_plus = 0;
  opts->label_name = NULL;

  ASTNode *labelident = peg_parse_LowerIdent(ctx);
  if (labelident) {
    WS();

    if (!IS_CURRENT(":")) {
      ASTNode_destroy(labelident);
      REWIND(begin);
      labelident = NULL;
    } else {
      NEXT();
    }
  }

  WS();

  while (HAS_CURRENT()) {
    if (CURRENT() == '&') {
      opts->rewind = 1;
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
    if (labelident)
      ASTNode_destroy(labelident);
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

  WS();

  ASTNode *errhandler = NULL;
  if (IS_CURRENT("|")) {
    NEXT();
    WS();

    int err = 0;
    list_codepoint_t cps = parse_codepoint_string(ctx, &err);
    if (!err) {
      errhandler = ASTNode_new("ErrString");
      codepoint_t *cpstr;
      errhandler->extra = cpstr = (codepoint_t *)malloc(
          sizeof(codepoint_t) + cps.len * sizeof(codepoint_t));
      if (!cpstr)
        OOM();
      for (size_t i = 0; i < cps.len; i++)
        cpstr[i] = list_codepoint_t_get(&cps, i);
      cpstr[cps.len] = 0;
      list_codepoint_t_clear(&cps);
    }

    if (!errhandler)
      errhandler = peg_parse_BaseExpr(ctx);

    if (!errhandler) {
      if (labelident)
        ASTNode_destroy(labelident);
      ASTNode_destroy(node);
      REWIND(begin);
      RETURN(NULL);
    }
  }

  if (labelident)
    ASTNode_addChild(node, labelident);
  if (errhandler)
    ASTNode_addChild(node, errhandler);

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
    } else if (CURRENT() == '{') {
      num_opens++;
    } else if (CURRENT() == '}') {
      num_opens--;
      if (!num_opens) {
        NEXT();
        break;
      }
    }

    if (CURRENT() == '\n') {
      ctx->line_nbr++;
    }

    c = CURRENT();
    NEXT();

    list_codepoint_t_add(&content, c);
  }

  size_t diff = ctx->pos - _rew_to_sv_start - 1;

  INIT("CodeExpr");
  CodeExprOpts *opts = (CodeExprOpts *)malloc(sizeof(size_t) + diff + 1);
  node->extra = opts;
  opts->line_nbr = _rew_to_line_nbr_sv_start;
  char *str = opts->content;
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

// tokendef->children[0] is an ident
// tokendef->children[1] is a litdef or smdef.
static inline ASTNode *peg_parse_TokenDef(parser_ctx *ctx) {

  RULE_BEGIN("TokenDef");

  ASTNode *id = peg_parse_UpperIdent(ctx);
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

  ASTNode *rule = peg_parse_LitDef(ctx);
  if (!rule) {
    rule = peg_parse_SMDef(ctx);
    if (!rule) {
      ASTNode_destroy(id);
      REWIND(begin);
      RETURN(NULL);
    }
  }

  WS();

  // Consume either a semicolon or a newline.
  if (!IS_CURRENT(";") && !IS_CURRENT("\n")) {
    ASTNode_destroy(id);
    ASTNode_destroy(rule);
    REWIND(begin);
    RETURN(NULL);
  }
  NEXT(); // One char, either ; or \n.

  INIT("TokenDef");
  ASTNode_addChild(node, id);
  ASTNode_addChild(node, rule);
  RETURN(node);
}

// litdef->extra is a null terminated codepoint string.
static inline ASTNode *peg_parse_LitDef(parser_ctx *ctx) {

  RULE_BEGIN("LitDef");

  // Read the contents
  int err = 0;
  list_codepoint_t cps = parse_codepoint_string(ctx, &err);
  if (err) {
    REWIND(begin);
    RETURN(NULL);
  }

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
static inline ASTNode *peg_parse_SMDef(parser_ctx *ctx) {

  RULE_BEGIN("SMDef");

  ASTNode *accepting_states = peg_parse_NumSet(ctx);
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
    ASTNode *pair = peg_parse_Pair(ctx);
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
    int next_state = peg_parse_Num(ctx, &advanced_by);
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
    if (!IS_CURRENT(";") && !IS_CURRENT("\n")) {
      ASTNode_destroy(node);
      REWIND(begin);
      RETURN(NULL);
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

// num->extra = int(int?)
// or
// num->children
static inline ASTNode *peg_parse_NumSet(parser_ctx *ctx) {

  RULE_BEGIN("NumSet");

  // Try to parse a number
  size_t advanced_by;
  int simple = peg_parse_Num(ctx, &advanced_by);
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
  int range1 = peg_parse_Num(ctx, &advanced_by);
  if (advanced_by) {
    WS();

    if (IS_CURRENT("-")) {
      NEXT();

      WS();

      int range2 = peg_parse_Num(ctx, &advanced_by);
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
      *(iptr + 0) = MIN(range1, range2);
      *(iptr + 1) = MAX(range1, range2);
      RETURN(node);
    }
  }

  REWIND(checkpoint);

  // Try to parse a numset list
  ASTNode *first = peg_parse_NumSet(ctx);
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

    ASTNode *next = peg_parse_NumSet(ctx);
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

// If the charset is a single quoted char, then char->num_children is 0 and
// char->extra is that character's codepoint. If the charset is of
// the form [^? ...] then charset->extra is a bool* of whether
// the ^ is present, and charset->children has the range's contents.
// The children are of the form char->extra = codepoint or
// charrange->extra = codepoint[2]. A charset of the
// form [^? ...] may have no children.
static inline ASTNode *peg_parse_CharSet(parser_ctx *ctx) {

  RULE_BEGIN("CharSet");

  // Try to parse a normal char
  if (IS_CURRENT("'")) {
    NEXT();

    codepoint_t c = peg_parse_Char(ctx);
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

  while (1) {

    if (IS_CURRENT("]"))
      break;

    // Parse a char.
    codepoint_t c1 = peg_parse_Char(ctx);
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

      c2 = peg_parse_Char(ctx);
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
static inline ASTNode *peg_parse_Pair(parser_ctx *ctx) {
  ASTNode *left_numset;
  ASTNode *right_charset;
  RULE_BEGIN("Pair");

  if (!IS_CURRENT("(")) {
    RETURN(NULL);
  }
  NEXT();

  WS();

  left_numset = peg_parse_NumSet(ctx);
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

  right_charset = peg_parse_CharSet(ctx);
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

  // This is a lot like peg_parse_ident().
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

// The same as codepoint_atoi, but also advances by the amount read and has
// debug info.
static inline int peg_parse_Num(parser_ctx *ctx, size_t *advance_by) {

  RULE_BEGIN("Num");

  size_t iread;
  int i = codepoint_atoi(&CURRENT(), REMAINING(), &iread);
  if (!iread || iread == SIZE_MAX) {
    RULE_FAIL();
    return *advance_by = 0, 0;
  }
  *advance_by = iread;

  ADVANCE(*advance_by);

  if (advance_by)
    RULE_SUCCESS();
  else
    RULE_FAIL();

  return i;
}

#endif /* PGEN_INCLUDE_PEGPARSER */
