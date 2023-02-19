#include "calc.h"

#include <errno.h>
#include <stdio.h>

#define PRINT_AST 1
#define PRINT_ANSWER 1

#define PANIC(str) fprintf(stderr, "%s\n", str), exit(1)
#define ARITH(op, a, b, reason)                                                \
  do {                                                                         \
    fprintf(stderr,                                                            \
            "Error: "                                                          \
            "%" PRId64 " %s %" PRId64 " (%s)\n\n",                             \
            a, op, b, reason),                                                 \
        *exception = 1;                                                        \
    return 0;                                                                  \
  } while (0)

int64_t getAnswer(calc_astnode_t *n, int *exception) {

  if (n->kind == CALC_NODE_PLUS) {
    int64_t total = getAnswer(n->children[0], exception);
    if (*exception)
      return 0;
    for (size_t i = 1; i < n->num_children; i++) {
      int64_t toadd = getAnswer(n->children[i], exception);
      if (*exception)
        return 0;
      if ((total > 0) && (toadd > INT_MAX - total))
        ARITH("+", total, toadd, "Overflow");
      if ((total < 0) && (toadd < INT_MIN - total))
        ARITH("+", total, toadd, "Underflow");
      total = total + toadd;
    }
    return total;
  }

  else if (n->kind == CALC_NODE_MINUS) {
    int64_t total = getAnswer(n->children[0], exception);
    if (*exception)
      return 0;
    for (size_t i = 1; i < n->num_children; i++) {
      int64_t tosub = getAnswer(n->children[i], exception);
      if (*exception)
        return 0;
      if ((tosub < 0) && (total > INT_MAX + tosub))
        ARITH("-", total, tosub, "Overflow");
      if ((tosub > 0) && (total < INT_MIN + tosub))
        ARITH("-", total, tosub, "Underflow");
      total = total - tosub;
    }
    return total;
  }

  else if (n->kind == CALC_NODE_MULT) {
    int64_t total = getAnswer(n->children[0], exception);
    if (*exception)
      return 0;
    for (size_t i = 1; i < n->num_children; i++) {
      int64_t tomult = getAnswer(n->children[i], exception);
      if (*exception)
        return 0;
      if ((tomult != 0) && (total > INT_MAX / tomult))
        ARITH("*", total, tomult, "Overflow");
      if ((tomult != 0) && (total < INT_MIN / tomult))
        ARITH("*", total, tomult, "Underflow");
      total = total * tomult;
    }
    return total;
  }

  else if (n->kind == CALC_NODE_DIV) {
    int64_t total = getAnswer(n->children[0], exception);
    if (*exception)
      return 0;
    for (size_t i = 1; i < n->num_children; i++) {
      int64_t todiv = getAnswer(n->children[i], exception);
      if (*exception)
        return 0;
      if (!todiv)
        ARITH("/", total, todiv, "Division by zero");
      total = total / todiv;
    }
    return total;
  }

  else if (n->kind == CALC_NODE_NUMBER) {
    char *utfstr = NULL;
    size_t utflen = 0;
    int64_t ret = 0;
    UTF8_encode(n->tok_repr, n->len_or_toknum, &utfstr, &utflen);
    errno = 0;
    int s = sscanf(utfstr, "%" PRId64, &ret) - 1;
    if (errno) {
      perror("sscanf");
      *exception = 1;
      ret = 1;
    }
    free(utfstr);
    return ret;
  }

  else {
    fprintf(stderr, "Unexpected node type: %s", calc_nodekind_name[n->kind]);
    PANIC("");
    return 0;
  }
}

void runCalculator(void) {
  char *input_str = NULL;
  size_t input_len = 0;
  char **lineptr = &input_str;
  while (1) {
    input_len = (size_t)getline(lineptr, &input_len, stdin);
    if (input_len < 2)
      continue;
    (*lineptr)[input_len - 1] = '\0'; // Remove trailine newline
    break;
  }

  codepoint_t *cps = NULL;
  size_t cpslen = 0;
  if (!UTF8_decode(input_str, input_len, &cps, &cpslen))
    fprintf(stderr, "Could not decode to UTF32.\n"), exit(1);

  calc_tokenizer tokenizer;
  calc_tokenizer_init(&tokenizer, cps, cpslen);

  // Define token list
  struct {
    calc_token *buf;
    size_t size;
    size_t cap;
  } toklist = {(calc_token *)malloc(sizeof(calc_token) * 4096), 0, 4096};
  if (!toklist.buf)
    fprintf(stderr, "Out of memory allocating token list.\n"), exit(1);

  // Parse Tokens
  calc_token tok;
  do {
    tok = calc_nextToken(&tokenizer);
    if (tok.kind != CALC_TOK_WS) {
      if (toklist.size == toklist.cap &&
          !(toklist.buf = realloc(toklist.buf, toklist.cap *= 2))) {
        fprintf(stderr, "Out of memory reallocating token list.\n"), exit(1);
      }
      toklist.buf[toklist.size++] = tok;
    }
  } while (tok.kind != CALC_TOK_STREAMEND);

  // Init Parser
  pgen_allocator allocator = pgen_allocator_new();
  calc_parser_ctx parser;
  calc_parser_ctx_init(&parser, &allocator, toklist.buf, toklist.size);

  // Parse AST
  calc_astnode_t *ast = calc_parse_expr(&parser);

  // Check for errors
  if (parser.num_errors) {
    for (size_t i = 0; i < parser.num_errors; i++) {
      calc_parse_err error = parser.errlist[i];
      fprintf(stderr,
              "An error was encountered on line %zu during parsing:\n"
              "%s\n\n",
              error.line, error.msg);
    }
    return;
  }

  // Print AST
  calc_astnode_print_json(toklist.buf, ast);

  // Print the answer
  if (ast) {
    int exception = 0;
    int64_t ans = getAnswer(ast, &exception);
    if (!exception)
      printf("Answer: %" PRId64 "\n\n", ans);

  } else if (input_len)
    printf("(parsing failed)\n\n");

  // Clean up
  pgen_allocator_destroy(&allocator);
  free(toklist.buf);
  free(cps);
  free(input_str);
}

int main(void) {
  for (;;)
    runCalculator();
}
