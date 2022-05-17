#ifndef TOKCODEGEN_INCLUDE
#define TOKCODEGEN_INCLUDE
#include "argparse.h"
#include "parserctx.h"

/*******/
/* ctx */
/*******/

#define PGEN_PREFIX_LEN 8
typedef struct {
  FILE *f;
  ASTNode *tokast;
  ASTNode *pegast;
  char prefix_lower[PGEN_PREFIX_LEN];
  char prefix_upper[PGEN_PREFIX_LEN];
} codegen_ctx;

static inline void codegen_ctx_init(codegen_ctx *ctx, Args args,
                                    ASTNode *tokast, ASTNode *pegast) {

  ctx->tokast = tokast;
  ctx->pegast = pegast;

  // Parse prefix from tokenizer file name.
  char* pref_start = args.tokenizerTarget;
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
      if ((low < '0' | low > '9'))
        break;

    ctx->prefix_lower[i] = low;
    ctx->prefix_upper[i] = up;
  }
  ctx->prefix_lower[i] = '\0';
  ctx->prefix_upper[i] = '\0';

  // Create/open the file prefix.h if -o was not an argument,
  // otherwise the -o target.
  char namebuf[PGEN_PREFIX_LEN + 2]; // Doesn't overflow.
  char* write_to;
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
  fprintf(ctx->f,
          "#ifndef %s_TOKENIZER_INCLUDE\n"
          "#define %s_TOKENIZER_INCLUDE\n"
          "#include \"utf8.h\"\n\n",
          ctx->prefix_upper, ctx->prefix_upper);
}

static inline void tok_write_toklist(codegen_ctx *ctx) {
  size_t num_defs = ctx->tokast->num_children;
  fprintf(ctx->f, "#define %s_TOKENS %s_END ", ctx->prefix_upper);
  for (size_t i = 0; i < num_defs; i++) {
    fprintf(ctx->f, "%s, ", (char *)(ctx->tokast->children[i]->children[0]->extra));
  }
  fprintf(ctx->f, "\n\n");
}

static inline void tok_write_enum(codegen_ctx *ctx) {
  fprintf(ctx->f, "typedef enum { %s_TOKENS } %s_token_id;\n",
          ctx->prefix_upper, ctx->prefix_lower);

  fprintf(ctx->f, "static %s_token_id _%s_num_tokids[] = { %s_TOKENS };\n",
          ctx->prefix_lower, ctx->prefix_lower, ctx->prefix_upper);

  fprintf(ctx->f,
          "static size_t %s_num_tokids = sizeof(_%s_num_tokids)"
          " / sizeof(%s_token_id);\n\n",
          ctx->prefix_lower, ctx->prefix_lower, ctx->prefix_lower);
}

static inline void tok_write_tokenstruct(codegen_ctx *ctx) {
  fprintf(ctx->f,
          "typedef struct {\n"
          "  %s_token_id lexeme;\n"
          "  codepoint_t* start;\n"
          "  size_t len;\n"
          "  size_t line;\n"
          "  size_t col;"
          "  char* sourceFile;\n"
          "} %s_token;\n\n",
          ctx->prefix_lower, ctx->prefix_lower);
}

static inline void tok_write_tokenizerstruct(codegen_ctx* ctx) {
  fprintf(ctx->f,
          "typedef struct {\n"
          "} %s_tokenizer;\n\n",
          ctx->prefix_lower);
}

static inline void tok_write_footer(codegen_ctx *ctx) {
  fprintf(ctx->f, "#endif /* %s_TOKENIZER_INCLUDE */\n", ctx->prefix_upper);
}

static inline void codegen_write_tokenizer(codegen_ctx *ctx) {
  tok_write_header(ctx);

  tok_write_toklist(ctx);

  tok_write_enum(ctx);

  tok_write_tokenstruct(ctx);

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
