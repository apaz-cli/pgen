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

  ASTNode* tokast = NULL, *pegast = NULL;

  Codepoint_String_View tokenFile, parserFile;
  tokenFile.str = parserFile.str = NULL;
  tokenFile.len = parserFile.len = 0;

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
  codegen_ctx_init(&cctx, args);

  // Clean up memory
  if (tokast)
    ASTNode_destroy(tokast);
  if (pegast)
    ASTNode_destroy(pegast);
  free(tokenFile.str);
  free(parserFile.str);

  return 0;
}
