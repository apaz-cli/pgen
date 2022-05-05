#if 0 // Memory debugging.
#define MEMDEBUG 1
#define PRINT_MEMALLOCS 1
#include <apaz-libc/memdebug.h>
#endif

#include "argparse.h"
#include "tokparser.h"
#include "pegparser.h"
#include "codegen.h"

int main(int argc, char **argv) {

  Args args = argparse(argc, argv);

  // Read the file
  Codepoint_String_View tokenFile = readFileCodepoints(args.tokenizerTarget);
  if (!tokenFile.str)
    ERROR("Could not read the token file.");

  // Parse the tokenizer's AST
  parser_ctx tpctx;
  tokparser_ctx_init(&tpctx, tokenFile);
  ASTNode *ast = tok_parse_TokenFile(&tpctx);
  if (!ast) {
    ERROR("Syntax error. No valid tokens.");
  }
  // AST_print(ast);

  // Parse the parser's AST

  // Write the file

  return 0;
}
