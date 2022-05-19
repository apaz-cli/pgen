#ifndef TOKCODEGEN_INCLUDE
#define TOKCODEGEN_INCLUDE
#include "argparse.h"
#include "automata.h"
#include "parserctx.h"

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
      if ((low<'0' | low> '9'))
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

/*************/
/* Tokenizer */
/*************/

static inline void tok_write_header(codegen_ctx *ctx) {
  // TODO move utf8 include path.
  fprintf(ctx->f,
          "#ifndef %s_TOKENIZER_INCLUDE\n"
          "#define %s_TOKENIZER_INCLUDE\n"
          "#include \"src/utf8.h\"\n\n"
          "#ifndef %s_TOKENIZER_SOURCEINFO\n"
          "#define %s_TOKENIZER_SOURCEINFO 1\n"
          "#endif\n\n",
          ctx->prefix_upper, ctx->prefix_upper, ctx->prefix_upper,
          ctx->prefix_upper);
}

static inline void tok_write_toklist(codegen_ctx *ctx) {
  size_t num_defs = ctx->tokast->num_children;
  fprintf(ctx->f, "#define %s_TOKENS %s_TOK_STREAMEND, ", ctx->prefix_upper,
          ctx->prefix_upper);
  for (size_t i = 0; i < num_defs; i++) {
    fprintf(ctx->f, "%s_TOK_%s, ", ctx->prefix_upper,
            (char *)(ctx->tokast->children[i]->children[0]->extra));
  }
  fprintf(ctx->f, "\n\n");
}

static inline void tok_write_enum(codegen_ctx *ctx) {
  fprintf(ctx->f, "typedef enum { %s_TOKENS } %s_token_id;\n",
          ctx->prefix_upper, ctx->prefix_lower);

  fprintf(ctx->f, "static %s_token_id _%s_num_tokids[] = { %s_TOKENS };\n",
          ctx->prefix_lower, ctx->prefix_lower, ctx->prefix_upper);

  fprintf(ctx->f,
          "static size_t %s_num_tokens = (sizeof(_%s_num_tokids)"
          " / sizeof(%s_token_id)) - 1;\n\n",
          ctx->prefix_lower, ctx->prefix_lower, ctx->prefix_lower);
}

static inline void tok_write_tokenstruct(codegen_ctx *ctx) {
  fprintf(ctx->f,
          "typedef struct {\n"
          "  %s_token_id lexeme;\n"
          "  codepoint_t* start;\n"
          "  size_t len;\n"
          "#if %s_TOKENIZER_SOURCEINFO\n"
          "  size_t line;\n"
          "  size_t col;\n"
          "  char* sourceFile;\n"
          "#endif\n"
          "} %s_token;\n\n",
          ctx->prefix_lower, ctx->prefix_upper, ctx->prefix_lower);
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
          "static inline void %s_tokenizer_init(%s_tokenizer* tokenizer, char* "
          "sourceFile, codepoint_t* start, size_t len) {\n"
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

static inline void tok_write_smauts(codegen_ctx *ctx) {
  list_SMAutomaton smauts = ctx->smauts;
  for (size_t i = 0; i < smauts.len; i++) {
    SMAutomaton smaut = smauts.buf[i];
  }
}

static inline void tok_write_trie(codegen_ctx *ctx) {}

static inline void tok_write_nexttoken(codegen_ctx *ctx) {
  // See tokenizer.txt.

  TrieAutomaton trie = ctx->trie;
  list_SMAutomaton smauts = ctx->smauts;
  int has_trie = trie.accepting.len ? 1 : 0;
  int has_smauts = smauts.len ? 1 : 0;

  fprintf(ctx->f,
          "static inline %s_token %s_nextToken(%s_tokenizer* tokenizer) {\n"
          "  codepoint_t* current = tokenizer->start + tokenizer->pos;\n"
          "  size_t remaining = tokenizer->len - tokenizer->pos;\n"
          "  %s_token ret;\n"
          "#if %s_TOKENIZER_SOURCEINFO\n"
          "  ret.line = tokenizer->pos_line;\n"
          "  ret.col = tokenizer->pos_col;\n"
          "  ret.sourceFile = tokenizer->pos_sourceFile;\n"
          "#endif\n\n",
          ctx->prefix_lower, ctx->prefix_lower, ctx->prefix_lower,
          ctx->prefix_lower, ctx->prefix_upper);

  // Variables for each automaton for the current run.
  size_t num_auts = smauts.len + has_trie;
  fprintf(ctx->f, "  size_t last_accept[%zu] = { 0", num_auts);
  for (size_t i = 1; i < num_auts; i++)
    fprintf(ctx->f, ", 0");
  fprintf(ctx->f, " };\n");
  fprintf(ctx->f, "  int current_states[%zu] = { 0", num_auts);
  for (size_t i = 1; i < num_auts; i++)
    fprintf(ctx->f, ", 0");
  fprintf(ctx->f, " };\n\n");

  // Outer loop
  fprintf(ctx->f, "  for (size_t iidx = 0; iidx < remaining; iidx++) {\n");
  fprintf(ctx->f, "    int all_dead = 1;\n");

  // Inner loop (automaton, unrolled)
  // Trie aut
  if (trie.accepting.len) {
    fprintf(ctx->f, "    // Trie\n");
    fprintf(ctx->f, "    if (current_states[0] != -1) {\n");
    fprintf(ctx->f, "      all_dead = 0;\n");
    fprintf(ctx->f, "      \n");
    fprintf(ctx->f, "    }\n\n");
  }

  // SM auts
  fprintf(ctx->f, "    // State Machines\n");
  for (size_t a = 0; a < smauts.len; a++) {
    SMAutomaton smaut = smauts.buf[a];
    size_t aut_num = a + has_trie;
    fprintf(ctx->f, "    if (current_states[%zu] != -1) {\n", aut_num);
    fprintf(ctx->f, "      all_dead = 0;\n");
    fprintf(ctx->f, "      \n");
    fprintf(ctx->f, "    }\n\n");
  }
  fprintf(ctx->f, "    if (all_dead)\n      break;\n");

  fprintf(ctx->f, "  }\n\n"); // For each remaining character
  fprintf(ctx->f, "  return ret;\n");
  fprintf(ctx->f, "}\n\n"); // end function
}

static inline void tok_write_footer(codegen_ctx *ctx) {
  fprintf(ctx->f, "#endif /* %s_TOKENIZER_INCLUDE */\n", ctx->prefix_upper);
}

static inline void codegen_write_tokenizer(codegen_ctx *ctx) {
  tok_write_header(ctx);

  tok_write_toklist(ctx);

  tok_write_enum(ctx);

  tok_write_tokenstruct(ctx);

  tok_write_ctxstruct(ctx);

  tok_write_nexttoken(ctx);

  tok_write_footer(ctx);
}

/**********/
/* Parser */
/**********/

static inline void codegen_write_parser(codegen_ctx *ctx) {}

/**************/
/* Everything */
/**************/

static inline void codegen_write(codegen_ctx *ctx) {
  if (ctx->tokast)
    codegen_write_tokenizer(ctx);
  if (ctx->pegast)
    codegen_write_parser(ctx);
}

#endif /* TOKCODEGEN_INCLUDE */
