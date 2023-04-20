#ifndef TOKCODEGEN_INCLUDE
#define TOKCODEGEN_INCLUDE
#include <assert.h>
#include <stdarg.h>
#include <stdio.h>

#include "argparse.h"
#include "ast.h"
#include "automata.h"
#include "list.h"
#include "parserctx.h"
#include "pegparser.h"
#include "symtab.h"
#include "utf8.h"

#define NODE_NUM_FIXED 5

/*******/
/* ctx */
/*******/

#define PGEN_PREFIX_LEN 8
typedef struct {
  FILE *f;
  char *fbuffer;
  ASTNode *ast;
  Args *args;
  TrieAutomaton trie;
  list_SMAutomaton smauts;
  size_t expr_cnt;
  size_t indent_cnt;
  size_t line_nbr;
  char lower[PGEN_PREFIX_LEN];
  char upper[PGEN_PREFIX_LEN];
  list_ASTNodePtr directives;
  list_ASTNodePtr definitions;
  list_ASTNodePtr tokendefs;
  list_cstr tok_kind_names;
  list_cstr peg_kind_names;
} codegen_ctx;

static inline int cwrite_inner(codegen_ctx *ctx, const char *fmt, ...) {
  size_t newlines = 0;
  va_list ap;
  va_start(ap, fmt);

#define CWRITE_INNER_BUFSZ (4096 * 4)
  char buf[CWRITE_INNER_BUFSZ];

  size_t csize = CWRITE_INNER_BUFSZ;
  char *cbuf = buf;
  int written;
  while (1) {
    va_list cpy;
    va_copy(cpy, ap);
    written = vsnprintf(cbuf, CWRITE_INNER_BUFSZ, fmt, cpy);
    va_end(cpy);

    if (written >= CWRITE_INNER_BUFSZ) { // C99
      char *re = csize == CWRITE_INNER_BUFSZ ? NULL : cbuf;
      csize *= 2;
      cbuf = (char *)realloc(re, csize);
    } else if (written < 0) {
      va_end(ap);
      return written;
    } else {
      break;
    }
  }
  if (csize != CWRITE_INNER_BUFSZ)
    free(cbuf);

  for (size_t i = 0; i < (size_t)written; i++)
    if (cbuf[i] == '\n')
      newlines++;
  ctx->line_nbr += newlines;

  written = vfprintf(ctx->f, fmt, ap);
  va_end(ap);
  return written;
}

#define cwrite(...) cwrite_inner(ctx, __VA_ARGS__)

static inline void codegen_ctx_init(codegen_ctx *ctx, Args *args, ASTNode *ast,
                                    Symtabs symtabs, TrieAutomaton trie,
                                    list_SMAutomaton smauts) {
  ctx->args = args;
  ctx->ast = ast;
  ctx->trie = trie;
  ctx->smauts = smauts;
  ctx->expr_cnt = 0;
  ctx->indent_cnt = 1;
  ctx->line_nbr = 1;
  ctx->tok_kind_names = symtabs.tok_kind_names;
  ctx->peg_kind_names = symtabs.peg_kind_names;
  ctx->directives = symtabs.directives;
  ctx->definitions = symtabs.definitions;
  ctx->tokendefs = symtabs.tokendefs;

  // Check to make sure we actually have code to generate.
  if ((!trie.accepting.len) & (!smauts.len))
    ERROR("No grammar rules defined. Exiting.");

  // Parse prefix from grammar file name.
  char *pref_start = args->grammarTarget;
  size_t gstart = strlen(args->grammarTarget);

  // Figure out where to start parsing from.
  // Backtrack to the last /.
  while (1) {
    if (!gstart)
      break;
    if (args->grammarTarget[--gstart] == '/') {
      pref_start = args->grammarTarget + gstart + 1;
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
      low -= (char)('A' - 'a');

    // [_a-zA-Z] to [_A-Z].
    if ((up >= 'a') & (up <= 'z'))
      up += (char)('A' - 'a');

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
  // Doesn't overflow.

  if (!args->outputTarget) {
    args->outputTarget = (char *)malloc(PGEN_PREFIX_LEN + 3);
    sprintf(args->outputTarget, "%s.h", ctx->lower);
  } else {
    char *cpy = (char *)malloc(strlen(args->outputTarget) + 1);
    strcpy(cpy, args->outputTarget);
    args->outputTarget = cpy;
  }

  ctx->f = tmpfile();
  if (!ctx->f)
    ERROR("Could not open a temp file.");

  // Set fully buffered with 64 pages of memory.
  size_t bufsz = 4096 * 64;
  ctx->fbuffer = (char *)malloc(bufsz);
  if (ctx->fbuffer)
    setvbuf(ctx->f, ctx->fbuffer, _IOFBF, bufsz);
}

static inline void codegen_ctx_destroy(codegen_ctx *ctx) {

// Copy the temp file to the other buffer.
#define DESTR_BUFLEN 4096 * 5
  char destr_buf[DESTR_BUFLEN];
  rewind(ctx->f);
  FILE *outfile = fopen(ctx->args->outputTarget, "w");
  while (!feof(ctx->f)) {
    if (fgets(destr_buf, DESTR_BUFLEN, ctx->f) == NULL)
      break;
    fputs(destr_buf, outfile);
  }
  fclose(outfile);

  fclose(ctx->f);
  if (ctx->fbuffer)
    free(ctx->fbuffer);

  free(ctx->args->outputTarget);

  ASTNode_destroy(ctx->ast);

  list_cstr_clear(&ctx->tok_kind_names);
  list_cstr_clear(&ctx->peg_kind_names);
  list_ASTNodePtr_clear(&ctx->directives);
  list_ASTNodePtr_clear(&ctx->definitions);
  list_ASTNodePtr_clear(&ctx->tokendefs);
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
static inline void write_oom_directive(codegen_ctx *ctx);
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
  cwrite("#define PGEN_PARSER_MACROS_INCLUDED\n\n");

  cwrite("#if defined(__STDC_VERSION__) && (__STDC_VERSION__ >= 199901L) && "
         "!defined(__cplusplus)\n");
  cwrite("#  define PGEN_RESTRICT restrict\n");
  cwrite("#elif defined(__clang__) || \\\n");
  cwrite("     (defined(__GNUC__) && (__GNUC__ >= 4)) || \\\n");
  cwrite("     (defined(_MSC_VER) && (_MSC_VER >= 1900))\n");
  cwrite("#  define PGEN_RESTRICT __restrict\n");
  cwrite("#else\n");
  cwrite("#  define PGEN_RESTRICT\n");
  cwrite("#endif\n\n");

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
         "\n",
         ctx->upper, ctx->upper, ctx->upper, ctx->upper);
}

static inline void tok_write_enum(codegen_ctx *ctx) {
  cwrite("typedef enum {\n");
  size_t num_defs = ctx->tok_kind_names.len;
  cwrite("  %s_TOK_STREAMBEGIN,\n"
         "  %s_TOK_STREAMEND,\n",
         ctx->upper, ctx->upper);
  for (size_t i = 0; i < num_defs; i++)
    cwrite("  %s_TOK_%s,\n", ctx->upper, (char *)(ctx->tok_kind_names.buf[i]));
  cwrite("} %s_token_kind;\n\n", ctx->lower);

  cwrite("// The 0th token is beginning of stream.\n"
         "// The 1st token isend of stream.\n"
         "// Tokens 1 through %zu are the ones you defined.\n"
         "// This totals %zu kinds of tokens.\n",
         num_defs, num_defs + 2);
  cwrite("#define %s_NUM_TOKENKINDS %zu\n", ctx->upper, num_defs + 2);
  cwrite("static const char* %s_tokenkind_name[%s_NUM_TOKENKINDS] = {\n"
         "  \"STREAMBEGIN\",\n"
         "  \"STREAMEND\",\n",
         ctx->lower, ctx->upper);
  for (size_t i = 0; i < num_defs; i++)
    cwrite("  \"%s\",\n", (char *)(ctx->tok_kind_names.buf[i]));
  cwrite("};\n\n");
}

static inline void tok_write_tokenstruct(codegen_ctx *ctx) {
  cwrite("typedef struct {\n"
         "  %s_token_kind kind;\n"
         "  codepoint_t* content; // The token begins at "
         "tokenizer->start[token->start].\n"
         "  size_t len;\n"
         "  size_t line;\n"
         "  size_t col;\n",
         ctx->lower);

  int has_tokenextra = 0;
  for (size_t i = 0; i < ctx->directives.len; i++) {
    ASTNode *dir = ctx->directives.buf[i];
    char *dir_name = (char *)dir->children[0]->extra;
    if (!strcmp(dir_name, "tokenextra")) {
      if (!has_tokenextra) {
        has_tokenextra = 1;
        cwrite("  // Extra fields from %%tokenextra directives:\n");
      }
      cwrite("  %s\n", (char *)dir->extra);
    }
  }

  cwrite("} %s_token;\n\n", ctx->lower);
}

static inline void tok_write_ctxstruct(codegen_ctx *ctx) {
  cwrite("typedef struct {\n"
         "  codepoint_t* start;\n"
         "  size_t len;\n"
         "  size_t pos;\n"
         "  size_t pos_line;\n"
         "  size_t pos_col;\n"
         "} %s_tokenizer;\n\n",
         ctx->lower);

  cwrite("static inline void %s_tokenizer_init(%s_tokenizer* tokenizer, "
         "codepoint_t* start, size_t len) {\n"
         "  tokenizer->start = start;\n"
         "  tokenizer->len = len;\n"
         "  tokenizer->pos = 0;\n"
         "  tokenizer->pos_line = 1;\n"
         "  tokenizer->pos_col = 0;\n"
         "}\n\n",
         ctx->lower, ctx->lower);
}

static inline void tok_write_staterange(codegen_ctx *ctx, size_t smaut_num,
                                        StateRange range) {
  if (range.f == range.s) {
    cwrite("smaut_state_%zu == %i", smaut_num, range.f);
  } else if (range.f + 1 == range.s) {
    cwrite("(smaut_state_%zu == %i) | (smaut_state_%zu == %i)", smaut_num,
           range.f, smaut_num, range.s);
  } else {
    cwrite("(smaut_state_%zu >= %i) & (smaut_state_%zu <= %i)", smaut_num,
           range.f, smaut_num, range.s);
  }
}

static inline void tok_write_staterangecheck(codegen_ctx *ctx, size_t smaut_num,
                                             list_StateRange states) {
  if (!states.len) {
    cwrite("1");
  } else if (states.len == 1) { // Single range
    tok_write_staterange(ctx, smaut_num, states.buf[0]);
  } else { // List of ranges
    cwrite("(");
    tok_write_staterange(ctx, smaut_num, states.buf[0]);
    for (size_t i = 1; i < states.len; i++)
      cwrite(") | ("), tok_write_staterange(ctx, smaut_num, states.buf[i]);
    cwrite(")");
  }
}

static inline void tok_write_charcmp(codegen_ctx *ctx, codepoint_t c) {
#define WRITE_ESCCMP(esc)                                                      \
  if (c == esc) {                                                              \
    cwrite(#esc);                                                              \
    return;                                                                    \
  }
  WRITE_ESCCMP('\n');
  WRITE_ESCCMP('\'');
  WRITE_ESCCMP('\"');
  WRITE_ESCCMP('\\');
  if ((c >= 33) & (c <= 126)) {
    // As the letter or symbol
    cwrite("'%c'", (char)c);
  } else {
    // As a number
    cwrite("%" PRI_CODEPOINT "", c);
  }
}

static inline void tok_write_charrange(codegen_ctx *ctx, CharRange range) {
  if (range.f == range.s) {
    cwrite("c == ");
    tok_write_charcmp(ctx, range.f);
  } else if (range.f + 1 == range.s) {
    cwrite("(c == ");
    tok_write_charcmp(ctx, range.f);
    cwrite(") | (c == ");
    tok_write_charcmp(ctx, range.s);
    cwrite(")");
  } else {
    cwrite("(c >= ");
    tok_write_charcmp(ctx, range.f);
    cwrite(") & (c <= ");
    tok_write_charcmp(ctx, range.s);
    cwrite(")");
  }
}

static inline void tok_write_charrangecheck(codegen_ctx *ctx,
                                            list_CharRange states,
                                            bool inverted) {
  if (!states.len) {
    cwrite(inverted ? "1" : "0");
  } else if (states.len == 1) {
    if (inverted)
      cwrite("!(");
    tok_write_charrange(ctx, states.buf[0]);
    if (inverted)
      cwrite(")");
  } else {
    if (inverted)
      cwrite("!(");
    cwrite("(");
    tok_write_charrange(ctx, states.buf[0]);
    for (size_t i = 1; i < states.len; i++) {
      cwrite(") | ("), tok_write_charrange(ctx, states.buf[i]);
    }
    cwrite(")");
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
    if (!ctx->args->u)
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
    if (!ctx->args->u)
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
      if (!ctx->args->u)
        cwrite("    // Transition %s State Machine\n", aut.ident);
      cwrite("    if (smaut_state_%zu != -1) {\n", a);
      cwrite("      all_dead = 0;\n\n");

      int eels = 0;
      for (size_t i = 0; i < aut.trans.len; i++) {
        SMTransition trans = list_SMTransition_get(&aut.trans, i);
        cwrite("      %sif ((", eels++ ? "else " : "");
        tok_write_staterangecheck(ctx, a, trans.from);
        cwrite(") &\n         (");
        tok_write_charrangecheck(ctx, trans.on, trans.inverted);
        cwrite(")) {\n");
        cwrite("          smaut_state_%zu = %i;\n", a, trans.to);
        cwrite("      }\n");
      }
      cwrite("      else {\n");
      cwrite("        smaut_state_%zu = -1;\n", a);
      cwrite("      }\n\n");

      if (!ctx->args->u)
        cwrite("      // Check accept\n");

      cwrite("      if (");
      tok_write_staterangecheck(ctx, a, aut.accepting);
      cwrite(") {\n");
      cwrite("        smaut_munch_size_%zu = iidx + 1;\n", a);
      cwrite("      }\n");

      cwrite("    }\n\n");
    }
  }
  cwrite("    if (all_dead)\n");
  cwrite("      break;\n");
  cwrite("  }\n\n"); // For each remaining character

  if (!ctx->args->u)
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

  cwrite("  %s_token tok;\n", ctx->lower);
  cwrite("  tok.kind = kind;\n");
  cwrite("  tok.content = tokenizer->start + tokenizer->pos;\n");
  cwrite("  tok.len = max_munch;\n\n");

  cwrite("  tok.line = tokenizer->pos_line;\n");
  cwrite("  tok.col = tokenizer->pos_col;\n");
  int inserted_tokenextra = 0;
  for (size_t i = 0; i < ctx->directives.len; i++) {
    ASTNode *dir = ctx->directives.buf[i];
    char *dir_name = (char *)dir->children[0]->extra;
    if (!strcmp(dir_name, "tokenextrainit")) {
      if (!inserted_tokenextra) {
        inserted_tokenextra = 1;
        cwrite("  // Extra fields from %%tokenextra directives:\n");
      }
      cwrite("  %s\n", (char *)dir->extra);
    }
  }
  cwrite("\n");
  cwrite("  for (size_t i = 0; i < tok.len; i++) {\n");
  cwrite("    if (current[i] == '\\n') {\n");
  cwrite("      tokenizer->pos_line++;\n");
  cwrite("      tokenizer->pos_col = 0;\n");
  cwrite("    } else {\n");
  cwrite("      tokenizer->pos_col++;\n");
  cwrite("    }\n");
  cwrite("  }\n\n");

  cwrite("  tokenizer->pos += max_munch;\n");
  cwrite("  return tok;\n");
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

static inline void peg_write_header(codegen_ctx *ctx) {
  cwrite("#ifndef PGEN_%s_ASTNODE_INCLUDE\n", ctx->upper);
  cwrite("#define PGEN_%s_ASTNODE_INCLUDE\n\n", ctx->upper);
}

static inline void peg_write_footer(codegen_ctx *ctx) {
  cwrite("#endif /* PGEN_%s_ASTNODE_INCLUDE */\n\n", ctx->upper);
}

static const char *known_directives[] = {
    "oom",        "node",        "token",      "include",
    "preinclude", "postinclude", "code",       "precode",
    "postcode",   "define",      "predefine",  "postdefine",
    "extra",      "extrainit",   "tokenextra", "tokenextrainit",
    "context",    "contextinit", "errextra",   "errextrainit"};
static const size_t num_known_directives =
    sizeof(known_directives) / sizeof(const char *);

static inline int directive_is_known(char *directive) {
  for (size_t i = 0; i < num_known_directives; i++)
    if (!strcmp(directive, known_directives[i]))
      return 1;
  return 0;
}

static inline void peg_write_directive_label(codegen_ctx *ctx, int p) {
  // p is 0 for pre, 1 for mid, 2 for post.
  const char *str = p == 0 ? "Pre" : p == 1 ? "Mid" : "Post";
  const char *line = p == 2 ? "****" : "***";
  cwrite("/**%s*************/\n"
         "/* %s Directives */\n"
         "/**%s*************/\n",
         line, str, line);
}

static inline void write_oom_directive(codegen_ctx *ctx) {
  int oom_written = 0;
  for (size_t n = 0; n < ctx->directives.len; n++) {
    ASTNode *dir = ctx->directives.buf[n];
    char *dir_name = (char *)dir->children[0]->extra;

    // %oom directive
    if (!strcmp(dir_name, "oom")) {
      if (ctx->args->u && oom_written)
        ERROR("Duplicate %%oom directives.");
      if (ctx->args->u)
        // Falls back to default handler.
        fprintf(stderr, "PGEN warning: "
                        "%%oom directive unused with unsafe codegen.\n");
      cwrite("#define PGEN_OOM() %s\n", (char *)dir->extra);
      oom_written = 1;
    }
  }
}

static inline void peg_write_predirectives(codegen_ctx *ctx) {
  cwrite("struct %s_astnode_t;\n", ctx->lower);
  cwrite("typedef struct %s_astnode_t %s_astnode_t;\n\n", ctx->lower,
         ctx->lower);

  int label_written = 0;
  for (size_t n = 0; n < ctx->directives.len; n++) {
    ASTNode *dir = ctx->directives.buf[n];
    char *dir_name = (char *)dir->children[0]->extra;

    // %preinclude directive
    if (!strcmp(dir_name, "preinclude")) {
      if (!label_written)
        label_written = 1, peg_write_directive_label(ctx, 0);
      cwrite("#include %s\n", (char *)dir->extra);
    }
    // %predefine directive
    else if (!strcmp(dir_name, "predefine")) {
      if (!label_written)
        label_written = 1, peg_write_directive_label(ctx, 0);
      cwrite("#define %s\n", (char *)dir->extra);
    }
    // %precode directive
    else if (!strcmp(dir_name, "precode")) {
      if (!label_written)
        label_written = 1, peg_write_directive_label(ctx, 0);
      cwrite("%s\n", (char *)dir->extra);
    }
  }
  cwrite("\n");
}

static inline void peg_write_middirectives(codegen_ctx *ctx) {
  int label_written = 0;
  for (size_t n = 0; n < ctx->directives.len; n++) {
    ASTNode *dir = ctx->directives.buf[n];
    char *dir_name = (char *)dir->children[0]->extra;

    // %define directive
    if (!strcmp(dir_name, "define")) {
      if (!label_written)
        label_written = 1, peg_write_directive_label(ctx, 1);
      cwrite("#define %s\n", (char *)dir->extra);
    }
    // %include directive
    else if (!strcmp(dir_name, "include")) {
      if (!label_written)
        label_written = 1, peg_write_directive_label(ctx, 1);
      cwrite("#include %s\n", (char *)dir->extra);
    }
    // %code directive
    else if (!strcmp(dir_name, "code")) {
      if (!label_written)
        label_written = 1, peg_write_directive_label(ctx, 1);
      cwrite("%s\n", (char *)dir->extra);
    }
  }
  cwrite("\n");
}

static inline void peg_write_postdirectives(codegen_ctx *ctx) {
  int label_written = 0;
  for (size_t n = 0; n < ctx->directives.len; n++) {
    ASTNode *dir = ctx->directives.buf[n];
    char *dir_name = (char *)dir->children[0]->extra;

    // %postdefine directive
    if (!strcmp(dir_name, "postdefine")) {
      if (!label_written)
        label_written = 1, peg_write_directive_label(ctx, 2);
      cwrite("#define %s\n", (char *)dir->extra);
    }
    // %postinclude directive
    else if (!strcmp(dir_name, "postinclude")) {
      if (!label_written)
        label_written = 1, peg_write_directive_label(ctx, 2);
      cwrite("#include %s\n", (char *)dir->extra);
    }
    // %postcode directive
    else if (!strcmp(dir_name, "postcode")) {
      if (!label_written)
        label_written = 1, peg_write_directive_label(ctx, 2);
      cwrite("%s\n", (char *)dir->extra);
    } else {
      if (!directive_is_known(dir_name)) {
        fprintf(stderr, "Unknown directive: %s\n", dir_name);
        continue;
      }
    }
  }
  cwrite("\n");
}

static inline void peg_write_parser_errdef(codegen_ctx *ctx) {
  cwrite("struct %s_parse_err;\n", ctx->lower);
  cwrite("typedef struct %s_parse_err %s_parse_err;\n", ctx->lower, ctx->lower);
  cwrite("struct %s_parse_err {\n", ctx->lower);
  cwrite("  const char* msg;\n");
  // cwrite("  void (*msgfree)(const char* msg, void* extra);\n");
  for (size_t n = 0; n < ctx->directives.len; n++) {
    ASTNode *dir = ctx->directives.buf[n];
    char *dir_name = (char *)dir->children[0]->extra;
    if (!strcmp(dir_name, "errextra")) {
      cwrite("  %s\n", (char *)dir->extra);
    }
  }
  cwrite("  int severity;\n");
  cwrite("  size_t line;\n");
  cwrite("  size_t col;\n");
  cwrite("};\n\n");
}

static inline void peg_write_parser_ctx(codegen_ctx *ctx) {
  cwrite("#ifndef %s_MAX_PARSER_ERRORS\n", ctx->upper);
  cwrite("#define %s_MAX_PARSER_ERRORS 20\n", ctx->upper);
  cwrite("#endif\n");
  cwrite("typedef struct {\n");
  cwrite("  %s_token* tokens;\n", ctx->lower);
  cwrite("  size_t len;\n");
  cwrite("  size_t pos;\n");
  cwrite("  int exit;\n");
  cwrite("  pgen_allocator *alloc;\n");
  for (size_t n = 0; n < ctx->directives.len; n++) {
    ASTNode *context_dir = ctx->directives.buf[n];
    char *dir_name = (char *)context_dir->children[0]->extra;
    if (strcmp(context_dir->name, "Directive") || strcmp(dir_name, "context"))
      continue;
    cwrite("  %s\n", (char *)context_dir->extra);
  }
  cwrite("  size_t num_errors;\n");
  cwrite("  %s_parse_err errlist[%s_MAX_PARSER_ERRORS];\n", ctx->lower,
         ctx->upper);
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
  cwrite("  parser->exit = 0;\n");
  cwrite("  parser->alloc = allocator;\n");
  for (size_t n = 0; n < ctx->directives.len; n++) {
    ASTNode *context_dir = ctx->directives.buf[n];
    char *dir_name = (char *)context_dir->children[0]->extra;
    if (strcmp(context_dir->name, "Directive") ||
        strcmp(dir_name, "contextinit"))
      continue;
    cwrite("  %s\n", (char *)context_dir->extra);
  }
  cwrite("  parser->num_errors = 0;\n");
  cwrite("  size_t to_zero = sizeof(%s_parse_err) * %s_MAX_PARSER_ERRORS;\n",
         ctx->lower, ctx->upper);
  cwrite("  memset(&parser->errlist, 0, to_zero);\n");
  cwrite("}\n");
}

static inline void peg_write_report_parse_error(codegen_ctx *ctx) {

  cwrite("static inline void freemsg(const char* msg, void* extra) {\n"
         "  (void)extra;\n"
         "  PGEN_FREE((void*)msg);\n"
         "}\n\n");

  cwrite(
      "static inline %s_parse_err* %s_report_parse_error(%s_parser_ctx* ctx,\n"
      "              const char* msg, void (*msgfree)(const char* msg, void* "
      "extra), int severity) {\n",
      ctx->lower, ctx->lower, ctx->lower);
  cwrite("  if (ctx->num_errors >= %s_MAX_PARSER_ERRORS) {\n", ctx->upper);
  cwrite("    ctx->exit = 1;\n");
  cwrite("    return NULL;\n");
  cwrite("  }\n");
  cwrite("  %s_parse_err* err = &ctx->errlist[ctx->num_errors++];\n",
         ctx->lower, ctx->lower);
  cwrite("  err->msg = (const char*)msg;\n");
  cwrite("  err->severity = severity;\n");
  cwrite("  size_t toknum = ctx->pos + (ctx->pos != ctx->len - 1);\n");
  cwrite("  %s_token tok = ctx->tokens[toknum];\n", ctx->lower);
  cwrite("  err->line = tok.line;\n");
  cwrite("  err->col = tok.col;\n\n");

  // Insert errextrainit directives
  for (size_t n = 0; n < ctx->directives.len; n++) {
    ASTNode *dir = ctx->directives.buf[n];
    char *dir_name = (char *)dir->children[0]->extra;
    if (!strcmp(dir_name, "errextrainit"))
      cwrite("  %s\n", (char *)dir->extra);
  }

  cwrite("  if (severity == 3)\n");
  cwrite("    ctx->exit = 1;\n");
  cwrite("  return err;\n");
  cwrite("}\n\n");
}

static inline void peg_write_astnode_kind(codegen_ctx *ctx) {
  cwrite("typedef enum {\n");
  for (size_t i = 0; i < ctx->tok_kind_names.len; i++)
    cwrite("  %s_NODE_%s,\n", ctx->upper, ctx->tok_kind_names.buf[i]);
  for (size_t i = 0; i < ctx->peg_kind_names.len; i++)
    cwrite("  %s_NODE_%s,\n", ctx->upper, ctx->peg_kind_names.buf[i]);
  cwrite("} %s_astnode_kind;\n\n", ctx->lower);

  size_t num_kinds = ctx->tok_kind_names.len + ctx->peg_kind_names.len;
  cwrite("#define %s_NUM_NODEKINDS %zu\n", ctx->upper, num_kinds);

  cwrite("static const char* %s_nodekind_name[%s_NUM_NODEKINDS] = {\n",
         ctx->lower, ctx->upper);
  for (size_t i = 0; i < ctx->tok_kind_names.len; i++)
    cwrite("  \"%s\",\n", ctx->tok_kind_names.buf[i]);
  for (size_t i = 0; i < ctx->peg_kind_names.len; i++)
    cwrite("  \"%s\",\n", ctx->peg_kind_names.buf[i]);
  cwrite("};\n\n");
}

// Check to make sure that the given ASTNode kind can be used.
// Error the program otherwise.
static inline void peg_ensure_kind(codegen_ctx *ctx, char *kind) {
  int found = 0;
  for (size_t n = 0; n < ctx->tok_kind_names.len; n++) {
    if (!strcmp(ctx->tok_kind_names.buf[n], kind)) {
      found = 1;
      break;
    }
  }
  for (size_t n = 0; n < ctx->peg_kind_names.len; n++) {
    if (!strcmp(ctx->peg_kind_names.buf[n], kind)) {
      found = 1;
      break;
    }
  }
  if (!found)
    ERROR("No node kind %s has been defined with a %%node directive.", kind);
}

static inline void peg_write_astnode_def(codegen_ctx *ctx) {
  cwrite("struct %s_astnode_t {\n", ctx->lower);

  cwrite("  %s_astnode_t* parent;\n", ctx->lower);
  cwrite("  uint16_t num_children;\n");
  cwrite("  uint16_t max_children;\n");
  cwrite("  %s_astnode_kind kind;\n\n", ctx->lower);
  cwrite("  codepoint_t* tok_repr;\n");
  cwrite("  size_t repr_len;\n");

  // Insert %extra directives.
  int inserted_extra = 0;
  for (size_t n = 0; n < ctx->directives.len; n++) {
    ASTNode *dir = ctx->directives.buf[n];
    char *dir_name = (char *)dir->children[0]->extra;

    // %extra directive
    if (!strcmp(dir_name, "extra")) {
      if (!inserted_extra) {
        inserted_extra = 1;
        cwrite("  // Extra data in %%extra directives:\n");
      }
      cwrite("  %s\n", (char *)dir->extra);
    }
  }
  cwrite(inserted_extra ? "  // End of extra data.\n"
                        : "  // No %%extra directives.\n");
  cwrite("  %s_astnode_t** children;\n", ctx->lower);
  cwrite("};\n\n");
}

static inline void peg_write_minmax(codegen_ctx *ctx) {
  const char *mm[] = {"MIN", "MAX"};
  for (size_t m = 0; m < 2; m++) {
    for (size_t i = 1; i < NODE_NUM_FIXED + 1; i++) {
      if (i == 1) {
        cwrite("#define PGEN_%s1(a) a\n", mm[m]);
        continue;
      }

      cwrite("#define PGEN_%s%zu(a", mm[m], i);
      for (size_t j = 1; j < i; j++) {
        cwrite(", %c", 'a' + (unsigned char)j);
      }
      cwrite(") PGEN_%s(a, PGEN_%s%zu(", mm[m], mm[m], i - 1);
      for (size_t j = 1; j < i; j++) {
        if (j > 1)
          cwrite(", ");
        cwrite("%c", 'a' + (unsigned char)j);
      }
      cwrite("))\n");
    }
  }
  // Min or Max nonzero
  cwrite("#define PGEN_MAX(a, b) ((a) > (b) ? (a) : (b))\n");
  cwrite("#define PGEN_MIN(a, b) ((a) ? ((a) > (b) ? (b) : (a)) : (b))\n\n\n");
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
  cwrite("  %s_astnode_t *node = (%s_astnode_t*)ret;\n\n", ctx->lower,
         ctx->lower);
  cwrite("  %s_astnode_t **children;\n", ctx->lower);
  cwrite("  if (initial_size) {\n");
  cwrite("    children = (%s_astnode_t**)PGEN_MALLOC("
         "sizeof(%s_astnode_t*) * initial_size);\n",
         ctx->lower, ctx->lower);
  if (!ctx->args->u)
    cwrite("    if (!children) PGEN_OOM();\n");
  cwrite("    pgen_defer(alloc, PGEN_FREE, children, alloc->rew);\n");
  cwrite("  } else {\n");
  cwrite("    children = NULL;\n");
  cwrite("  }\n\n");
  cwrite("  node->kind = kind;\n");
  cwrite("  node->parent = NULL;\n");
  cwrite("  node->max_children = (uint16_t)initial_size;\n");
  cwrite("  node->num_children = 0;\n");
  cwrite("  node->children = children;\n");
  cwrite("  node->tok_repr = NULL;\n");
  cwrite("  node->repr_len = 0;\n");

  // Insert %extrainit directives.
  int inserted_extrainit = 0;
  for (size_t n = 0; n < ctx->directives.len; n++) {
    ASTNode *dir = ctx->directives.buf[n];
    char *dir_name = (char *)dir->children[0]->extra;

    if (!strcmp(dir_name, "extrainit")) {
      if (!inserted_extrainit) {
        inserted_extrainit = 1;
        cwrite("  // Extra initialization from %%extrainit directives:\n");
      }
      cwrite("  %s\n", (char *)dir->extra);
    }
  }
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
  cwrite("  %s_astnode_t *node = (%s_astnode_t *)ret;\n", ctx->lower,
         ctx->lower);
  cwrite("  %s_astnode_t *children = NULL;\n", ctx->lower);
  cwrite("  node->kind = kind;\n");
  cwrite("  node->parent = NULL;\n");
  cwrite("  node->max_children = 0;\n");
  cwrite("  node->num_children = 0;\n");
  cwrite("  node->children = NULL;\n");
  cwrite("  node->tok_repr = NULL;\n");
  cwrite("  node->repr_len = 0;\n");
  inserted_extrainit = 0;
  for (size_t n = 0; n < ctx->directives.len; n++) {
    ASTNode *dir = ctx->directives.buf[n];
    if (strcmp(dir->name, "Directive"))
      continue;
    char *dir_name = (char *)dir->children[0]->extra;

    // %extra directive
    if (!strcmp(dir_name, "extrainit")) {
      if (!inserted_extrainit) {
        inserted_extrainit = 1;
        cwrite("  // Extra initialization from %%extrainit directives:\n");
      }
      cwrite("  %s\n", (char *)dir->extra);
    }
  }
  cwrite("  return node;\n");
  cwrite("}\n\n");

  for (size_t i = 1; i <= NODE_NUM_FIXED; i++) {
    cwrite("static inline %s_astnode_t* %s_astnode_fixed_%zu(\n", ctx->lower,
           ctx->lower, i);
    cwrite("                             pgen_allocator* alloc,\n"
           "                             %s_astnode_kind kind%s",
           ctx->lower, i ? ",\n" : "");
    for (size_t j = 0; j < i; j++)
      cwrite("                             %s_astnode_t* PGEN_RESTRICT n%zu%s",
             ctx->lower, j, j != i - 1 ? ",\n" : "");
    cwrite(") {\n");
    if (ctx->args->d) {
      cwrite("%s_astnode_t const * const SUCC = "
             "((%s_astnode_t*)(void*)(uintptr_t)_Alignof(%s_astnode_t));\n",
             ctx->lower, ctx->lower, ctx->lower);
      cwrite("  if ((!n0)");
      for (size_t j = 1; j < i; j++)
        cwrite(" | (!n%zu)", j);
      for (size_t j = 0; j < i; j++)
        cwrite(" | (n%zu == SUCC)", j);
      cwrite(")\n    fprintf(stderr, \"Invalid arguments: node(%%s");
      for (size_t j = 0; j < i; j++)
        cwrite(", %%p");
      cwrite(")\\n\", %s_nodekind_name[kind]", ctx->lower);
      for (size_t j = 0; j < i; j++)
        cwrite(", (void*)n%zu", j);
      cwrite("), exit(1);\n");
    }
    cwrite("  char* ret = pgen_alloc(alloc,\n"
           "                         sizeof(%s_astnode_t) +\n"
           "                         sizeof(%s_astnode_t *) * %zu,\n",
           ctx->lower, ctx->lower, i);
    cwrite("                         _Alignof(%s_astnode_t));\n", ctx->lower);
    cwrite("  %s_astnode_t *node = (%s_astnode_t *)ret;\n", ctx->lower,
           ctx->lower);
    cwrite("  %s_astnode_t **children = (%s_astnode_t **)(node + 1);\n",
           ctx->lower, ctx->lower);
    cwrite("  node->kind = kind;\n");
    cwrite("  node->parent = NULL;\n");
    cwrite("  node->max_children = 0;\n");
    cwrite("  node->num_children = %zu;\n", i);
    cwrite("  node->children = children;\n");
    cwrite("  node->tok_repr = NULL;\n");
    cwrite("  node->repr_len = 0;\n");
    for (size_t j = 0; j < i; j++) {
      cwrite("  children[%zu] = n%zu;\n", j, j);
      cwrite("  n%zu->parent = node;\n", j);
    }
    cwrite("  return node;\n");
    cwrite("}\n\n");
  }
}

static inline void peg_write_parsermacros(codegen_ctx *ctx) {

  cwrite("#define SUCC                     "
         "(%s_astnode_t*)(void*)(uintptr_t)_Alignof(%s_astnode_t)\n\n",
         ctx->lower, ctx->lower);

  cwrite("#define rec(label)               "
         "pgen_parser_rewind_t _rew_##label = "
         "(pgen_parser_rewind_t){ctx->alloc->rew, ctx->pos};\n");
  cwrite("#define rew(label)               "
         "%s_parser_rewind(ctx, _rew_##label)\n",
         ctx->lower);
  cwrite("#define node(kindname, ...)      "
         "PGEN_CAT(%s_astnode_fixed_, "
         "PGEN_NARG(__VA_ARGS__))"
         "(ctx->alloc, kind(kindname), __VA_ARGS__)\n",
         ctx->lower);
  cwrite("#define kind(name)               "
         "%s_NODE_##name\n",
         ctx->upper);
  cwrite("#define list(kind)               "
         "%s_astnode_list(ctx->alloc, %s_NODE_##kind, 16)\n",
         ctx->lower, ctx->upper);
  cwrite("#define leaf(kind)               "
         "%s_astnode_leaf(ctx->alloc, %s_NODE_##kind)\n",
         ctx->lower, ctx->upper);
  cwrite("#define add(list, node)          "
         "%s_astnode_add(ctx->alloc, list, node)\n",
         ctx->lower);
  cwrite("#define has(node)                "
         "(((uintptr_t)node <= (uintptr_t)SUCC) ? 0 : 1)\n");
  cwrite("#define repr(node, t)            "
         "%s_astnode_repr(node, t)\n",
         ctx->lower);
  cwrite("#define srepr(node, s)           "
         "%s_astnode_srepr(ctx->alloc, node, (char*)s)\n",
         ctx->lower);
  cwrite("#define cprepr(node, cps, len)   "
         "%s_astnode_cprepr(node, cps, len)\n",
         ctx->lower);
  cwrite(
      "#define expect(kind, cap)        "
      "((ctx->pos < ctx->len && ctx->tokens[ctx->pos].kind == %s_TOK_##kind) "
      "? ctx->pos++, (cap ? cprepr(leaf(kind), NULL, ctx->pos-1) : SUCC) "
      ": NULL)\n",
      ctx->lower);
  cwrite("\n");

  cwrite("#define LB {\n");
  cwrite("#define RB }\n");
  cwrite("\n");

  cwrite("#define INFO(msg)                "
         "%s_report_parse_error(ctx, (const char*)msg, NULL,   0)\n",
         ctx->lower);
  cwrite("#define WARNING(msg)             "
         "%s_report_parse_error(ctx, (const char*)msg, NULL,   1)\n",
         ctx->lower);
  cwrite("#define ERROR(msg)               "
         "%s_report_parse_error(ctx, (const char*)msg, NULL,   2)\n",
         ctx->lower);
  cwrite("#define FATAL(msg)               "
         "%s_report_parse_error(ctx, (const char*)msg, NULL,   3)\n",
         ctx->lower);
  cwrite("#define INFO_F(msg, freefn)      "
         "%s_report_parse_error(ctx, (const char*)msg, freefn, 0)\n",
         ctx->lower);
  cwrite("#define WARNING_F(msg, freefn)   "
         "%s_report_parse_error(ctx, (const char*)msg, freefn, 1)\n",
         ctx->lower);
  cwrite("#define ERROR_F(msg, freefn)     "
         "%s_report_parse_error(ctx, (const char*)msg, freefn, 2)\n",
         ctx->lower);
  cwrite("#define FATAL_F(msg, freefn)     "
         "%s_report_parse_error(ctx, (const char*)msg, freefn, 3)\n",
         ctx->lower);

  cwrite("\n");
}

static inline void peg_write_repr(codegen_ctx *ctx) {
  cwrite("static inline %s_astnode_t* %s_astnode_repr(%s_astnode_t* node, "
         "%s_astnode_t* t) {\n",
         ctx->lower, ctx->lower, ctx->lower, ctx->lower);
  cwrite("  node->tok_repr = t->tok_repr;\n");
  cwrite("  node->repr_len = t->repr_len;\n");
  cwrite("  return node;\n");
  cwrite("}\n\n");

  cwrite("static inline %s_astnode_t* %s_astnode_cprepr("
         "%s_astnode_t* node, codepoint_t* cps, size_t repr_len) {\n",
         ctx->lower, ctx->lower, ctx->lower);
  cwrite("  node->tok_repr = cps;\n");
  cwrite("  node->repr_len = repr_len;\n");
  cwrite("  return node;\n");
  cwrite("}\n\n");

  cwrite("static inline %s_astnode_t* %s_astnode_srepr("
         "pgen_allocator* allocator, "
         "%s_astnode_t* node, "
         "char* s) {\n",
         ctx->lower, ctx->lower, ctx->lower);
  cwrite("  size_t cpslen = strlen(s);\n");
  cwrite("  codepoint_t* cps = (codepoint_t*)pgen_alloc("
         "allocator, "
         "(cpslen + 1) * sizeof(codepoint_t), "
         "_Alignof(codepoint_t));\n");
  cwrite("  for (size_t i = 0; i < cpslen; i++) cps[i] = (codepoint_t)s[i];\n");
  cwrite("  cps[cpslen] = 0;\n");
  cwrite("  node->tok_repr = cps;\n");
  cwrite("  node->repr_len = cpslen;\n");
  cwrite("  return node;\n");
  cwrite("}\n\n");
}

static inline void peg_write_astnode_add(codegen_ctx *ctx) {
  cwrite("static inline void %s_astnode_add("
         "pgen_allocator* alloc, %s_astnode_t *list, %s_astnode_t *node) {\n",
         ctx->lower, ctx->lower, ctx->lower);
  cwrite("  if (list->max_children == list->num_children) {\n");
  if (!ctx->args->u)
    cwrite("    // Figure out the new size. Check for overflow where "
           "applicable.\n");
  cwrite("    uint64_t new_max = (uint64_t)list->max_children * 2;\n");
  if (!ctx->args->u) {
    cwrite("    if (new_max > UINT16_MAX || new_max > SIZE_MAX) PGEN_OOM();\n");
    cwrite("    if (SIZE_MAX < UINT16_MAX && "
           "(size_t)new_max > SIZE_MAX / sizeof(%s_astnode_t)) PGEN_OOM();\n",
           ctx->lower);
  }
  cwrite("    size_t new_bytes = (size_t)new_max * sizeof(%s_astnode_t);\n\n",
         ctx->lower);

  cwrite("    // Reallocate the list, and inform the allocator.\n");
  cwrite("    void* old_ptr = list->children;\n");
  cwrite("    void* new_ptr = realloc(list->children, new_bytes);\n");
  if (!ctx->args->u)
    cwrite("    if (!new_ptr) PGEN_OOM();\n");
  cwrite("    list->children = (%s_astnode_t **)new_ptr;\n", ctx->lower);
  cwrite("    list->max_children = (uint16_t)new_max;\n");
  cwrite("    pgen_allocator_realloced(alloc, old_ptr, new_ptr, free);\n");
  cwrite("  }\n");
  cwrite("  node->parent = list;\n");
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

static inline void peg_write_node_print(codegen_ctx *ctx) {

  cwrite("static inline int %s_node_print_content(%s_astnode_t* node, "
         "%s_token* tokens) {\n",
         ctx->lower, ctx->lower, ctx->lower);
  cwrite("  int found = 0;\n");
  cwrite("  codepoint_t* utf32 = NULL; size_t utf32len = 0;\n");
  cwrite("  char* utf8 = NULL; size_t utf8len = 0;\n");
  cwrite("  if (node->tok_repr && node->repr_len) {\n");
  cwrite("    utf32 = node->tok_repr;\n");
  cwrite("    utf32len = node->repr_len;\n");
  cwrite("    int success = UTF8_encode("
         "node->tok_repr, node->repr_len, &utf8, &utf8len);\n");
  cwrite("    if (success) {\n");
  cwrite("      for (size_t i = 0; i < utf8len; i++)\n");
  cwrite("        if (utf8[i] == '\\n') fputc('\\\\', stdout), fputc('n', "
         "stdout);\n");
  cwrite("        else fputc(utf8[i], stdout);\n");
  cwrite("      return PGEN_FREE(utf8), 1;\n");
  cwrite("    }\n");
  cwrite("  }\n");
  cwrite("  return 0;\n");
  cwrite("}\n\n");

  // codepoint_t *codepoints, size_t len, char **retstr, size_t *retlen

  // cwrite("static inline void %s_token_print_json(%s_astnode_t *node, size_t "
  // "depth, int fl) {\n", ctx->lower, ctx->lower);
}

static inline void peg_write_astnode_print(codegen_ctx *ctx) {
  cwrite("static inline int %s_astnode_print_h(%s_token* tokens, %s_astnode_t "
         "*node, size_t "
         "depth, int fl) {\n",
         ctx->lower, ctx->lower, ctx->lower);
  cwrite("  #define indent() "
         "for (size_t i = 0; i < depth; i++) printf(\"  \")\n");
  cwrite("  if (!node)\n");
  cwrite("    return 0;\n");
  cwrite("  else if (node == "
         "(%s_astnode_t*)(void*)(uintptr_t)_Alignof(%s_astnode_t))\n",
         ctx->lower, ctx->lower);
  cwrite("    puts(\"ERROR, CAPTURED SUCC.\"), exit(1);\n\n");

  cwrite("  indent(); puts(\"{\");\n");
  cwrite("  depth++;\n");

  cwrite("  indent(); printf(\"\\\"kind\\\": \"); "
         "printf(\"\\\"%%s\\\",\\n\", %s_nodekind_name[node->kind]);\n",
         ctx->lower);
  cwrite("  if (!(!node->tok_repr & !node->repr_len)) {\n"
         "    indent();\n"
         "    printf(\"\\\"content\\\": \\\"\");\n"
         "    %s_node_print_content(node, tokens);\n"
         "    printf(\"\\\",\\n\");\n"
         "  }\n",
         ctx->lower);
  cwrite("  size_t cnum = node->num_children;\n");
  cwrite("  if (cnum) {\n");
  cwrite("    indent(); printf(\"\\\"num_children\\\": %%zu,\\n\", cnum);\n");
  cwrite("    indent(); printf(\"\\\"children\\\": [\");\n");
  cwrite("    putchar('\\n');\n");
  cwrite("    for (size_t i = 0; i < cnum; i++)\n"
         "      %s_astnode_print_h(tokens, node->children[i], depth + 1, "
         "i == cnum - 1);\n",
         ctx->lower);
  cwrite("    indent();\n");
  cwrite("    printf(\"]\\n\");\n");
  cwrite("  }\n");

  cwrite("  depth--;\n");
  cwrite(
      "  indent(); putchar('}'); if (fl != 1) putchar(','); putchar('\\n');\n");
  cwrite("  return 0;\n");
  cwrite("#undef indent\n");
  cwrite("}\n\n"); // End of print helper fn

  cwrite("static inline void %s_astnode_print_json(%s_token* tokens, "
         "%s_astnode_t *node) {\n",
         ctx->lower, ctx->lower, ctx->lower);
  cwrite("  if (node)");
  cwrite("    %s_astnode_print_h(tokens, node, 0, 1);\n", ctx->lower);
  cwrite("  else");
  cwrite("    puts(\"The AST is null.\");");
  cwrite("}\n\n");
}

static inline void peg_write_definition_stub(codegen_ctx *ctx, ASTNode *def) {
  char *def_name = (char *)def->children[0]->extra;
  cwrite("static inline %s_astnode_t* %s_parse_%s(%s_parser_ctx* ctx);\n",
         ctx->lower, ctx->lower, def_name, ctx->lower);
}

static inline void peg_visit_add_labels(codegen_ctx *ctx, ASTNode *expr,
                                        list_cstr *idlist) {
  if (expr->num_children >= 2 && !strcmp(expr->name, "ModExpr") &&
      !strcmp(expr->children[1]->name, "LowerIdent")) {
    ASTNode *label_ident = expr->children[1];

    char *idname = (char *)label_ident->extra;

    int append = 1;
    for (size_t i = 0; i < idlist->len; i++) {
      if (!strcmp(idname, idlist->buf[i]) || !strcmp(idname, "rule")) {
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
    if (!ctx->args->u) {                                                       \
      indent(ctx);                                                             \
      cwrite("// ");                                                           \
      cwrite(__VA_ARGS__);                                                     \
      cwrite("\n");                                                            \
    }                                                                          \
  } while (0)

#define iwrite(...)                                                            \
  do {                                                                         \
    indent(ctx);                                                               \
    cwrite(__VA_ARGS__);                                                       \
  } while (0)

static inline void start_embed(codegen_ctx *ctx, size_t line) {
  iwrite("#line %zu \"%s\"\n", line, ctx->args->grammarTarget);
}
static inline void end_embed(codegen_ctx *ctx) {
  iwrite("#line %zu \"%s\"\n\n", ctx->line_nbr + 1, ctx->args->outputTarget);
}
static inline void peg_write_interactive_macro(codegen_ctx *ctx) {
  cwrite("#ifndef PGEN_INTERACTIVE\n");
  cwrite("#define PGEN_INTERACTIVE %i\n\n", (int)ctx->args->i);
  cwrite("#define PGEN_ALLOCATOR_DEBUG %i\n\n", (int)ctx->args->m);
  cwrite("#endif /* PGEN_INTERACTIVE */\n\n");
}

static inline void peg_write_interactive_stack(codegen_ctx *ctx) {

  if (ctx->args->i) {
    // Find max name length
    size_t max_len = 0;
    for (size_t n = 0; n < ctx->definitions.len; n++) {
      ASTNode *def = ctx->definitions.buf[n];
      if (strcmp(def->name, "Definition"))
        continue;
      ASTNode *li = def->children[0];
      char *rulename = (char *)li->extra;
      size_t lilen = strlen(rulename);
      max_len = MAX(max_len, lilen);
    }

    for (size_t i = 0; i < ctx->tokendefs.len; i++) {
      ASTNode *def = ctx->tokendefs.buf[i];
      ASTNode *id = def->children[0];
      char *name = (char *)id->extra;
      size_t idlen = strlen(name);
      max_len = MAX(max_len, idlen);
    }

    cwrite("#define PGEN_INTERACTIVE_WIDTH %zu\n", max_len);

    cwrite("typedef struct {\n");
    cwrite("  const char* rule_name;\n");
    cwrite("  size_t pos;\n");
    cwrite("} intr_entry;\n\n");

    cwrite("static struct {\n");
    cwrite("  intr_entry rules[500];\n");
    cwrite("  size_t size;\n");
    cwrite("  int status;\n");
    cwrite("  int first;\n");
    cwrite("} intr_stack;\n\n");

    cwrite("#include <unistd.h>\n");
    cwrite("#include <sys/ioctl.h>\n");
    cwrite("#include <string.h>\n");
    cwrite("static inline void intr_display(%s_parser_ctx* ctx, "
           "const char* last) {\n",
           ctx->lower);
    // Zero initialized
    cwrite("  if (!intr_stack.first) intr_stack.first = 1;\n");
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
    cwrite("  if (intr_stack.status == -1) {\n");
    cwrite("    printf(\"\\x1b[31m\"); // Red\n");
    cwrite("    printf(\"Failed: %%s\\n\", last);\n");
    cwrite("  } else if (intr_stack.status == 0) {\n");
    cwrite("    printf(\"\\x1b[34m\"); // Blue\n");
    cwrite("    printf(\"Entering: %%s\\n\", last);\n");
    cwrite("  } else if (intr_stack.status == 1) {\n");
    cwrite("    printf(\"\\x1b[32m\"); // Green\n");
    cwrite("    printf(\"Accepted: %%s\\n\", last);\n");
    cwrite("  } else {\n");
    cwrite("    printf(\"\\x1b[33m\"); // Green\n");
    cwrite("    printf(\"SUCCED: %%s\\n\", last), exit(1);\n");
    cwrite("  }\n");
    cwrite("  printf(\"\\x1b[0m\"); // Clear Formatting\n\n");

    cwrite("  // Write labels and line.\n");
    cwrite("  for (size_t i = 0; i < width; i++)\n");
    cwrite("    putchar('-');\n\n");

    cwrite("  // Write following lines\n");
    cwrite("  for (size_t i = height; i --> 0;) {\n");
    cwrite("    putchar(' ');\n\n");

    cwrite("    // Print rule stack\n");
    cwrite("    if (i < intr_stack.size) {\n");
    cwrite("      int d = intr_stack.size - height;");
    cwrite("      size_t disp = d > 0 ? i + d : i;");
    cwrite("      printf(\"%%-%zus\", "
           "intr_stack.rules[disp].rule_name);\n",
           max_len);
    cwrite("    } else {\n");
    cwrite("      for (size_t sp = 0; sp < %zu; sp++)\n", max_len);
    cwrite("        putchar(' ');\n");
    cwrite("    }\n\n");

    cwrite("    printf(\" | \"); // Middle bar\n\n");

    cwrite("    // Print tokens\n");
    cwrite("    size_t remaining_tokens = ctx->len - ctx->pos;\n");
    cwrite("    if (i < remaining_tokens) {\n");
    cwrite("      const char* name = "
           "%s_tokenkind_name["
           "ctx->tokens[ctx->pos + i].kind];\n",
           ctx->lower);
    cwrite("      size_t remaining = rightwidth - strlen(name);\n");
    cwrite("      printf(\"%%s\", name);\n");
    cwrite("      for (size_t sp = 0; sp < remaining; sp++)\n");
    cwrite("        putchar(' ');\n");
    cwrite("    }\n\n");

    cwrite("    putchar(' ');\n");
    cwrite("    putchar('\\n');\n");
    cwrite("  }\n");
    cwrite("}\n\n");

    cwrite("static inline void intr_enter(%s_parser_ctx* ctx,"
           " const char* name, size_t pos) {\n",
           ctx->lower);
    cwrite(
        "  intr_stack.rules[intr_stack.size++] = (intr_entry){name, pos};\n");
    cwrite("  intr_stack.status = 0;\n");
    cwrite("  intr_display(ctx, name);\n");
    cwrite("}\n\n");

    cwrite("static inline void intr_accept(%s_parser_ctx* ctx, const char* "
           "accpeting) {\n",
           ctx->lower);
    cwrite("  intr_stack.size--;\n");
    cwrite("  intr_stack.status = 1;\n");
    cwrite("  intr_display(ctx, accpeting);\n");
    cwrite("}\n\n");

    cwrite("static inline void intr_reject(%s_parser_ctx* ctx, const char* "
           "rejecting) {\n",
           ctx->lower);
    cwrite("  intr_stack.size--;\n");
    cwrite("  intr_stack.status = -1;\n");
    cwrite("  intr_display(ctx, rejecting);\n");
    cwrite("}\n");

    cwrite("static inline void intr_succ(%s_parser_ctx* ctx, const char* "
           "succing) {\n",
           ctx->lower);
    cwrite("  intr_stack.size--;\n");
    cwrite("  intr_stack.status = 2;\n");
    cwrite("  intr_display(ctx, succing);\n");
    cwrite("}\n");
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
    for (size_t i = 0; i < expr->num_children; i++) {
      comment("SlashExpr %zu", i);
      iwrite("if (!expr_ret_%zu) ", ret);
      start_block_0(ctx);
      peg_visit_write_exprs(ctx, expr->children[i], ret, capture);
      end_block(ctx);
    }
    comment("SlashExpr end");
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
        int last = (i == expr->num_children - 1);
        comment("ModExprList %zu", i);
        if (i) {
          iwrite("if (expr_ret_%zu) ", ret);
          start_block_0(ctx);
        }
        peg_visit_write_exprs(ctx, expr->children[i], ret, last ? capture : 0);
        if (i)
          end_block(ctx);
      }
    }
    comment("ModExprList end");
    iwrite("if (!expr_ret_%zu) rew(mod_%zu);\n", ret, ret);
    iwrite("expr_ret_%zu = expr_ret_%zu;\n", ret_to, ret);

  } else if (!strcmp(expr->name, "ModExpr")) {
    ModExprOpts opts = *(ModExprOpts *)expr->extra;
    int has_label = expr->num_children >= 2 &&
                    !strcmp(expr->children[1]->name, "LowerIdent");
    ASTNode *label = has_label ? expr->children[1] : NULL;
    int has_errhandler = (expr->num_children - (size_t)has_label) == 2;
    ASTNode *errhandler =
        has_errhandler ? expr->children[has_label ? 2 : 1] : NULL;

    // Simplify
    if ((!opts.inverted) & (!opts.kleene_plus) & (!opts.optional) &
        (!opts.rewind) & (!has_errhandler) & (expr->num_children == 1)) {
      peg_visit_write_exprs(ctx, expr->children[0], ret_to, capture);
      return;
    }

    size_t ret = ctx->expr_cnt++;
    int stateless = opts.inverted | opts.rewind;
    if (stateless)
      iwrite("rec(mexpr_state_%zu)\n", ret);
    iwrite("%s_astnode_t* expr_ret_%zu = NULL;\n", ctx->lower, ret);

    // Get plus or kleene SUCC/NULL or normal node return into ret
    if (opts.kleene_plus == 1) {
      // Plus (match one or more)
      size_t ret = ctx->expr_cnt++;
      iwrite("%s_astnode_t* expr_ret_%zu = NULL;\n", ctx->lower, ret);
      iwrite("int plus_times_%zu = 0;\n", ret);
      iwrite("while (1) ");
      start_block(ctx);
      if (!stateless)
        iwrite("rec(plus_rew_%zu);\n", ret);
      peg_visit_write_exprs(ctx, expr->children[0], ret, 0);
      iwrite("if (!expr_ret_%zu) {\n", ret);
      if (!stateless)
        iwrite("   rew(plus_rew_%zu);\n", ret);
      iwrite("   break;\n");
      iwrite("}");
      iwrite("else { plus_times_%zu++; }\n", ret);
      end_block(ctx);
      iwrite("expr_ret_%zu = plus_times_%zu ? SUCC : NULL;\n", ret_to, ret);

    } else if (opts.kleene_plus == 2) {
      // Kleene closure (match zero or more)
      // Kleene cannot forward capture either. Which one would be returned?
      size_t sentinel = ctx->expr_cnt++;
      iwrite("%s_astnode_t* expr_ret_%zu = SUCC;\n", ctx->lower, sentinel);
      iwrite("while (expr_ret_%zu)\n", sentinel);
      start_block(ctx);
      if (!stateless)
        iwrite("rec(kleene_rew_%zu);\n", ret);
      peg_visit_write_exprs(ctx, expr->children[0], sentinel, 0);
      end_block(ctx);
      iwrite("expr_ret_%zu = SUCC;\n", ret); // Always accepts with SUCC

    } else {
      // No plus or Kleene
      // A BaseExpr can either be forwarded in the simple case (is a Token,
      // Rule, or Code which is only one node), or is a SlashExpr in parens.
      // In that case, we should tell the expressions below to capture the token
      // or rule.
      // Here's where we determine if there's a capture to forward.
      int f = (capture | has_label) & (!opts.optional) & (!opts.inverted);
      peg_visit_write_exprs(ctx, expr->children[0], ret, f);
    }

    // Apply optional/inverted to ret
    if (opts.optional) {
      comment("optional");
      iwrite("if (!expr_ret_%zu)\n", ret);
      iwrite("  expr_ret_%zu = SUCC;\n", ret);
    } else if (opts.inverted) {
      comment("invert");
      iwrite("expr_ret_%zu = expr_ret_%zu ? NULL : SUCC;\n", ret, ret);
    }

    // Handle errors
    if (errhandler) {
      if (!strcmp(errhandler->name, "ErrString")) {
        codepoint_t *cps = (codepoint_t *)errhandler->extra;
        String_View sv =
            UTF8_encode_view((Codepoint_String_View){cps, cpstrlen(cps)});
        iwrite("if (!expr_ret_%zu) {\n", ret);
        iwrite("  FATAL(\"%s\");\n", sv.str);
        iwrite("  return NULL;\n");
        iwrite("}\n");
        free(sv.str);
      } else {
        iwrite("if (!expr_ret_%zu) ", ret);
        start_block_0(ctx);
        size_t err_val = ctx->expr_cnt++;
        iwrite("%s_astnode_t* expr_ret_%zu = NULL;\n", ctx->lower, err_val);
        peg_visit_write_exprs(ctx, errhandler, err_val, 0);
        iwrite("return expr_ret_%zu==SUCC ? NULL : expr_ret_%zu;\n", err_val,
               err_val);

        end_block(ctx);
      }
    }

    // Rewind if applicable
    if (stateless) {
      comment("rewind");
      iwrite("rew(mexpr_state_%zu);\n", ret);
    }

    // Copy ret into ret_to and label if applicable
    iwrite("expr_ret_%zu = expr_ret_%zu;\n", ret_to, ret);
    if (has_label) {
      char *label_name = (char *)expr->children[1]->extra;
      iwrite("%s = expr_ret_%zu;\n", label_name, ret);
    }

  } else if (!strcmp(expr->name, "BaseExpr")) {
    peg_visit_write_exprs(ctx, expr->children[0], ret_to, capture);

  } else if (!strcmp(expr->name, "UpperIdent")) {
    char *tokname = (char *)expr->extra;
    if (ctx->args->i)
      iwrite("intr_enter(ctx, \"%s\", ctx->pos);\n", tokname);
    iwrite(
        "if (ctx->pos < ctx->len && ctx->tokens[ctx->pos].kind == %s_TOK_%s) ",
        ctx->upper, tokname);
    start_block_0(ctx);
    if (capture) {
      if (!ctx->args->u)
        comment("Capturing %s.", tokname);
      iwrite("expr_ret_%zu = leaf(%s);\n", ret_to, tokname);
      iwrite("expr_ret_%zu->tok_repr = ctx->tokens[ctx->pos].content;\n",
             ret_to);
      iwrite("expr_ret_%zu->repr_len = ctx->tokens[ctx->pos].len;\n", ret_to);
      if (!ctx->args->u)
        peg_ensure_kind(ctx, tokname);
    } else {
      if (!ctx->args->u)
        comment("Not capturing %s.", tokname);
      iwrite("expr_ret_%zu = SUCC;\n", ret_to);
    }
    iwrite("ctx->pos++;\n");
    end_block_0(ctx);
    cwrite(" else ");
    start_block_0(ctx);
    iwrite("expr_ret_%zu = NULL;\n", ret_to);
    end_block(ctx);
    if (ctx->args->i)
      iwrite(
          "if (expr_ret_%zu) intr_accept(ctx, \"%s\"); else intr_reject(ctx, "
          "\"%s\");\n",
          ret_to, tokname, tokname);
  } else if (!strcmp(expr->name, "LowerIdent")) {
    iwrite("expr_ret_%zu = %s_parse_%s(ctx);\n", ret_to, ctx->lower,
           (char *)expr->extra);
    iwrite("if (ctx->exit) return NULL;\n");
  } else if (!strcmp(expr->name, "CodeExpr")) {
    // No need to respect capturing for a CodeExpr.
    // The user will allocate their own with node() or list() if they want to.
    // At the start of the block we'll write SUCC to ret_to. If that's not okay,
    // they can override that on their own with NULL or node() or list() or
    // whatever.
    comment("CodeExpr");
    if (ctx->args->i)
      iwrite("intr_enter(ctx, \"CodeExpr\", ctx->pos);\n");
    iwrite("#define ret expr_ret_%zu\n", ret_to);
    iwrite("ret = SUCC;\n");
    CodeExprOpts *opts = (CodeExprOpts *)expr->extra;
    if (ctx->args->l)
      start_embed(ctx, opts->line_nbr);
    iwrite("%s;\n", opts->content);
    if (ctx->args->l)
      end_embed(ctx);
    if (ctx->args->i)
      iwrite("if (ret) intr_accept(ctx, \"CodeExpr\"); else intr_reject(ctx, "
             "\"CodeExpr\");\n");
    iwrite("#undef ret\n");
  } else {
    ERROR("UNREACHABLE ERROR. UNKNOWN NODE TYPE:\n %s\n", expr->name);
  }
}

static inline void peg_write_definition(codegen_ctx *ctx, ASTNode *def) {
  char *def_name = (char *)def->children[0]->extra;
  ASTNode *def_expr = def->children[1];

  cwrite("static inline %s_astnode_t* %s_parse_%s(%s_parser_ctx* ctx) {\n",
         ctx->lower, ctx->lower, def_name, ctx->lower);

  // Visit labels, write variables.
  if (def->num_children == 3) {
    ASTNode *sdefs = def->children[2];
    for (size_t i = 0; i < sdefs->num_children; i++)
      cwrite("  %s;\n", (char *)sdefs->children[i]->extra);
    cwrite("\n");
  }

  list_cstr ids = list_cstr_new();
  peg_visit_add_labels(ctx, def_expr, &ids);
  for (size_t i = 0; i < ids.len; i++)
    cwrite("  %s_astnode_t* %s = NULL;\n", ctx->lower, ids.buf[i]);
  list_cstr_clear(&ids);

  size_t ult_ret = ctx->expr_cnt++;
  size_t ret = ctx->expr_cnt++;
  cwrite("  #define rule expr_ret_%zu\n", ult_ret);
  cwrite("  %s_astnode_t* expr_ret_%zu = NULL;\n", ctx->lower, ult_ret);
  cwrite("  %s_astnode_t* expr_ret_%zu = NULL;\n", ctx->lower, ret);
  if (ctx->args->i)
    iwrite("intr_enter(ctx, \"%s\", ctx->pos);\n", def_name);

  peg_visit_write_exprs(ctx, def_expr, ret, 1);

  iwrite("if (!rule) rule = expr_ret_%zu;\n", ret);
  iwrite("if (!expr_ret_%zu) rule = NULL;\n", ret);

  if (ctx->args->i) {
    iwrite("if (rule==SUCC) intr_succ(ctx, \"%s\");\n", def_name);
    iwrite("else if (rule) intr_accept(ctx, \"%s\");\n", def_name);
    iwrite("else intr_reject(ctx, \"%s\");\n", def_name);
  } else if (ctx->args->d) {
    iwrite("if (rule==SUCC) fprintf(stderr, \"ERROR: Rule %s returned "
           "SUCC.\\n\"), exit(1);\n",
           def_name);
  }
  cwrite("  return rule;\n");
  cwrite("  #undef rule\n");
  cwrite("}\n\n");
}

static inline void peg_write_parser_body(codegen_ctx *ctx) {
  // Write stubs

  for (size_t n = 0; n < ctx->definitions.len; n++) {
    ASTNode *def = ctx->definitions.buf[n];
    peg_write_definition_stub(ctx, def);
  }
  cwrite("\n\n");

  // Write bodies
  for (size_t n = 0; n < ctx->definitions.len; n++) {
    peg_write_definition(ctx, ctx->definitions.buf[n]);
  }
  cwrite("\n\n");
}

static inline void peg_write_undef_parsermacros(codegen_ctx *ctx) {
  cwrite("#undef rec\n");
  cwrite("#undef rew\n");
  cwrite("#undef node\n");
  cwrite("#undef kind\n");
  cwrite("#undef list\n");
  cwrite("#undef leaf\n");
  cwrite("#undef add\n");
  cwrite("#undef has\n");
  cwrite("#undef expect\n");
  cwrite("#undef repr\n");
  cwrite("#undef srepr\n");
  cwrite("#undef cprepr\n");
  cwrite("#undef rret\n");
  cwrite("#undef SUCC\n\n");

  cwrite("#undef PGEN_MIN\n");
  cwrite("#undef PGEN_MAX\n");
  for (size_t i = 1; i < NODE_NUM_FIXED + 1; i++) {
    cwrite("#undef PGEN_MIN%zu\n", i);
    cwrite("#undef PGEN_MAX%zu\n", i);
  }
  cwrite("\n");

  cwrite("#undef LB\n");
  cwrite("#undef RB\n");
  cwrite("\n");

  cwrite("#undef INFO\n");
  cwrite("#undef INFO_F\n");
  cwrite("#undef WARNING\n");
  cwrite("#undef WARNING_F\n");
  cwrite("#undef ERROR\n");
  cwrite("#undef ERROR_F\n");
  cwrite("#undef FATAL\n");
  cwrite("#undef FATAL_F\n");
}

static inline void peg_write_parser(codegen_ctx *ctx) {
  peg_write_header(ctx);
  peg_write_parser_errdef(ctx);
  peg_write_parser_ctx(ctx);
  peg_write_parser_ctx_init(ctx);
  peg_write_report_parse_error(ctx);
  peg_write_astnode_kind(ctx);
  peg_write_astnode_def(ctx);
  peg_write_minmax(ctx);
  peg_write_astnode_init(ctx);
  peg_write_astnode_add(ctx);
  peg_write_parser_rewind(ctx);
  peg_write_repr(ctx);
  peg_write_node_print(ctx);
  peg_write_astnode_print(ctx);
  peg_write_parsermacros(ctx);
  peg_write_middirectives(ctx);
  peg_write_interactive_stack(ctx);
  peg_write_parser_body(ctx);
  peg_write_postdirectives(ctx);
  peg_write_undef_parsermacros(ctx);
  peg_write_footer(ctx);
}

/**************/
/* Everything */
/**************/

static inline void codegen_write(codegen_ctx *ctx) {

  // Write headers
  write_utf8_lib(ctx);

  peg_write_interactive_macro(ctx);
  write_arena_lib(ctx);
  peg_write_predirectives(ctx);
  write_helpermacros(ctx);

  // Write bodies
  codegen_write_tokenizer(ctx);
  peg_write_parser(ctx);
}

#endif /* TOKCODEGEN_INCLUDE */
