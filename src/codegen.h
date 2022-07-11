#ifndef TOKCODEGEN_INCLUDE
#define TOKCODEGEN_INCLUDE
#include "argparse.h"
#include "ast.h"
#include "automata.h"
#include "list.h"
#include "parserctx.h"
#include "pegparser.h"
#include "utf8.h"
#include <stdio.h>

#define NODE_NUM_FIXED 5

#define cwrite(...) fprintf(ctx->f, __VA_ARGS__)

// TODO debug messages
// TODO parser context init
// TODO move token print into codegen.
// TODO parse failure returns NULL rule.
// TODO SlashExpr and ModExprList rewind to before ModExpr on partial failure.

/*******/
/* ctx */
/*******/

#define PGEN_PREFIX_LEN 8
typedef struct {
  FILE *f;
  Args args;
  ASTNode *tokast;
  ASTNode *pegast;
  TrieAutomaton trie;
  list_SMAutomaton smauts;
  char *fbuffer;
  size_t expr_cnt;
  size_t indent_cnt;
  char lower[PGEN_PREFIX_LEN];
  char upper[PGEN_PREFIX_LEN];
} codegen_ctx;

static inline void codegen_ctx_init(codegen_ctx *ctx, Args args,
                                    ASTNode *tokast, ASTNode *pegast,
                                    TrieAutomaton trie,
                                    list_SMAutomaton smauts) {
  ctx->args = args;
  ctx->tokast = tokast;
  ctx->pegast = pegast;
  ctx->trie = trie;
  ctx->smauts = smauts;
  ctx->expr_cnt = 0;
  ctx->indent_cnt = 1;

  // Check to make sure we actually have code to generate.
  if ((!trie.accepting.len) & (!smauts.len))
    fprintf(stderr, "Empty tokenizer file. Exiting."), exit(1);

  // Parse prefix from tokenizer file name.
  char *pref_start = args.tokenizerTarget;
  size_t tokstart = strlen(args.tokenizerTarget);

  // Figure out where to start parsing from.
  // Backtrack to the last /.
  while (1) {
    if (!tokstart)
      break;
    if (args.tokenizerTarget[--tokstart] == '/') {
      pref_start = &(args.tokenizerTarget[tokstart + 1]);
      break;
    }
  }

  // Parse the prefix
  size_t i = 0;
  for (; i < PGEN_PREFIX_LEN - 1; i++) {
    // File name is expected in [_a-zA-Z].
    // Any other characters will cause parsing the prefix to exit.
    char low = pref_start[i];
    char up = pref_start[i];

    // [_a-zA-Z] to [_a-z].
    if ((low >= 'A') & (low <= 'Z'))
      low -= ('A' - 'a');

    // [_a-zA-Z] to [_A-Z].
    if ((up >= 'a') & (up <= 'z'))
      up += ('A' - 'a');

    // Copy up to the first invalid character
    // If it's been hit, copy the null terminator.
    if ((low != '_') & ((low < 'a') | (low > 'z')) &
        ((low < '0') | (low > '9')))
      break;

    ctx->lower[i] = low;
    ctx->upper[i] = up;
  }
  ctx->lower[i] = '\0';
  ctx->upper[i] = '\0';

  // Create/open the file prefix.h if -o was not an argument,
  // otherwise the -o target.
  char namebuf[PGEN_PREFIX_LEN + 2]; // Doesn't overflow.
  char *write_to;
  if (!args.outputTarget) {
    sprintf(namebuf, "%s.h", ctx->lower);
    write_to = namebuf;
  } else {
    write_to = args.outputTarget;
  }

  ctx->f = fopen(write_to, "w");
  if (!ctx->f)
    ERROR("Could not write to %s.", namebuf);

  // Set unbuffered.
  size_t bufsz = 4096 * 50;
  ctx->fbuffer = (char *)malloc(bufsz);
  if (ctx->fbuffer)
    setvbuf(ctx->f, ctx->fbuffer, _IOFBF, bufsz);
}

static inline void codegen_ctx_destroy(codegen_ctx *ctx) {
  fclose(ctx->f);
  if (ctx->fbuffer)
    free(ctx->fbuffer);

  ASTNode_destroy(ctx->tokast);
  if (ctx->pegast)
    ASTNode_destroy(ctx->pegast);
}

/****************/
/* UTF8 Library */
/****************/
static inline void write_utf8_lib(codegen_ctx *ctx) {
#include "strutf8.xxd"
  cwrite("%s", (char *)src_utf8_h);
  cwrite("\n\n");
}

/************************/
/* AST Memory Allocator */
/************************/
static inline void write_arena_lib(codegen_ctx *ctx) {
#include "strarena.xxd"
  cwrite("%s", (char *)src_arena_h);
  cwrite("\n\n");
}

/************************/
/* Parser Helper Macros */
/************************/
static inline void write_helpermacros(codegen_ctx *ctx) {
  cwrite("#ifndef PGEN_PARSER_MACROS_INCLUDED\n");
  cwrite("#define PGEN_PARSER_MACROS_INCLUDED\n");
  cwrite("#define PGEN_CAT_(x, y) x##y\n");
  cwrite("#define PGEN_CAT(x, y) PGEN_CAT_(x, y)\n");
  cwrite("#define PGEN_NARG(...) "
         "PGEN_NARG_(__VA_ARGS__, PGEN_RSEQ_N())\n");
  cwrite("#define PGEN_NARG_(...) PGEN_128TH_ARG(__VA_ARGS__)\n");

  cwrite("#define PGEN_128TH_ARG(                 "
         "                                       \\\n");
  cwrite("    _1, _2, _3, _4, _5, _6, _7, _8, _9, "
         "_10, _11, _12, _13, _14, _15, _16,     \\\n");
  cwrite("    _17, _18, _19, _20, _21, _22, _23, "
         "_24, _25, _26, _27, _28, _29, _30, _31, \\\n");
  cwrite("    _32, _33, _34, _35, _36, _37, _38, "
         "_39, _40, _41, _42, _43, _44, _45, _46, \\\n");
  cwrite("    _47, _48, _49, _50, _51, _52, _53, "
         "_54, _55, _56, _57, _58, _59, _60, _61, \\\n");
  cwrite("    _62, _63, _64, _65, _66, _67, _68, "
         "_69, _70, _71, _72, _73, _74, _75, _76, \\\n");
  cwrite("    _77, _78, _79, _80, _81, _82, _83, "
         "_84, _85, _86, _87, _88, _89, _90, _91, \\\n");
  cwrite("    _92, _93, _94, _95, _96, _97, _98, "
         "_99, _100, _101, _102, _103, _104,      \\\n");
  cwrite("    _105, _106, _107, _108, _109, _110, "
         "_111, _112, _113, _114, _115, _116,    \\\n");
  cwrite("    _117, _118, _119, _120, _121, _122, "
         "_123, _124, _125, _126, _127, N, ...)  \\\n");
  cwrite("  N\n");

  cwrite("#define PGEN_RSEQ_N()                                       "
         "                   \\\n");
  cwrite("  127, 126, 125, 124, 123, 122, 121, 120,"
         " 119, 118, 117, 116, 115, 114, 113,   \\\n");
  cwrite("      112, 111, 110, 109, 108, 107, 106, "
         "105, 104, 103, 102, 101, 100, 99, 98, \\\n");
  cwrite("      97, 96, 95, 94, 93, 92, 91, 90, 89,"
         " 88, 87, 86, 85, 84, 83, 82, 81, 80,  \\\n");
  cwrite("      79, 78, 77, 76, 75, 74, 73, 72, 71,"
         " 70, 69, 68, 67, 66, 65, 64, 63, 62,  \\\n");
  cwrite("      61, 60, 59, 58, 57, 56, 55, 54, 53,"
         " 52, 51, 50, 49, 48, 47, 46, 45, 44,  \\\n");
  cwrite("      43, 42, 41, 40, 39, 38, 37, 36, 35,"
         " 34, 33, 32, 31, 30, 29, 28, 27, 26,  \\\n");
  cwrite("      25, 24, 23, 22, 21, 20, 19, 18, 17,"
         " 16, 15, 14, 13, 12, 11, 10, 9, 8, 7, \\\n");
  cwrite("      6, 5, 4, 3, 2, 1, 0\n");
  cwrite("#endif /* PGEN_PARSER_MACROS_INCLUDED */\n\n");
}

/*************/
/* Tokenizer */
/*************/

static inline void tok_write_header(codegen_ctx *ctx) {
  cwrite("#ifndef %s_TOKENIZER_INCLUDE\n"
         "#define %s_TOKENIZER_INCLUDE\n"
         "\n"
         "#ifndef %s_TOKENIZER_SOURCEINFO\n"
         "#define %s_TOKENIZER_SOURCEINFO 1\n"
         "#endif\n"
         "\n",
         ctx->upper, ctx->upper, ctx->upper, ctx->upper);
}

static inline void tok_write_enum(codegen_ctx *ctx) {
  cwrite("typedef enum {\n");

  size_t num_defs = ctx->tokast->num_children;
  cwrite("  %s_TOK_STREAMEND,\n", ctx->upper);
  for (size_t i = 0; i < num_defs; i++)
    cwrite("  %s_TOK_%s,\n", ctx->upper,
           (char *)(ctx->tokast->children[i]->children[0]->extra));

  cwrite("} %s_token_kind;\n\n", ctx->lower);

  cwrite("// The 0th token is end of stream.\n// Tokens 1 through %zu are the "
         "ones you defined.\n// This totals %zu kinds of tokens.\n",
         num_defs, num_defs + 1);
  cwrite("#define %s_NUM_TOKENKINDS %zu\n", ctx->upper, num_defs + 1);
  cwrite("static const char* %s_tokenkind_name[%s_NUM_TOKENKINDS] = {\n  "
         "\"%s_TOK_STREAMEND\",\n",
         ctx->lower, ctx->upper, ctx->upper);
  for (size_t i = 0; i < num_defs; i++)
    cwrite("  \"%s_TOK_%s\",\n", ctx->upper,
           (char *)(ctx->tokast->children[i]->children[0]->extra));
  cwrite("};\n\n");
}

static inline void tok_write_tokenstruct(codegen_ctx *ctx) {
  cwrite("typedef struct {\n"
         "  %s_token_kind kind;\n"
         "  size_t start; // The token begins at "
         "tokenizer->start[token->start].\n"
         "  size_t len;   // It goes until tokenizer->start[token->start + "
         "token->len] (non-inclusive).\n"
         "#if %s_TOKENIZER_SOURCEINFO\n"
         "  size_t line;\n"
         "  size_t col;\n"
         "  char* sourceFile;\n"
         "#endif\n"
         "#ifdef %s_TOKEN_EXTRA\n"
         "  %s_TOKEN_EXTRA\n"
         "#endif\n"
         "} %s_token;\n\n",
         ctx->lower, ctx->upper, ctx->upper, ctx->upper, ctx->lower);
}

static inline void tok_write_ctxstruct(codegen_ctx *ctx) {
  cwrite("typedef struct {\n"
         "  codepoint_t* start;\n"
         "  size_t len;\n"
         "  size_t pos;\n"
         "#if %s_TOKENIZER_SOURCEINFO\n"
         "  size_t pos_line;\n"
         "  size_t pos_col;\n"
         "  char* pos_sourceFile;\n"
         "#endif\n"
         "} %s_tokenizer;\n\n",
         ctx->upper, ctx->lower);

  cwrite("static inline void %s_tokenizer_init(%s_tokenizer* tokenizer, "
         "codepoint_t* start, size_t len, char* sourceFile) {\n"
         "  tokenizer->start = start;\n"
         "  tokenizer->len = len;\n"
         "  tokenizer->pos = 0;\n"
         "#if %s_TOKENIZER_SOURCEINFO\n"
         "  tokenizer->pos_line = 0;\n"
         "  tokenizer->pos_col = 0;\n"
         "  tokenizer->pos_sourceFile = sourceFile;\n"
         "#else\n"
         "  (void)sourceFile;\n"
         "#endif\n"
         "}\n\n",
         ctx->lower, ctx->lower, ctx->upper);
}

static inline void tok_write_statecheck(codegen_ctx *ctx, size_t smaut_num,
                                        list_int states) {

  // Single int
  if (states.len == 1) {
    cwrite("smaut_state_%zu == %i", smaut_num, states.buf[0]);
  }

  // Charset
  else {
    cwrite("(smaut_state_%zu == %i)", smaut_num, states.buf[0]);
    for (size_t i = 1; i < states.len; i++)
      cwrite(" | (smaut_state_%zu == %i)", smaut_num, states.buf[i]);
  }
}

static inline void tok_write_charsetcheck(codegen_ctx *ctx, ASTNode *charset) {

  // Single char
  if (!charset->num_children) {
    if (strcmp(charset->name, "CharSet") == 0) {
      cwrite("%i", (int)*(bool *)charset->extra);
    } else { // single char
      cwrite("c == %" PRI_CODEPOINT "", *(codepoint_t *)charset->extra);
    }
  }

  // Charset
  else {
    bool inverted = *(bool *)charset->extra;
    if (!charset->num_children) {
      cwrite("%i", (int)inverted);
      return;
    }

    if (inverted)
      cwrite("!(");

    for (size_t i = 0; i < charset->num_children; i++) {
      ASTNode *child = charset->children[i];
      if (strcmp(child->name, "Char") == 0) {
        codepoint_t c = *(codepoint_t *)child->extra;
        if (charset->num_children == 1) {
          // If it's the only one, it doensn't need extra parens.
          cwrite("c == %" PRI_CODEPOINT, c);
        } else {
          cwrite("(c == %" PRI_CODEPOINT ")", c);
        }
      } else {
        codepoint_t *cpptr = (codepoint_t *)child->extra;
        codepoint_t r1 = *cpptr, r2 = *(cpptr + 1);
        codepoint_t c1 = MIN(r1, r2), c2 = MAX(r1, r2);
        cwrite("((c >= %" PRI_CODEPOINT ") & (c <= %" PRI_CODEPOINT "))", c1,
               c2);
      }
      if (i != charset->num_children - 1)
        cwrite(" | ");
    }

    if (inverted)
      cwrite(")");
  }
}

static inline void tok_write_nexttoken(codegen_ctx *ctx) {
  // See tokenizer.txt.

  TrieAutomaton trie = ctx->trie;
  list_SMAutomaton smauts = ctx->smauts;
  int has_trie = trie.accepting.len ? 1 : 0;
  int has_smauts = smauts.len ? 1 : 0;

  cwrite("static inline %s_token %s_nextToken(%s_tokenizer* tokenizer) {\n"
         "  codepoint_t* current = tokenizer->start + tokenizer->pos;\n"
         "  size_t remaining = tokenizer->len - tokenizer->pos;\n\n",
         ctx->lower, ctx->lower, ctx->lower);

  // Variables for each automaton for the current run.
  if (has_trie)
    cwrite("  int trie_state = 0;\n");
  if (has_smauts) {
    for (size_t i = 0; i < smauts.len; i++)
      cwrite("  int smaut_state_%zu = 0;\n", i);
  }

  if (has_trie)
    cwrite("  size_t trie_munch_size = 0;\n");
  if (has_smauts) {
    for (size_t i = 0; i < smauts.len; i++)
      cwrite("  size_t smaut_munch_size_%zu = 0;\n", i);
  }

  if (has_trie)
    cwrite("  %s_token_kind trie_tokenkind = %s_TOK_STREAMEND;\n\n", ctx->lower,
           ctx->upper);
  else
    cwrite("\n");

  // Outer loop
  cwrite("  for (size_t iidx = 0; iidx < remaining; iidx++) {\n");
  cwrite("    codepoint_t c = current[iidx];\n");
  cwrite("    int all_dead = 1;\n\n");

  // Inner loop (automaton, unrolled)
  // Trie aut
  if (has_trie) {
    cwrite("    // Trie\n");
    cwrite("    if (trie_state != -1) {\n");
    cwrite("      all_dead = 0;\n");

    // Group runs of to.
    int eels = 0;
    for (size_t i = 0; i < trie.trans.len;) {
      int els = 0;
      int from = trie.trans.buf[i].from;
      cwrite("      %sif (trie_state == %i) {\n", eels++ ? "else " : "", from);
      while (i < trie.trans.len && trie.trans.buf[i].from == from) {
        codepoint_t c = trie.trans.buf[i].c;
        int to = trie.trans.buf[i].to;
        const char *lsee = els++ ? "else " : "";
        if (c == '\n')
          cwrite("        %sif (c == %" PRI_CODEPOINT " /*'\\n'*/"
                 ") trie_state = %i;\n",
                 lsee, c, to);
        else if (c <= 127)
          cwrite("        %sif (c == %" PRI_CODEPOINT " /*'%c'*/"
                 ") trie_state = %i;\n",
                 lsee, c, (char)c, to);
        else
          cwrite("        %sif (c == %" PRI_CODEPOINT ") trie_state = %i;\n",
                 lsee, c, to);

        i++;
      }
      cwrite("        else trie_state = -1;\n");
      cwrite("      }\n");
    }
    cwrite("      else {\n");
    cwrite("        trie_state = -1;\n");
    cwrite("      }\n\n");

    eels = 0;
    cwrite("      // Check accept\n");
    for (size_t i = 0; i < trie.accepting.len; i++) {
      cwrite("      %sif (trie_state == %i) {\n", eels++ ? "else " : "",
             trie.accepting.buf[i].num);
      cwrite("        trie_tokenkind =  %s_TOK_%s;\n", ctx->upper,
             (char *)trie.accepting.buf[i].rule->children[0]->extra);
      cwrite("        trie_munch_size = iidx + 1;\n");
      cwrite("      }\n");
    }
    cwrite("    }\n\n"); // End of Trie aut
  }

  // SM auts
  if (has_smauts) {
    for (size_t a = 0; a < smauts.len; a++) {
      SMAutomaton aut = smauts.buf[a];
      cwrite("    // Transition %s State Machine\n", aut.ident);
      cwrite("    if (smaut_state_%zu != -1) {\n", a);
      cwrite("      all_dead = 0;\n\n");

      int eels = 0;
      for (size_t i = 0; i < aut.trans.len; i++) {
        SMTransition trans = list_SMTransition_get(&aut.trans, i);

        cwrite("      %sif ((", eels++ ? "else " : "");
        tok_write_statecheck(ctx, a, trans.from);
        cwrite(") &\n         (");
        tok_write_charsetcheck(ctx, trans.act);
        cwrite(")) {\n");

        cwrite("          smaut_state_%zu = %i;\n", a, trans.to);

        cwrite("      }\n");
      }
      cwrite("      else {\n");
      cwrite("        smaut_state_%zu = -1;\n", a);
      cwrite("      }\n\n");

      cwrite("      // Check accept\n");

      cwrite("      if (");
      tok_write_statecheck(ctx, a, aut.accepting);
      cwrite(") {\n");
      cwrite("        smaut_munch_size_%zu = iidx + 1;\n", a);
      cwrite("      }\n");

      cwrite("    }\n\n");
    }
  }
  cwrite("    if (all_dead)\n");
  cwrite("      break;\n");
  cwrite("  }\n\n"); // For each remaining character

  cwrite("  // Determine what token was accepted, if any.\n");
  cwrite("  %s_token_kind kind = %s_TOK_STREAMEND;\n", ctx->lower, ctx->upper);
  cwrite("  size_t max_munch = 0;\n");
  if (has_smauts) {
    for (size_t i = smauts.len; i-- > 0;) {
      SMAutomaton aut = smauts.buf[i];
      cwrite("  if (smaut_munch_size_%zu >= max_munch) {\n", i);
      cwrite("    kind = %s_TOK_%s;\n", ctx->upper, aut.ident);
      cwrite("    max_munch = smaut_munch_size_%zu;\n", i);
      cwrite("  }\n");
    }
  }
  if (has_trie) {
    cwrite("  if (trie_munch_size >= max_munch) {\n");
    cwrite("    kind = trie_tokenkind;\n");
    cwrite("    max_munch = trie_munch_size;\n");
    cwrite("  }\n");
  }
  cwrite("\n");

  cwrite("  %s_token ret;\n", ctx->lower);
  cwrite("  ret.kind = kind;\n");
  cwrite("  ret.start = tokenizer->pos;\n");
  cwrite("  ret.len = max_munch;\n\n");

  cwrite("#if %s_TOKENIZER_SOURCEINFO\n", ctx->upper);
  cwrite("  ret.line = tokenizer->pos_line;\n");
  cwrite("  ret.col = tokenizer->pos_col;\n");
  cwrite("  ret.sourceFile = tokenizer->pos_sourceFile;\n");
  cwrite("\n");
  cwrite("  for (size_t i = 0; i < ret.len; i++) {\n");
  cwrite("    if (current[i] == '\\n') {\n");
  cwrite("      tokenizer->pos_line++;\n");
  cwrite("      tokenizer->pos_col = 0;\n");
  cwrite("    } else {\n");
  cwrite("      tokenizer->pos_col++;\n");
  cwrite("    }\n");
  cwrite("  }\n");
  cwrite("#endif\n\n");

  cwrite("  tokenizer->pos += max_munch;\n");
  cwrite("  return ret;\n");
  cwrite("}\n\n");
}

static inline void tok_write_footer(codegen_ctx *ctx) {
  cwrite("#endif /* %s_TOKENIZER_INCLUDE */\n\n", ctx->upper);
}

static inline void codegen_write_tokenizer(codegen_ctx *ctx) {

  tok_write_header(ctx);

  tok_write_enum(ctx);

  tok_write_tokenstruct(ctx);

  tok_write_ctxstruct(ctx);

  tok_write_nexttoken(ctx);

  tok_write_footer(ctx);
}

/**********/
/* Parser */
/**********/

static inline void peg_write_structs(codegen_ctx *ctx) {
  cwrite("struct %s_astnode_t;\n", ctx->lower);
  cwrite("typedef struct %s_astnode_t %s_astnode_t;\n", ctx->lower, ctx->lower);
  cwrite("\n");
  ASTNode *pegast = ctx->pegast;
  for (size_t i = 0; i < pegast->num_children; i++) {
    ASTNode *child = pegast->children[i];
    ASTNode *rident = child->children[0];
    ASTNode *strucdef = child->num_children == 3 ? child->children[2] : NULL;

    cwrite("typedef struct {\n");
    if (strucdef) {
      for (size_t j = 0; j < strucdef->num_children; j++) {
        cwrite("  %s_astnode_t* %s;\n", ctx->lower,
               (char *)strucdef->children[j]->children[0]->extra);
      }
    }
    cwrite("} %s_astnode_t;\n\n", (char *)rident->extra);
  }
}

static inline void peg_write_header(codegen_ctx *ctx) {
  cwrite("#ifndef PGEN_%s_ASTNODE_INCLUDE\n", ctx->upper);
  cwrite("#define PGEN_%s_ASTNODE_INCLUDE\n\n", ctx->upper);
}

static inline void peg_write_footer(codegen_ctx *ctx) {
  cwrite("#endif /* PGEN_%s_ASTNODE_INCLUDE */\n\n", ctx->upper);
}

static inline void peg_write_directives(codegen_ctx *ctx) {
  int oom_written = 0;

  ASTNode *pegast = ctx->pegast;
  if (pegast->num_children)
    cwrite("/**************/\n/* Directives */\n/**************/\n");
  for (size_t n = 0; n < pegast->num_children; n++) {
    ASTNode *dir = pegast->children[n];
    if (strcmp(dir->name, "Directive"))
      continue;

    // %define directive
    if (!strcmp((char *)dir->children[0]->extra, "define")) {
      cwrite("#define %s\n", (char *)dir->extra);
    }
    // %oom directive
    else if (!strcmp((char *)dir->children[0]->extra, "oom")) {
      if (oom_written)
        ERROR("Duplicate %%oom directives.");

      if (ctx->args.u) {
        fprintf(stderr,
                "PGEN error: "
                "Comment out your %%oom directive to use unsafe codegen.\n");
      } else {
        cwrite("#define PGEN_OOM() %s\n", (char *)dir->extra);
        oom_written = 1;
      }
    }
    // %include directive
    else if (!strcmp((char *)dir->children[0]->extra, "include")) {
      cwrite("#include %s\n", (char *)dir->extra);
    }
    // %code directive
    else if (!strcmp((char *)dir->children[0]->extra, "code")) {
      cwrite("%s\n", (char *)dir->extra);
    }
  }
  cwrite("\n");
}

static inline void peg_write_parser_ctx(codegen_ctx *ctx) {
  cwrite("typedef struct {\n");
  cwrite("  %s_token* tokens;\n", ctx->lower);
  cwrite("  size_t len;\n");
  cwrite("  size_t pos;\n");
  cwrite("  pgen_allocator *alloc;\n");
  cwrite("} %s_parser_ctx;\n\n", ctx->lower);
}

static inline void peg_write_parser_ctx_init(codegen_ctx *ctx) {
  cwrite("static inline void %s_parser_ctx_init(%s_parser_ctx* parser,\n"
         "                                       pgen_allocator* allocator,\n"
         "                                       %s_token* tokens, size_t "
         "num_tokens) {\n",
         ctx->lower, ctx->lower, ctx->lower);
  cwrite("  parser->tokens = tokens;\n");
  cwrite("  parser->len = num_tokens;\n");
  cwrite("  parser->pos = 0;\n");
  cwrite("  parser->alloc = allocator;\n");
  cwrite("}\n");
}

static inline void peg_write_astnode_kind(codegen_ctx *ctx) {
  size_t num_kinds = 0;
  cwrite("typedef enum {\n");
  cwrite("  %s_NODE_EMPTY,\n", ctx->lower);
  ASTNode *pegast = ctx->pegast;
  for (size_t n = 0; n < pegast->num_children; n++) {
    ASTNode *dir = pegast->children[n];
    if (strcmp(dir->name, "Directive"))
      continue;

    if (!strcmp((char *)dir->children[0]->extra, "node")) {
      char *paste = (char *)dir->extra;
      for (size_t i = 0; i < strlen(paste); i++) {
        char c = paste[i];
        if (!strcmp("EMPTY", paste)) {
          ERROR("Node kind cannot be EMPTY.");
        } else if (((c < 'A') | (c > 'Z')) & (c != '_') & (c<'0' | c> '9')) {
          ERROR("Node kind %s would not create a valid (uppercase) identifier.",
                paste);
        }
      }
      cwrite("  %s_NODE_%s,\n", ctx->upper, paste);
      num_kinds++;
    }
  }
  cwrite("} %s_astnode_kind;\n\n", ctx->lower);

  cwrite("#define %s_NUM_NODEKINDS %zu\n", ctx->upper, num_kinds + 1);
  cwrite("static const char* %s_nodekind_name[%s_NUM_NODEKINDS] = {\n  "
         "\"%s_NODE_EMPTY\",\n",
         ctx->lower, ctx->upper, ctx->upper);
  for (size_t n = 0; n < pegast->num_children; n++) {
    ASTNode *dir = pegast->children[n];
    if (strcmp(dir->name, "Directive"))
      continue;

    if (!strcmp((char *)dir->children[0]->extra, "node")) {
      char *paste = (char *)dir->extra;
      for (size_t i = 0; i < strlen(paste); i++) {
        char c = paste[i];
        if (!strcmp("EMPTY", paste)) {
          ERROR("Node kind cannot be EMPTY.");
        } else if (((c < 'A') | (c > 'Z')) & (c != '_') & (c<'0' | c> '9')) {
          ERROR("Node kind %s would not create a valid (uppercase) identifier.",
                paste);
        }
      }
      cwrite("  \"%s_NODE_%s\",\n", ctx->upper, paste);
      num_kinds++;
    }
  }
  cwrite("};\n\n");
}

static inline void peg_ensure_kind(codegen_ctx *ctx, char *kind) {
  int found = 0;
  ASTNode *pegast = ctx->pegast;
  for (size_t n = 0; n < pegast->num_children; n++) {
    ASTNode *dir = pegast->children[n];
    if (strcmp(dir->name, "Directive"))
      continue;

    if (!strcmp((char *)dir->children[0]->extra, "node")) {
      char *_kind = (char *)dir->extra;
      if (!strcmp(kind, _kind)) {
        found = 1;
        break;
      }
    }
  }

  if (!found)
    ERROR("No node kind %s has been defined with a %%node directive.", kind);
}

static inline void peg_write_astnode_def(codegen_ctx *ctx) {
  cwrite("struct %s_astnode_t;\n", ctx->lower);
  cwrite("typedef struct %s_astnode_t %s_astnode_t;\n", ctx->lower, ctx->lower);
  cwrite("struct %s_astnode_t {\n", ctx->lower);

  // Insert %extra directives.
  int inserted_extra = 0;
  ASTNode *pegast = ctx->pegast;
  for (size_t n = 0; n < pegast->num_children; n++) {
    ASTNode *dir = pegast->children[n];
    if (strcmp(dir->name, "Directive"))
      continue;

    // %extra directive
    if (!strcmp((char *)dir->children[0]->extra, "extra")) {
      if (!inserted_extra) {
        inserted_extra = 1;
        cwrite("  // Extra data in %%extra directives:\n\n");
      }
      cwrite("  %s\n", (char *)dir->extra);
    }
  }

  if (inserted_extra)
    cwrite("\n  // End of extra data.\n\n");
  else
    cwrite("  // No %%extra directives.\n\n");

  cwrite("  %s_astnode_kind kind;\n", ctx->lower);
  cwrite("  size_t num_children;\n");
  cwrite("  size_t max_children;\n");
  cwrite("  %s_astnode_t** children;\n", ctx->lower);
  cwrite("};\n\n");
}

static inline void peg_write_astnode_init(codegen_ctx *ctx) {

  cwrite("static inline %s_astnode_t* %s_astnode_list(\n", ctx->lower,
         ctx->lower);
  cwrite("                             pgen_allocator* alloc,\n"
         "                             %s_astnode_kind kind,\n"
         "                             size_t initial_size) {\n",
         ctx->lower);
  cwrite("  char* ret = pgen_alloc(alloc,\n"
         "                         sizeof(%s_astnode_t),\n"
         "                         _Alignof(%s_astnode_t));\n",
         ctx->lower, ctx->lower);
  if (!ctx->args.u)
    cwrite("  if (!ret) PGEN_OOM();\n");
  cwrite("  %s_astnode_t *node = (pl0_astnode_t*)ret;\n\n", ctx->lower);
  cwrite("  %s_astnode_t **children;\n", ctx->lower);
  cwrite("  if (initial_size) {\n");
  cwrite("    children = (%s_astnode_t**)"
         "malloc(sizeof(%s_astnode_t*) * initial_size);\n",
         ctx->lower, ctx->lower);
  if (!ctx->args.u)
    cwrite("    if (!children)\n      PGEN_OOM();\n");
  cwrite("    pgen_defer(alloc, free, children, alloc->rew);\n");
  cwrite("  } else {\n");
  cwrite("    children = NULL;\n");
  cwrite("  }\n\n");
  cwrite("  node->kind = kind;\n");
  cwrite("  node->max_children = initial_size;\n");
  cwrite("  node->num_children = 0;\n");
  cwrite("  node->children = children;\n");
  cwrite("  return node;\n");
  cwrite("}\n\n");

  cwrite("static inline %s_astnode_t* %s_astnode_leaf(\n", ctx->lower,
         ctx->lower);
  cwrite("                             pgen_allocator* alloc,\n"
         "                             %s_astnode_kind kind) {\n",
         ctx->lower);
  cwrite("  char* ret = pgen_alloc(alloc,\n"
         "                         sizeof(%s_astnode_t),\n"
         "                         _Alignof(%s_astnode_t));\n",
         ctx->lower, ctx->lower);
  if (!ctx->args.u)
    cwrite("  if (!ret) PGEN_OOM();\n");
  cwrite("  %s_astnode_t *node = (%s_astnode_t *)ret;\n", ctx->lower,
         ctx->lower);
  cwrite("  %s_astnode_t *children = NULL;\n", ctx->lower);
  cwrite("  node->kind = kind;\n");
  cwrite("  node->max_children = 0;\n");
  cwrite("  node->num_children = 0;\n");
  cwrite("  node->children = NULL;\n");
  cwrite("  return node;\n");
  cwrite("}\n\n");

  for (size_t i = 1; i <= NODE_NUM_FIXED; i++) {

    cwrite("static inline %s_astnode_t* %s_astnode_fixed_%zu(\n", ctx->lower,
           ctx->lower, i);
    cwrite("                             pgen_allocator* alloc,\n"
           "                             %s_astnode_kind kind%s",
           ctx->lower, i ? ",\n" : "");
    for (size_t j = 0; j < i; j++)
      cwrite("                             %s_astnode_t* n%zu%s", ctx->lower, j,
             j != i - 1 ? ",\n" : "");
    cwrite(") {\n");
    cwrite("  char* ret = pgen_alloc(alloc,\n"
           "                         sizeof(%s_astnode_t) +\n"
           "                         sizeof(%s_astnode_t *) * %zu,\n",
           ctx->lower, ctx->lower, i);
    cwrite("                                        "
           "_Alignof(%s_astnode_t));\n",
           ctx->lower);
    if (!ctx->args.u)
      cwrite("  if (!ret) PGEN_OOM();\n");
    cwrite("  %s_astnode_t *node = (%s_astnode_t *)ret;\n", ctx->lower,
           ctx->lower);
    cwrite("  %s_astnode_t **children = (%s_astnode_t **)(node + 1);\n",
           ctx->lower, ctx->lower);
    cwrite("  node->kind = kind;\n");
    cwrite("  node->max_children = 0;\n");
    cwrite("  node->num_children = %zu;\n", i);
    cwrite("  node->children = children;\n");
    for (size_t j = 0; j < i; j++)
      cwrite("  children[%zu] = n%zu;\n", j, j);
    cwrite("  return node;\n");
    cwrite("}\n\n");
  }
}

static inline void peg_write_parsermacros(codegen_ctx *ctx) {
  cwrite("#define rec(label)               "
         "pgen_parser_rewind_t _rew_##label = "
         "(pgen_parser_rewind_t){ctx->alloc->rew, ctx->pos};\n");
  cwrite("#define rew(label)               "
         "%s_parser_rewind(ctx, _rew_##label)\n",
         ctx->lower);
  cwrite("#define node(kind, ...)          "
         "PGEN_CAT(%s_astnode_fixed_, "
         "PGEN_NARG(__VA_ARGS__))"
         "(ctx->alloc, %s_NODE_##kind, __VA_ARGS__)\n",
         ctx->lower, ctx->upper);
  cwrite("#define list(kind)               "
         "%s_astnode_list(ctx->alloc, %s_NODE_##kind, 16)\n",
         ctx->lower, ctx->upper);
  cwrite("#define leaf(kind)               "
         "%s_astnode_leaf(ctx->alloc, %s_NODE_##kind)\n",
         ctx->lower, ctx->upper);
  cwrite("#define add(list, node)          "
         "%s_astnode_add(ctx->alloc, list, node)\n",
         ctx->lower);
  cwrite("#define defer(node, freefn, ptr) "
         "pgen_defer(ctx->alloc, freefn, ptr, node->rew)\n");
  cwrite("#define SUCC                     "
         "((%s_astnode_t*)(void*)(uintptr_t)_Alignof(%s_astnode_t))\n\n",
         ctx->lower, ctx->lower);
}

static inline void peg_write_astnode_add(codegen_ctx *ctx) {
  cwrite("static inline void %s_astnode_add("
         "pgen_allocator* alloc, %s_astnode_t *list, %s_astnode_t *node) {\n",
         ctx->lower, ctx->lower, ctx->lower);
  cwrite("  if (list->max_children == list->num_children) {\n");
  cwrite("    size_t new_max = list->max_children * 2;\n");
  cwrite("    void* old_ptr = list->children;\n");
  cwrite("    void* new_ptr = realloc(list->children, new_max);\n");
  if (!ctx->args.u)
    cwrite("    if (!new_ptr)\n      PGEN_OOM();\n");
  cwrite("    list->children = (pl0_astnode_t **)new_ptr;\n");
  cwrite("    list->max_children = new_max;\n");
  cwrite("    pgen_allocator_realloced(alloc, old_ptr, new_ptr, free);\n");
  cwrite("  }\n");
  cwrite("  list->children[list->num_children++] = node;\n");
  cwrite("}\n\n");
}

static inline void peg_write_parser_rewind(codegen_ctx *ctx) {
  cwrite("static inline void %s_parser_rewind("
         "%s_parser_ctx *ctx, pgen_parser_rewind_t rew) {\n",
         ctx->lower, ctx->lower);
  cwrite("  pgen_allocator_rewind(ctx->alloc, rew.arew);\n");
  cwrite("  ctx->pos = rew.prew;\n");
  cwrite("}\n\n");
}

static inline void peg_write_astnode_print(codegen_ctx *ctx) {
  cwrite("static inline void %s_astnode_print_h(%s_astnode_t *node, size_t "
         "depth) {\n",
         ctx->lower, ctx->lower);
  cwrite("  for (size_t i = 0; i < depth; i++) putchar(' ');\n");
  cwrite("  if (node == SUCC)\n");
  cwrite("    puts(\"ERROR, CAPTURED SUCC.\");\n");
  cwrite("  else\n");
  cwrite("    puts(pl0_nodekind_name[node->kind]);\n");
  cwrite("  for (size_t i = 0; i < node->num_children; i++)\n"
         "    %s_astnode_print_h(node->children[i], depth + 1);\n",
         ctx->lower);
  cwrite("}\n\n");

  cwrite("static inline void %s_astnode_print(%s_astnode_t *node) {\n",
         ctx->lower, ctx->lower);
  cwrite("  %s_astnode_print_h(node, 0);\n", ctx->lower);
  cwrite("}\n\n");
}

static inline void peg_write_definition_stub(codegen_ctx *ctx, ASTNode *def) {
  char *def_name = (char *)def->children[0]->extra;
  cwrite("static inline %s_astnode_t* %s_parse_%s(%s_parser_ctx* ctx);\n",
         ctx->lower, ctx->lower, def_name, ctx->lower);
}

static inline void peg_visit_add_labels(codegen_ctx *ctx, ASTNode *expr,
                                        list_cstr *idlist) {
  if (expr->num_children == 2 && !strcmp(expr->name, "ModExpr")) {
    ASTNode *label_ident = expr->children[1];
    char *idname = (char *)label_ident->extra;

    int append = 1;
    for (size_t i = 0; i < idlist->len; i++) {
      if (!strcmp(idname, idlist->buf[i])) {
        append = 0;
        break;
      }
    }
    if (append)
      list_cstr_add(idlist, idname);
  }

  for (size_t i = 0; i < expr->num_children; i++)
    peg_visit_add_labels(ctx, expr->children[i], idlist);
}

static inline void indent(codegen_ctx *ctx) {
  for (size_t i = 0; i < ctx->indent_cnt; i++) {
    cwrite("  ");
  }
}
static inline void start_block(codegen_ctx *ctx) {
  indent(ctx);
  cwrite("{\n");
  ctx->indent_cnt++;
}
static inline void end_block(codegen_ctx *ctx) {
  ctx->indent_cnt--;
  indent(ctx);
  cwrite("}\n\n");
}
static inline void start_block_0(codegen_ctx *ctx) {
  cwrite("{\n");
  ctx->indent_cnt++;
}
static inline void end_block_0(codegen_ctx *ctx) {
  ctx->indent_cnt--;
  indent(ctx);
  cwrite("}");
}
#define comment(...)                                                           \
  do {                                                                         \
    indent(ctx);                                                               \
    cwrite("// ");                                                             \
    cwrite(__VA_ARGS__);                                                       \
    cwrite("\n");                                                              \
  } while (0)

#define iwrite(...)                                                            \
  do {                                                                         \
    indent(ctx);                                                               \
    cwrite(__VA_ARGS__);                                                       \
  } while (0)

static inline void peg_write_debug_macro(codegen_ctx *ctx) {
  cwrite("#ifndef PGEN_DEBUG\n");
  cwrite("#define PGEN_DEBUG %i\n\n", (int)ctx->args.d);
  cwrite("#define PGEN_ALLOCATOR_DEBUG %i\n\n", (int)ctx->args.m);
  cwrite("#endif /* PGEN_DEBUG */\n\n");
}

static inline void peg_write_debug_stack(codegen_ctx *ctx) {

  if (ctx->args.d) {
    cwrite("#if PGEN_DEBUG\n");

    // Find max name length
    size_t max_len = 0;
    ASTNode *pegast = ctx->pegast;
    for (size_t n = 0; n < pegast->num_children; n++) {
      ASTNode *def = pegast->children[n];
      if (strcmp(def->name, "Definition"))
        continue;
      ASTNode *li = def->children[0];
      char *rulename = (char *)li->extra;
      size_t lilen = strlen(rulename);
      max_len = MAX(max_len, lilen);
    }

    ASTNode *tokast = ctx->tokast;
    for (size_t i = 0; i < tokast->num_children; i++) {
      ASTNode *def = tokast->children[i];
      ASTNode *id = def->children[0];
      char *name = (char *)id->extra;
      size_t idlen = strlen(name);
      max_len = MAX(max_len, idlen);
    }

    cwrite("#define PGEN_DEBUG_WIDTH %zu\n", max_len);

    cwrite("typedef struct {\n");
    cwrite("  const char* rule_name;\n");
    cwrite("  size_t pos;\n");
    cwrite("} dbg_entry;\n\n");

    cwrite("static struct {\n");
    cwrite("  dbg_entry rules[500];\n");
    cwrite("  size_t size;\n");
    cwrite("  int status;\n");
    cwrite("  int first;\n");
    cwrite("} dbg_stack;\n\n");

    cwrite("#include <unistd.h>\n");
    cwrite("#include <sys/ioctl.h>\n");
    cwrite("#include <string.h>\n");
    cwrite("static inline void dbg_display(%s_parser_ctx* ctx, "
           "const char* last) {\n",
           ctx->lower);
    // Zero initialized
    cwrite("  if (!dbg_stack.first) dbg_stack.first = 1;\n");
    cwrite("  else getchar();\n\n");

    cwrite("  struct winsize w;\n");
    cwrite("  ioctl(STDOUT_FILENO, TIOCGWINSZ, &w);\n");
    cwrite("  size_t width = w.ws_col;\n");
    cwrite("  size_t leftwidth = (width - (1 + 3 + 1)) / 2;\n");
    cwrite("  size_t rightwidth = leftwidth + (leftwidth %% 2);\n");
    cwrite("  size_t height = w.ws_row - 4;\n\n");

    cwrite("// Clear screen, cursor to top left\n");
    cwrite("  printf(\"\\x1b[2J\\x1b[H\");\n\n");

    cwrite("  // Write first line in color.\n");
    cwrite("  if (dbg_stack.status == -1) {\n");
    cwrite("    printf(\"\\x1b[31m\"); // Red\n");
    cwrite("    printf(\"Failed: %%s\\n\", last);\n");
    cwrite("  } else if (dbg_stack.status == 0) {\n");
    cwrite("    printf(\"\\x1b[34m\"); // Blue\n");
    cwrite("    printf(\"Entering: %%s\\n\", last);\n");
    cwrite("  } else {\n");
    cwrite("    printf(\"\\x1b[32m\"); // Green\n");
    cwrite("    printf(\"Accepted: %%s\\n\", last);\n");
    cwrite("    \n");
    cwrite("  }\n");
    cwrite("  printf(\"\\x1b[0m\"); // Clear Formatting\n\n");

    cwrite("  // Write labels and line.\n");
    cwrite("  for (size_t i = 0; i < width; i++) "
           "putchar('-');");

    cwrite("  // Write following lines\n");
    cwrite("  for (size_t i = height; i --> 0;) {\n");
    cwrite("    putchar(' ');\n\n");

    cwrite("    // Print rule stack\n");
    cwrite("    if (i < dbg_stack.size) {\n");
    cwrite("      printf(\"%%-%zus\", "
           "dbg_stack.rules[i].rule_name);\n",
           max_len);
    cwrite("    } else {\n");
    cwrite("      for (size_t sp = 0; sp < %zu; sp++) putchar(' ');\n",
           max_len);
    cwrite("    }\n\n");

    cwrite("    printf(\" | \"); // 3 Separator chars\n\n");

    cwrite("    // Print tokens\n");
    cwrite("    size_t remaining_tokens = ctx->len - ctx->pos;\n");
    cwrite("    if (i < remaining_tokens) {\n");
    cwrite("      const char* name = "
           "%s_tokenkind_name["
           "ctx->tokens[ctx->pos + i].kind];\n",
           ctx->lower);
    cwrite("      size_t ns = strlen(name);\n");
    cwrite("      size_t remaining = rightwidth - ns;\n");
    cwrite("      printf(\"%%s\", name);\n");
    cwrite("      for (size_t sp = 0; sp < remaining; sp++) putchar(' ');\n");
    cwrite("    }\n\n");

    cwrite("    putchar(' ');\n");
    cwrite("    putchar('\\n');\n");
    cwrite("  }\n");
    cwrite("}\n\n");

    cwrite("static inline void dbg_enter(%s_parser_ctx* ctx,"
           " const char* name, size_t pos) {\n",
           ctx->lower);
    cwrite("  dbg_stack.rules[dbg_stack.size++] = (dbg_entry){name, pos};\n");
    cwrite("  dbg_stack.status = 0;\n");
    cwrite("  dbg_display(ctx, name);\n");
    cwrite("}\n\n");

    cwrite("static inline void dbg_accept(%s_parser_ctx* ctx, const char* "
           "accpeting) {\n",
           ctx->lower);
    cwrite("  dbg_stack.size--;\n");
    cwrite("  dbg_stack.status = 1;\n");
    cwrite("  dbg_display(ctx, accpeting);\n");
    cwrite("}\n\n");

    cwrite("static inline void dbg_reject(%s_parser_ctx* ctx, const char* "
           "rejecting) {\n",
           ctx->lower);
    cwrite("  dbg_stack.size--;\n");
    cwrite("  dbg_stack.status = -1;\n");
    cwrite("  dbg_display(ctx, rejecting);\n");
    cwrite("}\n");
    cwrite("#endif /* PGEN_DEBUG */\n\n");
  }
}

static inline void peg_visit_write_exprs(codegen_ctx *ctx, ASTNode *expr,
                                         size_t ret_to, int capture) {

  if (!strcmp(expr->name, "SlashExpr")) {
    if (expr->num_children == 1) {
      // Forward capture
      peg_visit_write_exprs(ctx, expr->children[0], ret_to, capture);
      return;
    }

    size_t ret = ctx->expr_cnt++;
    iwrite("%s_astnode_t* expr_ret_%zu = NULL;\n\n", ctx->lower, ret);
    iwrite("rec(slash_%zu);\n\n", ret);

    for (size_t i = 0; i < expr->num_children; i++) {

      comment("SlashExpr %zu", i);
      int notlast = i != expr->num_children - 1;
      iwrite("if (!expr_ret_%zu)\n", ret);
      start_block(ctx);

      // Forward capture through to each candidate.
      // A SlashExpr can only match a single candidate.
      peg_visit_write_exprs(ctx, expr->children[i], ret, capture);

      end_block(ctx);
    }
    iwrite("// SlashExpr end\n");
    iwrite("if (!expr_ret_%zu) rew(slash_%zu);\n", ret, ret);
    iwrite("expr_ret_%zu = expr_ret_%zu;\n\n", ret_to, ret);

  } else if (!strcmp(expr->name, "ModExprList")) {
    size_t ret = ctx->expr_cnt++;
    iwrite("%s_astnode_t* expr_ret_%zu = NULL;\n", ctx->lower, ret);
    iwrite("rec(mod_%zu);\n", ret);
    if (expr->num_children == 1) {
      comment("ModExprList Forwarding");
      peg_visit_write_exprs(ctx, expr->children[0], ret, capture);
    } else {
      for (size_t i = 0; i < expr->num_children; i++) {
        comment("ModExprList %zu", i);
        if (i)
          iwrite("if (expr_ret_%zu)\n", ret);
        start_block(ctx);
        peg_visit_write_exprs(ctx, expr->children[i], ret, 0);
        end_block(ctx);
      }
    }

    iwrite("// ModExprList end\n");
    iwrite("if (!expr_ret_%zu) rew(mod_%zu);\n", ret, ret);
    if (expr->num_children == 1) {
      iwrite("expr_ret_%zu = expr_ret_%zu;\n", ret_to, ret);
    } else {
      iwrite("expr_ret_%zu = expr_ret_%zu ? SUCC : NULL;\n", ret_to, ret);
    }

  } else if (!strcmp(expr->name, "ModExpr")) {
    ModExprOpts opts = *(ModExprOpts *)expr->extra;
    if ((opts.inverted == 0) & (opts.kleene_plus == 0) & (opts.optional == 0) &
            (opts.rewinds == 0) &&
        expr->num_children == 1) {
      // Forward capture in trivial case.
      peg_visit_write_exprs(ctx, expr->children[0], ret_to, capture);
      return;
    }

    size_t ret = ctx->expr_cnt++;

    // Copy state for rewind
    if (opts.rewinds) {
      // TODO: Opting into rewinds will most likely optimize out
      // the forced failure rewind. Need to test.
      iwrite("rec(mexpr_state_%zu)\n", ret);
    }

    iwrite("%s_astnode_t* expr_ret_%zu = NULL;\n", ctx->lower, ret);

    // Get plus or kleene SUCC/NULL or normal node return into ret
    if (opts.kleene_plus == 1) {
      // Plus (match one or more)
      // This can be implemented with a do while loop.
      // Plus cannot forward capture. Which one would be returned?
      size_t times_num = ctx->expr_cnt++;
      iwrite("int plus_times_%zu = 0;\n", times_num);
      iwrite("%s_astnode_t* expr_ret_%zu = NULL;\n", ctx->lower, times_num);
      iwrite("do\n");
      start_block(ctx);
      peg_visit_write_exprs(ctx, expr->children[0], times_num, 0);
      iwrite("plus_times_%zu++;\n", times_num);
      end_block(ctx);
      iwrite("while (expr_ret_%zu);\n", times_num);
      iwrite("expr_ret_%zu = plus_times_%zu ? SUCC : NULL;\n", ret, times_num);
    } else if (opts.kleene_plus == 2) {
      // Kleene closure (match zero or more)
      // Kleene cannot forward capture either. Which one would be returned?
      iwrite("expr_ret_%zu = SUCC;\n", ret);
      iwrite("while (expr_ret_%zu)\n", ret);
      start_block(ctx);
      peg_visit_write_exprs(ctx, expr->children[0], ret, 0);
      end_block(ctx);
      iwrite("expr_ret_%zu = SUCC;\n", ret); // Always accepts
    } else {
      // No plus or Kleene
      // A BaseExpr can either be forwarded in the simple case (is a Token,
      // Rule, or Code which is only one node), or is a SlashExpr in parens.
      // In that case, we should tell the expressions below to capture the token
      // or rule.
      // Here's where we determine if there's a capture to forward.
      // start_block(ctx);
      peg_visit_write_exprs(ctx, expr->children[0], ret,
                            expr->num_children == 2);
      // end_block(ctx);
    }

    // Apply optional/inverted to ret
    if (opts.optional) {
      iwrite("// optional\n");
      iwrite("if (!expr_ret_%zu)\n", ret);
      iwrite("  expr_ret_%zu = SUCC;\n", ret);
    } else if (opts.inverted) {
      iwrite("// invert\n");
      iwrite("expr_ret_%zu = expr_ret_%zu ? NULL : SUCC;\n", ret, ret);
    }

    // Rewind if applicable
    if (opts.rewinds) {
      iwrite("// rewind\n");
      iwrite("rew(mexpr_state_%zu);\n", ret);
    }

    // Copy ret into ret_to and label if applicable
    iwrite("expr_ret_%zu = expr_ret_%zu;\n", ret_to, ret);
    if (expr->num_children == 2) {
      char *label_name = (char *)expr->children[1]->extra;
      iwrite("%s = expr_ret_%zu;\n", label_name, ret);
    }

    // return ret_to and write to label

  } else if (!strcmp(expr->name, "BaseExpr")) {
    peg_visit_write_exprs(ctx, expr->children[0], ret_to, capture);
  } else if (!strcmp(expr->name, "UpperIdent")) {
    char *tokname = (char *)expr->extra;
    if (ctx->args.d)
      iwrite("dbg_enter(ctx, \"%s\", ctx->pos);\n", tokname);
    iwrite("if (ctx->tokens[ctx->pos].kind == %s_TOK_%s) ", ctx->upper,
           tokname);
    start_block_0(ctx);
    if (capture) {
      iwrite("// Capturing %s.\n", tokname);
      iwrite("expr_ret_%zu = leaf(%s);\n", ret_to, tokname);
      // Make sure that it actually exists and the code will compile.
      peg_ensure_kind(ctx, tokname);
    } else {
      iwrite("expr_ret_%zu = SUCC; // Not capturing %s.\n", ret_to, tokname);
    }
    iwrite("ctx->pos++;\n");
    end_block_0(ctx);
    cwrite(" else ");
    start_block_0(ctx);
    iwrite("expr_ret_%zu = NULL;\n", ret_to);
    end_block(ctx);
    if (ctx->args.d)
      iwrite("if (expr_ret_%zu) dbg_accept(ctx, \"%s\"); else dbg_reject(ctx, "
             "\"%s\");\n",
             ret_to, tokname, tokname);
  } else if (!strcmp(expr->name, "LowerIdent")) {
    iwrite("expr_ret_%zu = %s_parse_%s(ctx);\n", ret_to, ctx->lower,
           (char *)expr->extra);
  } else if (!strcmp(expr->name, "CodeExpr")) {
    // No need to respect capturing for a CodeExpr.
    // The user will allocate their own with node() or list() if they want to.
    // At the start of the block we'll write SUCC to ret_to. If that's not okay,
    // they can override that on their own with NULL or node() or list() or
    // whatever.
    comment("CodeExpr");
    if (ctx->args.d)
      iwrite("dbg_enter(ctx, \"CodeExpr\", ctx->pos);\n");
    iwrite("#define ret expr_ret_%zu\n", ret_to);
    iwrite("ret = SUCC;\n\n");
    // start_block(ctx);
    iwrite("%s;\n\n", (char *)expr->extra);
    // end_block(ctx);
    if (ctx->args.d)
      iwrite("if (ret) dbg_accept(ctx, \"CodeExpr\"); else dbg_reject(ctx, "
             "\"CodeExpr\");\n");
    iwrite("#undef ret\n");
  } else {
    ERROR("UNREACHABLE ERROR. UNKNOWN NODE TYPE:\n %s\n", expr->name);
  }
}

static inline void peg_write_definition(codegen_ctx *ctx, ASTNode *def) {
  char *def_name = (char *)def->children[0]->extra;
  ASTNode *def_expr = def->children[1];
  ASTNode *current_expr;

  cwrite("static inline %s_astnode_t* %s_parse_%s(%s_parser_ctx* ctx) {\n",
         ctx->lower, ctx->lower, def_name, ctx->lower);

  // Visit labels, write variables.
  list_cstr ids = list_cstr_new();
  peg_visit_add_labels(ctx, def_expr, &ids);
  for (size_t i = 0; i < ids.len; i++)
    cwrite("  %s_astnode_t* %s = NULL;\n", ctx->lower, ids.buf[i]);
  list_cstr_clear(&ids);

  size_t ult_ret = ctx->expr_cnt++;
  size_t ret = ctx->expr_cnt++;

  cwrite("  %s_astnode_t* expr_ret_%zu = NULL;\n", ctx->lower, ret);
  cwrite("  %s_astnode_t* expr_ret_%zu = NULL;\n", ctx->lower, ult_ret);
  cwrite("  #define rule expr_ret_%zu\n\n", ult_ret);

  if (ctx->args.d)
    iwrite("dbg_enter(ctx, \"%s\", ctx->pos);\n", def_name);

  peg_visit_write_exprs(ctx, def_expr, ret, 1);

  if (ctx->args.d)
    iwrite("if (expr_ret_%zu) dbg_accept(ctx, \"%s\"); else dbg_reject(ctx, "
           "\"%s\");\n",
           ret, def_name, def_name);

  cwrite("  return expr_ret_%zu ? rule : NULL;\n", ret);
  cwrite("  #undef rule\n");
  cwrite("}\n\n");
}

static inline void peg_write_parser_body(codegen_ctx *ctx) {
  // Write stubs
  ASTNode *pegast = ctx->pegast;
  for (size_t n = 0; n < pegast->num_children; n++) {
    ASTNode *def = pegast->children[n];
    if (strcmp(def->name, "Definition"))
      continue;

    char *def_name = (char *)def->children[0]->extra;
    ASTNode *def_expr = def->children[1];
    peg_write_definition_stub(ctx, def);
  }
  cwrite("\n\n");

  // Write bodies
  for (size_t n = 0; n < pegast->num_children; n++) {
    ASTNode *def = pegast->children[n];
    if (strcmp(def->name, "Definition"))
      continue;

    char *def_name = (char *)def->children[0]->extra;
    ASTNode *def_expr = def->children[1];
    peg_write_definition(ctx, def);
  }
  cwrite("\n\n");
}

static inline void peg_write_parser(codegen_ctx *ctx) {
  peg_write_header(ctx);
  peg_write_parser_ctx(ctx);
  peg_write_parser_ctx_init(ctx);
  peg_write_astnode_kind(ctx);
  peg_write_astnode_def(ctx);
  peg_write_astnode_init(ctx);
  peg_write_astnode_add(ctx);
  peg_write_parser_rewind(ctx);
  peg_write_parsermacros(ctx);
  peg_write_astnode_print(ctx);
  peg_write_debug_stack(ctx);
  peg_write_parser_body(ctx);
  peg_write_footer(ctx);
}

/**************/
/* Everything */
/**************/

static inline void codegen_write(codegen_ctx *ctx) {
  // Write headers
  if (ctx->tokast) {
    write_utf8_lib(ctx);
  }

  peg_write_debug_macro(ctx);
  if (ctx->pegast) {
    peg_write_directives(ctx);
    write_arena_lib(ctx);
    write_helpermacros(ctx);
  }

  // Write bodies
  if (ctx->tokast)
    codegen_write_tokenizer(ctx);
  if (ctx->pegast)
    peg_write_parser(ctx);
}

#endif /* TOKCODEGEN_INCLUDE */
