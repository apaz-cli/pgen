#include "argparse.h"
#include "parserctx.h"
#include "tokparser.h"
#include "pegparser.h"
#include "astvalid.h"
#include "automata.h"
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

  // Validate the ASTs.
  validateTokast(tokast);
  validatePegast(pegast);

  // Create the automata (Tokenizer IR).
  TrieAutomaton trie = createTrieAutomaton(tokast);
  list_SMAutomaton smauts = createSMAutomata(tokast);

  // Codegen
  codegen_ctx cctx;
  codegen_ctx_init(&cctx, args, tokast, pegast, trie, smauts);
  codegen_write(&cctx);
  codegen_ctx_destroy(&cctx);

  // Clean up memory
  destroyTrieAutomaton(trie);
  destroySMAutomata(smauts);
  free(tokenFile.str);
  free(parserFile.str);

  return 0;
}
