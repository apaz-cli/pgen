#ifndef TOKCODEGEN_INCLUDE
#define TOKCODEGEN_INCLUDE
#include "argparse.h"
#include "ast.h"
#include "automata.h"
#include "list.h"
#include "parserctx.h"
#include "utf8.h"

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

/****************/
/* UTF8 Library */
/****************/

static inline void write_utf8_lib(codegen_ctx *ctx) {
#include "strutf8.xxd"
  fprintf(ctx->f, "%s", (char *)src_utf8_h);
  fprintf(ctx->f, "\n\n");
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
          "#endif\n\n",
          ctx->prefix_upper, ctx->prefix_upper, ctx->prefix_upper,
          ctx->prefix_upper);
}

static inline void tok_write_enum(codegen_ctx *ctx) {
  fprintf(ctx->f, "typedef enum { ");

  size_t num_defs = ctx->tokast->num_children;
  fprintf(ctx->f, "%s_TOK_STREAMEND, ", ctx->prefix_upper);
  for (size_t i = 0; i < num_defs; i++)
    fprintf(ctx->f, "%s_TOK_%s, ", ctx->prefix_upper,
            (char *)(ctx->tokast->children[i]->children[0]->extra));

  fprintf(ctx->f, "} %s_token_id;\n\n", ctx->prefix_lower);

  fprintf(ctx->f,
          "// The 0th token is end of stream.\n// Tokens 1 - %zu are the ones "
          "you defined.\n",
          num_defs);
  fprintf(ctx->f, "static size_t %s_num_tokens = %zu;\n", ctx->prefix_lower,
          num_defs + 1);
  fprintf(ctx->f,
          "static const char* %s_lexeme_name[] = { \"%s_TOK_STREAMEND\", ",
          ctx->prefix_lower, ctx->prefix_upper);
  for (size_t i = 0; i < num_defs; i++)
    fprintf(ctx->f, "\"%s_TOK_%s\", ", ctx->prefix_upper,
            (char *)(ctx->tokast->children[i]->children[0]->extra));
  fprintf(ctx->f, "};\n\n");
}

static inline void tok_write_tokenstruct(codegen_ctx *ctx) {
  fprintf(ctx->f,
          "typedef struct {\n"
          "  %s_token_id lexeme;\n"
          "  size_t start;\n"
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

static inline void tok_write_charsetcheck(codegen_ctx *ctx, ASTNode *charset) {

  // Single char
  if (!charset->num_children) {
    codepoint_t c = *(codepoint_t *)charset->extra;
    fprintf(ctx->f, "c == %" PRI_CODEPOINT "", c);
  }

  // Charset
  else {
    bool inverted = *(bool *)charset->extra;
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
  fprintf(ctx->f, "\n\n");

  // Outer loop
  fprintf(ctx->f, "  for (size_t iidx = 0; iidx < remaining; iidx++) {\n");
  fprintf(ctx->f, "    codepoint_t c = current[iidx];\n\n");
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
    fprintf(ctx->f, "      }\n");
    fprintf(ctx->f, "    }\n\n"); // End of Trie aut
  }

  // SM auts
  if (has_smauts) {
    for (size_t a = 0; a < smauts.len; a++) {
      SMAutomaton smaut = smauts.buf[a];
      fprintf(ctx->f, "    // Transition State Machine %zu\n", a);
      fprintf(ctx->f, "    if (smaut_state_%zu != -1) {\n", a);
      fprintf(ctx->f, "      all_dead = 0;\n");
      int eels = 0;
      for (size_t i = 0; i < smaut.trans.len; i++) {
        int els = 0;
        SMTransition trans = smaut.trans.buf[i];
        fprintf(ctx->f, "      %sif (", eels++ ? "else " : "");
        tok_write_charsetcheck(ctx, trans.act);
        fprintf(ctx->f, ") {\n");
        for (size_t j = 0; j < trans.from.len; j++) {
          fprintf(ctx->f,
                  "        %sif (smaut_state_%zu == %i) "
                  "smaut_state_%zu = %i;\n",
                  els++ ? "else " : "", a, trans.from.buf[j], a, trans.to);
        }
        fprintf(ctx->f, "        else smaut_state_%zu = -1;\n", a);
        fprintf(ctx->f, "      }\n");
      }
      fprintf(ctx->f, "      else {\n");
      fprintf(ctx->f, "        smaut_state_%zu = -1;\n", a);
      fprintf(ctx->f, "      }\n\n");
      // Check SM Accepting
      fprintf(ctx->f, "      int accept = %s",
              smaut.accepting.len > 1 ? "(" : "");
      for (size_t i = 0; i < smaut.accepting.len; i++) {
        int acc = list_int_get(&smaut.accepting, i);
        fprintf(ctx->f, "(smaut_state_%zu == %i)", a, acc);
        if (i != smaut.accepting.len - 1)
          fprintf(ctx->f, " | ");
      }
      fprintf(ctx->f, "%s;\n", smaut.accepting.len > 1 ? ")" : "");
      fprintf(ctx->f, "      if (accept)\n");
      fprintf(ctx->f, "        smaut_munch_size_%zu = iidx + 1;\n", a);

      fprintf(ctx->f, "    }\n\n");
    }
  }
  fprintf(ctx->f, "    if (all_dead)\n");
  fprintf(ctx->f, "      break;\n");
  fprintf(ctx->f, "  }\n\n"); // For each remaining character

  fprintf(ctx->f, "  // Determine what token was accepted, if any.\n");
  fprintf(ctx->f,
          "  %s_token ret;\n"
          "#if %s_TOKENIZER_SOURCEINFO\n"
          "  ret.line = tokenizer->pos_line;\n"
          "  ret.col = tokenizer->pos_col;\n"
          "  ret.sourceFile = tokenizer->pos_sourceFile;\n"
          "#endif\n\n",
          ctx->prefix_lower, ctx->prefix_upper);

  for (size_t a = 0; a < smauts.len; a++) {
  }
  fprintf(ctx->f, "  \n");

  fprintf(ctx->f, "  return ret;\n");
  fprintf(ctx->f, "}\n\n"); // end function
}

static inline void tok_write_footer(codegen_ctx *ctx) {
  fprintf(ctx->f, "#endif /* %s_TOKENIZER_INCLUDE */\n", ctx->prefix_upper);
}

static inline void codegen_write_tokenizer(codegen_ctx *ctx) {

  write_utf8_lib(ctx);

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
