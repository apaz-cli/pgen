#if 0 // Memory debugging.
#define MEMDEBUG 1
#define PRINT_MEMALLOCS 1
#include <apaz-libc/memdebug.h>
#endif

#include "util.h"
#include "tokparser.h"

struct {
  char* tokenizerTarget;
  char* grammarTarget;
  char* outputTarget;
  bool h;
  bool g;
} args;

void argparse(int argc, char** argv) {
  args.tokenizerTarget = NULL;
  args.grammarTarget = NULL;
  args.outputTarget = NULL;
  args.h = 0;
  args.g = 0;

  for (int i = 1; i < argc; i++) {
    char* a = argv[i];
    if (strcmp(a, "-h") == 0 || strcmp(a, "--help") == 0) {
      args.h = 1;
    } else if (strcmp(a, "-g") == 0) {
      args.g = 1;
    } else if (strcmp(a, "-o") == 0) {
      if (i != argc - 1) {
        args.outputTarget = argv[++i];
      } else {
        ERROR("-o requires an argument.");
      }
    } else {
      const char errmsg[] = " file does not end in ";
      const char errend[] = ". Consider renaming it. Proceeding anyway.\n";
      if (!args.tokenizerTarget) {
        if (strlen(a) < 4 || strcmp(a + strlen(a) - 4, ".tok") != 0)
          fprintf(stderr, "%s%s%s%s", "Tokenizer", errmsg, ".tok", errend);
        args.tokenizerTarget = a;
      } else if (!args.grammarTarget) {
        if (strlen(a) < 4 || strcmp(a + strlen(a) - 4, ".peg") != 0)
          fprintf(stderr, "%s%s%s%s",  "Grammar", errmsg, ".peg", errend);
        args.grammarTarget = a;
      } else {
        ERROR("Too many targets specified.");
      }
    }
  }

  if (args.h || !args.tokenizerTarget) {
    puts("pgen - A tokenizer and parser generator.\n"
         "    pgen [-h] [-g] INPUT_TOK [INPUT_PEG] [-o OUTPUT_PATH]\n");
    exit(0);
  }
  if (!args.tokenizerTarget)
   ERROR("No target files.");
}


int main(int argc, char **argv) {

  argparse(argc, argv);

  // Read the file
  Codepoint_String_View tokenFile = readFileCodepoints(args.tokenizerTarget);
  if (!tokenFile.str)
    ERROR("Could not read the token file.");

  // Parse the AST
  tokparser_ctx tpctx;
  tokparser_ctx_init(&tpctx, tokenFile);
  ASTNode* ast = tok_parse_TokenFile(&tpctx);
  if (!ast) {
    ERROR("Failed to parset the AST.");
  }

  AST_print(ast);

  return 0;
}
