#include "argparse.h"
#include "astvalid.h"
#include "automata.h"
#include "codegen.h"
#include "parserctx.h"
#include "pegoptim.h"
#include "pegparser.h"
#include "symtab.h"

int main(int argc, char **argv) {

  // Parse command line arguments
  Args args = argparse(argc, argv);

  // Read the parser's file
  Codepoint_String_View parserFile = readFileCodepoints(args.grammarTarget);
  if (!parserFile.str)
    ERROR("Could not read the parser file.");

  // Parse the grammar's AST
  parser_ctx ppctx;
  parser_ctx_init(&ppctx, parserFile);
  ctx_debug = args.g;
  ASTNode *ast = peg_parse_GrammarFile(&ppctx);
  if (!ast) {
    ERROR("Parser file syntax error.");
  }

  // Generate symbol tables from the AST.
  Symtabs symtabs = gen_symtabs(ast);

  // Validate the symbol tables.
  validateSymtabs(args, symtabs);

  // Create the automata (Tokenizer IR).
  TrieAutomaton trie = createTrieAutomaton(symtabs.tokendefs);
  list_SMAutomaton smauts = createSMAutomata(symtabs.tokendefs);

  // Codegen
  codegen_ctx cctx;
  codegen_ctx_init(&cctx, &args, ast, symtabs, trie, smauts);
  codegen_write(&cctx);
  codegen_ctx_destroy(&cctx);

  // Clean up memory
  destroyTrieAutomaton(trie);
  destroySMAutomata(smauts);
  free(parserFile.str);

  return 0;
}
