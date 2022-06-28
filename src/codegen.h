#ifndef TOKCODEGEN_INCLUDE
#define TOKCODEGEN_INCLUDE
#include "argparse.h"
#include "ast.h"
#include "automata.h"
#include "list.h"
#include "parserctx.h"
#include "utf8.h"

#define NODE_NUM_FIXED 10

/*******/
/* ctx */
/*******/

#define PGEN_PREFIX_LEN 8
typedef struct {
  FILE *f;
  ASTNode *tokast;
  ASTNode *pegast;
  TrieAutomaton trie;
  list_SMAutomaton smauts;
  char prefix_lower[PGEN_PREFIX_LEN];
  char prefix_upper[PGEN_PREFIX_LEN];
} codegen_ctx;

static inline void codegen_ctx_init(codegen_ctx *ctx, Args args,
                                    ASTNode *tokast, ASTNode *pegast,
                                    TrieAutomaton trie,
                                    list_SMAutomaton smauts) {

  ctx->tokast = tokast;
  ctx->pegast = pegast;
  ctx->trie = trie;
  ctx->smauts = smauts;

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
    if ((low != '_') & ((low < 'a') | (low > 'z')))
      if ((low < '0') | (low > '9'))
        break;

    ctx->prefix_lower[i] = low;
    ctx->prefix_upper[i] = up;
  }
  ctx->prefix_lower[i] = '\0';
  ctx->prefix_upper[i] = '\0';

  // Create/open the file prefix.h if -o was not an argument,
  // otherwise the -o target.
  char namebuf[PGEN_PREFIX_LEN + 2]; // Doesn't overflow.
  char *write_to;
  if (!args.outputTarget) {
    sprintf(namebuf, "%s.h", ctx->prefix_lower);
    write_to = namebuf;
  } else {
    write_to = args.outputTarget;
  }

  ctx->f = fopen(write_to, "w");
  if (!ctx->f) {
    ERROR("Could not write to %s.", namebuf);
  }
}

static inline void codegen_ctx_destroy(codegen_ctx *ctx) {
  fclose(ctx->f);
  ASTNode_destroy(ctx->tokast);
  if (ctx->pegast)
    ASTNode_destroy(ctx->pegast);
}

/****************/
/* UTF8 Library */
/****************/
static inline void write_utf8_lib(codegen_ctx *ctx) {
#include "strutf8.xxd"
  fprintf(ctx->f, "%s", (char *)src_utf8_h);
  fprintf(ctx->f, "\n\n");
}

/************************/
/* AST Memory Allocator */
/************************/
static inline void write_arena_lib(codegen_ctx *ctx) {
#include "strarena.xxd"
  fprintf(ctx->f, "%s", (char *)src_arena_h);
  fprintf(ctx->f, "\n\n");
}

/************************/
/* Parser Helper Macros */
/************************/
static inline void write_helpermacros(codegen_ctx *ctx) {
  fprintf(ctx->f, "#ifndef PGEN_PARSER_MACROS_INCLUDED\n");
  fprintf(ctx->f, "#define PGEN_PARSER_MACROS_INCLUDED\n");
  fprintf(ctx->f, "#define PGEN_CAT_(x, y) x##y\n");
  fprintf(ctx->f, "#define PGEN_CAT(x, y) PGEN_CAT_(x, y)\n");
  fprintf(ctx->f, "#define PGEN_NARG(...) "
                  "PGEN_NARG_(__VA_ARGS__, PGEN_RSEQ_N())\n");
  fprintf(ctx->f, "#define PGEN_NARG_(...) PGEN_128TH_ARG(__VA_ARGS__)\n");

  fprintf(ctx->f, "#define PGEN_128TH_ARG(                 "
                  "                                       \\\n");
  fprintf(ctx->f, "    _1, _2, _3, _4, _5, _6, _7, _8, _9, "
                  "_10, _11, _12, _13, _14, _15, _16,     \\\n");
  fprintf(ctx->f, "    _17, _18, _19, _20, _21, _22, _23, "
                  "_24, _25, _26, _27, _28, _29, _30, _31, \\\n");
  fprintf(ctx->f, "    _32, _33, _34, _35, _36, _37, _38, "
                  "_39, _40, _41, _42, _43, _44, _45, _46, \\\n");
  fprintf(ctx->f, "    _47, _48, _49, _50, _51, _52, _53, "
                  "_54, _55, _56, _57, _58, _59, _60, _61, \\\n");
  fprintf(ctx->f, "    _62, _63, _64, _65, _66, _67, _68, "
                  "_69, _70, _71, _72, _73, _74, _75, _76, \\\n");
  fprintf(ctx->f, "    _77, _78, _79, _80, _81, _82, _83, "
                  "_84, _85, _86, _87, _88, _89, _90, _91, \\\n");
  fprintf(ctx->f, "    _92, _93, _94, _95, _96, _97, _98, "
                  "_99, _100, _101, _102, _103, _104,      \\\n");
  fprintf(ctx->f, "    _105, _106, _107, _108, _109, _110, "
                  "_111, _112, _113, _114, _115, _116,    \\\n");
  fprintf(ctx->f, "    _117, _118, _119, _120, _121, _122, "
                  "_123, _124, _125, _126, _127, N, ...)  \\\n");
  fprintf(ctx->f, "  N\n");

  fprintf(ctx->f, "#define PGEN_RSEQ_N()                                       "
                  "                   \\\n");
  fprintf(ctx->f, "  127, 126, 125, 124, 123, 122, 121, 120,"
                  " 119, 118, 117, 116, 115, 114, 113,   \\\n");
  fprintf(ctx->f, "      112, 111, 110, 109, 108, 107, 106, "
                  "105, 104, 103, 102, 101, 100, 99, 98, \\\n");
  fprintf(ctx->f, "      97, 96, 95, 94, 93, 92, 91, 90, 89,"
                  " 88, 87, 86, 85, 84, 83, 82, 81, 80,  \\\n");
  fprintf(ctx->f, "      79, 78, 77, 76, 75, 74, 73, 72, 71,"
                  " 70, 69, 68, 67, 66, 65, 64, 63, 62,  \\\n");
  fprintf(ctx->f, "      61, 60, 59, 58, 57, 56, 55, 54, 53,"
                  " 52, 51, 50, 49, 48, 47, 46, 45, 44,  \\\n");
  fprintf(ctx->f, "      43, 42, 41, 40, 39, 38, 37, 36, 35,"
                  " 34, 33, 32, 31, 30, 29, 28, 27, 26,  \\\n");
  fprintf(ctx->f, "      25, 24, 23, 22, 21, 20, 19, 18, 17,"
                  " 16, 15, 14, 13, 12, 11, 10, 9, 8, 7, \\\n");
  fprintf(ctx->f, "      6, 5, 4, 3, 2, 1, 0\n");
  fprintf(ctx->f, "#endif /* PGEN_PARSER_MACROS_INCLUDED */\n\n");
}

/*************/
/* Tokenizer */
/*************/

static inline void tok_write_header(codegen_ctx *ctx) {
  fprintf(ctx->f,
          "#ifndef %s_TOKENIZER_INCLUDE\n"
          "#define %s_TOKENIZER_INCLUDE\n"
          "\n"
          "#ifndef %s_TOKENIZER_SOURCEINFO\n"
          "#define %s_TOKENIZER_SOURCEINFO 1\n"
          "#endif\n"
          "\n",
          ctx->prefix_upper, ctx->prefix_upper, ctx->prefix_upper,
          ctx->prefix_upper);
}

static inline void tok_write_enum(codegen_ctx *ctx) {
  fprintf(ctx->f, "typedef enum {\n");

  size_t num_defs = ctx->tokast->num_children;
  fprintf(ctx->f, "  %s_TOK_STREAMEND,\n", ctx->prefix_upper);
  for (size_t i = 0; i < num_defs; i++)
    fprintf(ctx->f, "  %s_TOK_%s,\n", ctx->prefix_upper,
            (char *)(ctx->tokast->children[i]->children[0]->extra));

  fprintf(ctx->f, "} %s_token_kind;\n\n", ctx->prefix_lower);

  fprintf(ctx->f,
          "// The 0th token is end of stream.\n// Tokens 1 through %zu are the "
          "ones you defined.\n// This totals %zu kinds of tokens.\n",
          num_defs, num_defs + 1);
  fprintf(ctx->f, "static size_t %s_num_tokens = %zu;\n", ctx->prefix_lower,
          num_defs + 1);
  fprintf(ctx->f,
          "static const char* %s_kind_name[] = {\n  \"%s_TOK_STREAMEND\",\n",
          ctx->prefix_lower, ctx->prefix_upper);
  for (size_t i = 0; i < num_defs; i++)
    fprintf(ctx->f, "  \"%s_TOK_%s\",\n", ctx->prefix_upper,
            (char *)(ctx->tokast->children[i]->children[0]->extra));
  fprintf(ctx->f, "};\n\n");
}

static inline void tok_write_tokenstruct(codegen_ctx *ctx) {
  fprintf(ctx->f,
          "typedef struct {\n"
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
          ctx->prefix_lower, ctx->prefix_upper, ctx->prefix_upper,
          ctx->prefix_upper, ctx->prefix_lower);
}

static inline void tok_write_ctxstruct(codegen_ctx *ctx) {
  fprintf(ctx->f,
          "typedef struct {\n"
          "  codepoint_t* start;\n"
          "  size_t len;\n"
          "  size_t pos;\n"
          "#if %s_TOKENIZER_SOURCEINFO\n"
          "  size_t pos_line;\n"
          "  size_t pos_col;\n"
          "  char* pos_sourceFile;\n"
          "#endif\n"
          "} %s_tokenizer;\n\n",
          ctx->prefix_upper, ctx->prefix_lower);

  fprintf(ctx->f,
          "static inline void %s_tokenizer_init(%s_tokenizer* tokenizer, "
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
          ctx->prefix_lower, ctx->prefix_lower, ctx->prefix_upper);
}

static inline void tok_write_statecheck(codegen_ctx *ctx, size_t smaut_num,
                                        list_int states) {

  // Single int
  if (states.len == 1) {
    fprintf(ctx->f, "smaut_state_%zu == %i", smaut_num, states.buf[0]);
  }

  // Charset
  else {
    fprintf(ctx->f, "(smaut_state_%zu == %i)", smaut_num, states.buf[0]);
    for (size_t i = 1; i < states.len; i++)
      fprintf(ctx->f, " | (smaut_state_%zu == %i)", smaut_num, states.buf[i]);
  }
}

static inline void tok_write_charsetcheck(codegen_ctx *ctx, ASTNode *charset) {

  // Single char
  if (!charset->num_children) {
    // printf("%s\n", charset->name);
    if (strcmp(charset->name, "CharSet") == 0) {
      fprintf(ctx->f, "%i", (int)*(bool *)charset->extra);
    } else { // single char
      fprintf(ctx->f, "c == %" PRI_CODEPOINT "",
              *(codepoint_t *)charset->extra);
    }
  }

  // Charset
  else {
    bool inverted = *(bool *)charset->extra;
    if (!charset->num_children) {
      fprintf(ctx->f, "%i", (int)inverted);
      return;
    }

    if (inverted)
      fprintf(ctx->f, "!(");

    for (size_t i = 0; i < charset->num_children; i++) {
      ASTNode *child = charset->children[i];
      if (strcmp(child->name, "Char") == 0) {
        codepoint_t c = *(codepoint_t *)child->extra;
        if (charset->num_children == 1) {
          // If it's the only one, it doensn't need extra parens.
          fprintf(ctx->f, "c == %" PRI_CODEPOINT, c);
        } else {
          fprintf(ctx->f, "(c == %" PRI_CODEPOINT ")", c);
        }
      } else {
        codepoint_t *cpptr = (codepoint_t *)child->extra;
        codepoint_t r1 = *cpptr, r2 = *(cpptr + 1);
        codepoint_t c1 = MIN(r1, r2), c2 = MAX(r1, r2);
        fprintf(ctx->f,
                "((c >= %" PRI_CODEPOINT ") & (c <= %" PRI_CODEPOINT "))", c1,
                c2);
      }
      if (i != charset->num_children - 1)
        fprintf(ctx->f, " | ");
    }

    if (inverted)
      fprintf(ctx->f, ")");
  }
}

static inline void tok_write_nexttoken(codegen_ctx *ctx) {
  // See tokenizer.txt.

  TrieAutomaton trie = ctx->trie;
  list_SMAutomaton smauts = ctx->smauts;
  int has_trie = trie.accepting.len ? 1 : 0;
  int has_smauts = smauts.len ? 1 : 0;

  fprintf(ctx->f,
          "static inline %s_token %s_nextToken(%s_tokenizer* tokenizer) {\n"
          "  codepoint_t* current = tokenizer->start + tokenizer->pos;\n"
          "  size_t remaining = tokenizer->len - tokenizer->pos;\n\n",
          ctx->prefix_lower, ctx->prefix_lower, ctx->prefix_lower);

  // Variables for each automaton for the current run.
  if (has_trie)
    fprintf(ctx->f, "  int trie_state = 0;\n");
  if (has_smauts) {
    for (size_t i = 0; i < smauts.len; i++)
      fprintf(ctx->f, "  int smaut_state_%zu = 0;\n", i);
  }

  if (has_trie)
    fprintf(ctx->f, "  size_t trie_munch_size = 0;\n");
  if (has_smauts) {
    for (size_t i = 0; i < smauts.len; i++)
      fprintf(ctx->f, "  size_t smaut_munch_size_%zu = 0;\n", i);
  }

  if (has_trie)
    fprintf(ctx->f, "  %s_token_kind trie_tokenkind = %s_TOK_STREAMEND;\n\n",
            ctx->prefix_lower, ctx->prefix_upper);
  else
    fprintf(ctx->f, "\n");

  // Outer loop
  fprintf(ctx->f, "  for (size_t iidx = 0; iidx < remaining; iidx++) {\n");
  fprintf(ctx->f, "    codepoint_t c = current[iidx];\n");
  fprintf(ctx->f, "    int all_dead = 1;\n\n");

  // Inner loop (automaton, unrolled)
  // Trie aut
  if (has_trie) {
    fprintf(ctx->f, "    // Trie\n");
    fprintf(ctx->f, "    if (trie_state != -1) {\n");
    fprintf(ctx->f, "      all_dead = 0;\n");

    // Group runs of to.
    int eels = 0;
    for (size_t i = 0; i < trie.trans.len;) {
      int els = 0;
      int from = trie.trans.buf[i].from;
      fprintf(ctx->f, "      %sif (trie_state == %i) {\n",
              eels++ ? "else " : "", from);
      while (i < trie.trans.len && trie.trans.buf[i].from == from) {
        codepoint_t c = trie.trans.buf[i].c;
        int to = trie.trans.buf[i].to;
        const char *lsee = els++ ? "else " : "";
        if (c == '\n')
          fprintf(ctx->f,
                  "        %sif (c == %" PRI_CODEPOINT " /*'\\n'*/"
                  ") trie_state = %i;\n",
                  lsee, c, to);
        else if (c <= 127)
          fprintf(ctx->f,
                  "        %sif (c == %" PRI_CODEPOINT " /*'%c'*/"
                  ") trie_state = %i;\n",
                  lsee, c, (char)c, to);
        else
          fprintf(ctx->f,
                  "        %sif (c == %" PRI_CODEPOINT ") trie_state = %i;\n",
                  lsee, c, to);

        i++;
      }
      fprintf(ctx->f, "        else trie_state = -1;\n");
      fprintf(ctx->f, "      }\n");
    }
    fprintf(ctx->f, "      else {\n");
    fprintf(ctx->f, "        trie_state = -1;\n");
    fprintf(ctx->f, "      }\n\n");

    eels = 0;
    fprintf(ctx->f, "      // Check accept\n");
    for (size_t i = 0; i < trie.accepting.len; i++) {
      fprintf(ctx->f, "      %sif (trie_state == %i) {\n",
              eels++ ? "else " : "", trie.accepting.buf[i].num);
      fprintf(ctx->f, "        trie_tokenkind =  %s_TOK_%s;\n",
              ctx->prefix_upper,
              (char *)trie.accepting.buf[i].rule->children[0]->extra);
      fprintf(ctx->f, "        trie_munch_size = iidx + 1;\n");
      fprintf(ctx->f, "      }\n");
    }
    fprintf(ctx->f, "    }\n\n"); // End of Trie aut
  }

  // SM auts
  if (has_smauts) {
    for (size_t a = 0; a < smauts.len; a++) {
      SMAutomaton aut = smauts.buf[a];
      fprintf(ctx->f, "    // Transition %s State Machine\n", aut.ident);
      fprintf(ctx->f, "    if (smaut_state_%zu != -1) {\n", a);
      fprintf(ctx->f, "      all_dead = 0;\n\n");

      int eels = 0;
      for (size_t i = 0; i < aut.trans.len; i++) {
        SMTransition trans = list_SMTransition_get(&aut.trans, i);

        fprintf(ctx->f, "      %sif ((", eels++ ? "else " : "");
        tok_write_statecheck(ctx, a, trans.from);
        fprintf(ctx->f, ") &\n         (");
        tok_write_charsetcheck(ctx, trans.act);
        fprintf(ctx->f, ")) {\n");

        fprintf(ctx->f, "          smaut_state_%zu = %i;\n", a, trans.to);

        fprintf(ctx->f, "      }\n");
      }
      fprintf(ctx->f, "      else {\n");
      fprintf(ctx->f, "        smaut_state_%zu = -1;\n", a);
      fprintf(ctx->f, "      }\n\n");

      fprintf(ctx->f, "      // Check accept\n");

      fprintf(ctx->f, "      if (");
      tok_write_statecheck(ctx, a, aut.accepting);
      fprintf(ctx->f, ") {\n");
      fprintf(ctx->f, "        smaut_munch_size_%zu = iidx + 1;\n", a);
      fprintf(ctx->f, "      }\n");

      fprintf(ctx->f, "    }\n\n");
    }
  }
  fprintf(ctx->f, "    if (all_dead)\n");
  fprintf(ctx->f, "      break;\n");
  fprintf(ctx->f, "  }\n\n"); // For each remaining character

  fprintf(ctx->f, "  // Determine what token was accepted, if any.\n");
  fprintf(ctx->f, "  %s_token_kind kind = %s_TOK_STREAMEND;\n",
          ctx->prefix_lower, ctx->prefix_upper);
  fprintf(ctx->f, "  size_t max_munch = 0;\n");
  if (has_smauts) {
    for (size_t i = smauts.len; i-- > 0;) {
      SMAutomaton aut = smauts.buf[i];
      fprintf(ctx->f, "  if (smaut_munch_size_%zu >= max_munch) {\n", i);
      fprintf(ctx->f, "    kind = %s_TOK_%s;\n", ctx->prefix_upper, aut.ident);
      fprintf(ctx->f, "    max_munch = smaut_munch_size_%zu;\n", i);
      fprintf(ctx->f, "  }\n");
    }
  }
  if (has_trie) {
    fprintf(ctx->f, "  if (trie_munch_size >= max_munch) {\n");
    fprintf(ctx->f, "    kind = trie_tokenkind;\n");
    fprintf(ctx->f, "    max_munch = trie_munch_size;\n");
    fprintf(ctx->f, "  }\n");
  }
  fprintf(ctx->f, "\n");

  fprintf(ctx->f, "  %s_token ret;\n", ctx->prefix_lower);
  fprintf(ctx->f, "  ret.kind = kind;\n");
  fprintf(ctx->f, "  ret.start = tokenizer->pos;\n");
  fprintf(ctx->f, "  ret.len = max_munch;\n\n");

  fprintf(ctx->f, "#if %s_TOKENIZER_SOURCEINFO\n", ctx->prefix_upper);
  fprintf(ctx->f, "  ret.line = tokenizer->pos_line;\n");
  fprintf(ctx->f, "  ret.col = tokenizer->pos_col;\n");
  fprintf(ctx->f, "  ret.sourceFile = tokenizer->pos_sourceFile;\n");
  fprintf(ctx->f, "\n");
  fprintf(ctx->f, "  for (size_t i = 0; i < ret.len; i++) {\n");
  fprintf(ctx->f, "    if (current[i] == '\\n') {\n");
  fprintf(ctx->f, "      tokenizer->pos_line++;\n");
  fprintf(ctx->f, "      tokenizer->pos_col = 0;\n");
  fprintf(ctx->f, "    } else {\n");
  fprintf(ctx->f, "      tokenizer->pos_col++;\n");
  fprintf(ctx->f, "    }\n");
  fprintf(ctx->f, "  }\n");
  fprintf(ctx->f, "#endif\n\n");

  fprintf(ctx->f, "  tokenizer->pos += max_munch;\n");
  fprintf(ctx->f, "  return ret;\n");
  fprintf(ctx->f, "}\n\n");
}

static inline void tok_write_footer(codegen_ctx *ctx) {
  fprintf(ctx->f, "#endif /* %s_TOKENIZER_INCLUDE */\n\n", ctx->prefix_upper);
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
  fprintf(ctx->f, "struct %s_astnode_t;\n", ctx->prefix_lower);
  fprintf(ctx->f, "typedef struct %s_astnode_t %s_astnode_t;\n",
          ctx->prefix_lower, ctx->prefix_lower);
  fprintf(ctx->f, "\n");
  ASTNode *pegast = ctx->pegast;
  for (size_t i = 0; i < pegast->num_children; i++) {
    ASTNode *child = pegast->children[i];
    ASTNode *rident = child->children[0];
    ASTNode *strucdef = child->num_children == 3 ? child->children[2] : NULL;

    fprintf(ctx->f, "typedef struct {\n");
    if (strucdef) {
      for (size_t j = 0; j < strucdef->num_children; j++) {
        fprintf(ctx->f, "  %s_astnode_t* %s;\n", ctx->prefix_lower,
                (char *)strucdef->children[j]->children[0]->extra);
      }
    }
    fprintf(ctx->f, "} %s_astnode_t;\n\n", (char *)rident->extra);
  }
}

static inline void peg_write_header(codegen_ctx *ctx) {
  fprintf(ctx->f, "#ifndef PGEN_%s_ASTNODE_INCLUDE\n", ctx->prefix_upper);
  fprintf(ctx->f, "#define PGEN_%s_ASTNODE_INCLUDE\n\n", ctx->prefix_upper);
}

static inline void peg_write_footer(codegen_ctx *ctx) {
  fprintf(ctx->f, "#endif /* PGEN_%s_ASTNODE_INCLUDE */\n\n",
          ctx->prefix_upper);
}

static inline void peg_write_directives(codegen_ctx *ctx) {
  int oom_written = 0;

  ASTNode *pegast = ctx->pegast;
  if (pegast->num_children)
    fprintf(ctx->f, "/**************/\n/* Directives */\n/**************/\n");
  for (size_t n = 0; n < pegast->num_children; n++) {
    ASTNode *dir = pegast->children[n];
    if (strcmp(dir->name, "Directive"))
      continue;

    // %define directive
    if (!strcmp((char *)dir->children[0]->extra, "define")) {
      fprintf(ctx->f, "#define %s\n", (char *)dir->extra);
    }
    // %oom directive
    else if (!strcmp((char *)dir->children[0]->extra, "oom")) {
      if (oom_written)
        ERROR("Duplicate %%oom directives.");

      fprintf(ctx->f, "#define PGEN_OOM() %s\n", (char *)dir->extra);
      oom_written = 1;
    }
    // %include directive
    else if (!strcmp((char *)dir->children[0]->extra, "include")) {
      fprintf(ctx->f, "#include %s\n", (char *)dir->extra);
    }
    // %code directive
    else if (!strcmp((char *)dir->children[0]->extra, "code")) {
      fprintf(ctx->f, "%s\n", (char *)dir->extra);
    }
  }
  fprintf(ctx->f, "\n");
}

static inline void peg_write_parser_ctx(codegen_ctx *ctx) {
  fprintf(ctx->f, "typedef struct {\n");
  fprintf(ctx->f, "  %s_token* tokens;\n", ctx->prefix_lower);
  fprintf(ctx->f, "  size_t len;\n");
  fprintf(ctx->f, "  size_t pos;\n");
  fprintf(ctx->f, "  pgen_allocator *alloc;\n");
  fprintf(ctx->f, "} %s_parser_ctx;\n\n", ctx->prefix_lower);
}

static inline void peg_write_astnode_def(codegen_ctx *ctx) {
  fprintf(ctx->f, "struct %s_astnode_t;\n", ctx->prefix_lower);
  fprintf(ctx->f, "typedef struct %s_astnode_t %s_astnode_t;\n",
          ctx->prefix_lower, ctx->prefix_lower);
  fprintf(ctx->f, "struct %s_astnode_t {\n", ctx->prefix_lower);
  fprintf(ctx->f, "#ifdef PGEN_%s_NODE_EXTRA\n", ctx->prefix_upper);
  fprintf(ctx->f, "  PGEN_%s_NODE_EXTRA\n", ctx->prefix_upper);
  fprintf(ctx->f, "#endif\n");
  fprintf(ctx->f, "  const char* kind;\n");
  fprintf(ctx->f, "  size_t num_children;\n");
  fprintf(ctx->f, "  size_t max_children;\n");
  fprintf(ctx->f, "  %s_astnode_t** children;\n", ctx->prefix_lower);
  fprintf(ctx->f, "  pgen_allocator_rewind_t rew;\n");
  fprintf(ctx->f, "};\n\n");
}

static inline void peg_write_astnode_init(codegen_ctx *ctx) {

  fprintf(ctx->f, "static inline %s_astnode_t* %s_astnode_list(\n",
          ctx->prefix_lower, ctx->prefix_lower);
  fprintf(ctx->f, "                             pgen_allocator* alloc,\n"
                  "                             const char* kind,\n"
                  "                             size_t initial_size) {\n");
  fprintf(ctx->f,
          "  pgen_allocator_ret_t ret = pgen_alloc(alloc,\n"
          "                                        sizeof(%s_astnode_t),\n"
          "                                        _Alignof(%s_astnode_t));\n",
          ctx->prefix_lower, ctx->prefix_lower);
  fprintf(ctx->f, "  %s_astnode_t *node = (pl0_astnode_t*)ret.buf;\n\n",
          ctx->prefix_lower);
  fprintf(ctx->f, "  %s_astnode_t *children;\n", ctx->prefix_lower);
  fprintf(ctx->f, "  if (initial_size) {\n");
  fprintf(ctx->f,
          "    children = (%s_astnode_t*)"
          "malloc(sizeof(%s_astnode_t) * initial_size);\n",
          ctx->prefix_lower, ctx->prefix_lower);
  fprintf(ctx->f, "    if (!children)\n      PGEN_OOM();\n");
  fprintf(ctx->f, "    pgen_defer(alloc, free, children, ret.rew);\n");
  fprintf(ctx->f, "  } else {\n");
  fprintf(ctx->f, "    children = NULL;\n");
  fprintf(ctx->f, "  }\n\n");
  fprintf(ctx->f, "  node->kind = kind;\n");
  fprintf(ctx->f, "  node->max_children = initial_size;\n");
  fprintf(ctx->f, "  node->num_children = 0;\n");
  fprintf(ctx->f, "  node->children = NULL;\n");
  fprintf(ctx->f, "  node->rew = ret.rew;\n");
  fprintf(ctx->f, "  return node;\n");
  fprintf(ctx->f, "}\n\n");

  fprintf(ctx->f, "static inline %s_astnode_t* %s_astnode_leaf(\n",
          ctx->prefix_lower, ctx->prefix_lower);
  fprintf(ctx->f, "                             pgen_allocator* alloc,\n"
                  "                             const char* kind) {\n");
  fprintf(ctx->f,
          "  pgen_allocator_ret_t ret = pgen_alloc(alloc,\n"
          "                                        sizeof(%s_astnode_t),\n"
          "                                        _Alignof(%s_astnode_t));\n",
          ctx->prefix_lower, ctx->prefix_lower);
  fprintf(ctx->f, "  %s_astnode_t *node = (%s_astnode_t *)ret.buf;\n",
          ctx->prefix_lower, ctx->prefix_lower);
  fprintf(ctx->f, "  %s_astnode_t *children = NULL;\n", ctx->prefix_lower);
  fprintf(ctx->f, "  node->kind = kind;\n");
  fprintf(ctx->f, "  node->max_children = 0;\n");
  fprintf(ctx->f, "  node->num_children = 0;\n");
  fprintf(ctx->f, "  node->children = NULL;\n");
  fprintf(ctx->f, "  node->rew = ret.rew;\n");
  fprintf(ctx->f, "  return node;\n");
  fprintf(ctx->f, "}\n\n");

  for (size_t i = 1; i <= NODE_NUM_FIXED; i++) {

    fprintf(ctx->f, "static inline %s_astnode_t* %s_astnode_fixed_%zu(\n",
            ctx->prefix_lower, ctx->prefix_lower, i);
    fprintf(ctx->f,
            "                             pgen_allocator* alloc,\n"
            "                             const char* kind%s",
            i ? ",\n" : "");
    for (size_t j = 0; j < i; j++)
      fprintf(ctx->f, "                             %s_astnode_t* n%zu%s",
              ctx->prefix_lower, j, j != i - 1 ? ",\n" : "");
    fprintf(ctx->f, ") {\n");
    fprintf(ctx->f,
            "  pgen_allocator_ret_t ret = pgen_alloc(alloc,\n"
            "                                        sizeof(%s_astnode_t) +\n"
            "                                        sizeof(%s_astnode_t *) * "
            "%zu,\n",
            ctx->prefix_lower, ctx->prefix_lower, i);
    fprintf(ctx->f,
            "                                        "
            "_Alignof(%s_astnode_t));\n",
            ctx->prefix_lower);
    fprintf(ctx->f, "  %s_astnode_t *node = (%s_astnode_t *)ret.buf;\n",
            ctx->prefix_lower, ctx->prefix_lower);
    fprintf(ctx->f,
            "  %s_astnode_t **children = (%s_astnode_t **)(node + 1);\n",
            ctx->prefix_lower, ctx->prefix_lower);
    fprintf(ctx->f, "  node->kind = kind;\n");
    fprintf(ctx->f, "  node->max_children = 0;\n");
    fprintf(ctx->f, "  node->num_children = %zu;\n", i);
    fprintf(ctx->f, "  node->children = children;\n");
    fprintf(ctx->f, "  node->rew = ret.rew;\n");
    for (size_t j = 0; j < i; j++)
      fprintf(ctx->f, "  children[%zu] = n%zu;\n", j, j);
    fprintf(ctx->f, "  return node;\n");
    fprintf(ctx->f, "}\n\n");
  }
}

static inline void peg_write_parsermacros(codegen_ctx *ctx) {
  fprintf(ctx->f,
          "#define node(kind, ...)          "
          "PGEN_CAT(%s_astnode_fixed_, "
          "PGEN_NARG(__VA_ARGS__))"
          "(ctx->alloc, kind, __VA_ARGS__)\n",
          ctx->prefix_lower);
  fprintf(ctx->f, "#define rewind(node)             "
                  "pgen_allocator_rewind(ctx->alloc, node->rew)\n");
  fprintf(ctx->f,
          "#define list(kind)               "
          "%s_astnode_list(ctx->alloc, kind, 32)\n",
          ctx->prefix_lower);
  fprintf(ctx->f,
          "#define leaf(kind)               "
          "%s_astnode_leaf(ctx->alloc, kind)\n",
          ctx->prefix_lower);
  fprintf(ctx->f,
          "#define add(to, node)            "
          "%s_astnode_add(ctx->alloc, to, node)\n",
          ctx->prefix_lower);
  fprintf(ctx->f, "#define defer(node, freefn, ptr) "
                  "pgen_defer(ctx->alloc, freefn, ptr, node->rew)\n\n");
}

static inline void peg_write_astnode_add(codegen_ctx *ctx) {
  fprintf(ctx->f,
          "static inline void %s_astnode_add("
          "pgen_allocator* alloc, %s_astnode_t *list, %s_astnode_t *node) {\n",
          ctx->prefix_lower, ctx->prefix_lower, ctx->prefix_lower);
  fprintf(ctx->f, "  if (list->num_children > list->max_children)\n"
                  "    PGEN_OOM();\n\n");
  fprintf(ctx->f, "  if (list->max_children == list->num_children) {\n");
  fprintf(ctx->f, "    size_t new_max = list->max_children * 2;\n");
  fprintf(ctx->f, "    void* old_ptr = list->children;\n");
  fprintf(ctx->f, "    void* new_ptr = realloc(list->children, new_max);\n");
  fprintf(ctx->f, "    if (!new_ptr)\n      PGEN_OOM();\n");
  fprintf(ctx->f, "    list->children = (pl0_astnode_t **)new_ptr;\n");
  fprintf(ctx->f, "    list->max_children = new_max;\n");
  fprintf(ctx->f, "    pgen_allocator_realloced(alloc, old_ptr, new_ptr, free, "
                  "list->rew);\n");
  fprintf(ctx->f, "  }\n");
  fprintf(ctx->f, "  \n");
  fprintf(ctx->f, "  list->children[list->num_children++] = node;\n");
  fprintf(ctx->f, "}\n\n");
}

static inline void codegen_write_parser(codegen_ctx *ctx) {
  peg_write_header(ctx);
  peg_write_parser_ctx(ctx);
  peg_write_astnode_def(ctx);
  peg_write_astnode_init(ctx);
  peg_write_astnode_add(ctx);
  peg_write_parsermacros(ctx);
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
  if (ctx->pegast) {
    peg_write_directives(ctx);
    write_arena_lib(ctx);
    write_helpermacros(ctx);
  }

  // Write bodies
  if (ctx->tokast)
    codegen_write_tokenizer(ctx);
  if (ctx->pegast)
    codegen_write_parser(ctx);
}

#endif /* TOKCODEGEN_INCLUDE */
