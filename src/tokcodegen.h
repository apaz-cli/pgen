#ifndef TOKCODEGEN_INCLUDE
#define TOKCODEGEN_INCLUDE
#include "tokparser.h"

typedef struct {
  ASTNode *ast;
  FILE *f;
  char *prefix_lower;
  char *prefix_upper;
} tok_writectx;

static inline void tok_writectx_init(tok_writectx* ctx, Args args) {
  char* prefix;
  char* out = args.outputTarget;
  if (!out) {
    size_t l = strlen(args.tokenizerTarget);
    out = (char*)malloc(l + 4);

    size_t i = 0;
    while (1) {
      // File name is expected in [_a-zA-Z].
      // Any other characters will cause parsing the prefix to exit.
      char c = args.tokenizerTarget[i];

      // [A-Z] to lowercase
      if ((c >= 'A') & (c <= 'Z'))
        c -= ('A' - 'a');

      // Copy up to the first invalid character
      int exit = 0;
      if (c != "_")
        if ((c < 'a') | (c > 'z'))
          exit = 1;

      if (exit | (i == l)) {
        out[i] = '\0';
        break;
      }

      out[i] = args.tokenizerTarget[i];
      i++;
    }
  }
  ctx->f = fopen(out, 'w')
}

static inline void tok_write_header(tok_writectx *ctx) {
  fprintf(ctx->f,
          "#ifndef %s_TOKENIZER_INCLUDE\n"
          "#define %s_TOKENIZER_INCLUDE\n"
          "#include \"utf8.h\"\n\n",
          ctx->prefix_upper, ctx->prefix_upper);
  fputs_unlocked("", ctx->f);
}

static inline void tok_write_toklist(tok_writectx *ctx) {
  size_t num_defs = ctx->ast->num_children; // >= 1
  fprintf(ctx->f, "#define %s_TOKENS ", ctx->prefix_upper);
  for (size_t i = 0; i < num_defs; i++) {
    fprintf(ctx->f, "%s, ", (char *)(ctx->ast->children[i]->extra));
  }
}

static inline void tok_write_footer(tok_writectx *ctx) {
  fprintf(ctx->f, "#endif /* %s_TOKENIZER_INCLUDE */\n", ctx->prefix_upper);
}

static inline void tok_write_tokenizerFile(tok_writectx *ctx) {
  tok_write_header(ctx);
  tok_write_toklist(ctx);
  tok_write_footer(ctx);
}

#endif /* TOKCODEGEN_INCLUDE */
