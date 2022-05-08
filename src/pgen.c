
#define MEMDEBUG 0
#if MEMDEBUG // Memory debugging.
#define PRINT_MEMALLOCS 0
#include <apaz-libc/memdebug.h>
#endif

#include "argparse.h"
#include "tokparser.h"
#include "pegparser.h"
#include "codegen.h"

int main(int argc, char **argv) {


  ASTNode* tokast = NULL, *pegast = NULL;

  Codepoint_String_View tokenFile, parserFile;
  tokenFile.str = parserFile.str = NULL;
  tokenFile.len = parserFile.len = 0;

  // Parse command line arguments
  Args args = argparse(argc, argv);

  // Read the tokenizer file
  tokenFile = readFileCodepoints(args.tokenizerTarget);
  if (!tokenFile.str)
    ERROR("Could not read the token file.");

  // Parse the tokenizer's AST
  parser_ctx tpctx;
  parser_ctx_init(&tpctx, tokenFile);
  tokast = tok_parse_TokenFile(&tpctx);
  if (!tokast) {
    ERROR("Tokenizer file syntax error.");
  }

  // Read the parser's file
  if (args.grammarTarget) {
    parserFile = readFileCodepoints(args.grammarTarget);
    if (!parserFile.str)
      ERROR("Could not read the parser file.");

    // Parse the parser's AST
    parser_ctx ppctx;
    parser_ctx_init(&ppctx, parserFile);
    pegast = peg_parse_GrammarFile(&ppctx);
    if (!pegast) {
      ERROR("Parser file syntax error.");
    }
  }

  // Write the file
  codegen_ctx cctx;
  codegen_ctx_init(&cctx, args, tokast, pegast);
  codegen_write(&cctx);
  codegen_ctx_destroy(&cctx);

  // Clean up memory
  free(tokenFile.str);
  free(parserFile.str);

#if MEMDEBUG
  print_heap();
#endif

  return 0;
}
