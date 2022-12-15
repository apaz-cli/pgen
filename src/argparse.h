#ifndef PGEN_ARGPARSE
#define PGEN_ARGPARSE

#include "util.h"

typedef struct {
  char *tokenizerTarget; // (path to .tok) May not be null
  char *grammarTarget;   // (path to .peg) May be null
  char *outputTarget;    // (path to output) May be null
  bool h:1;              // Help
  bool i:1;              // Interactive
  bool d:1;              // Debug runtime errors
  bool u:1;              // Generate Unsafe (but fast) code
  bool t:1;              // Tokenizer debug prompt
  bool g:1;              // Grammar debug prompt
  bool m:1;              // Memory allocator debugging
  bool l:1;              // Line directives
} Args;

static inline Args argparse(int argc, char **argv) {
  Args args;
  args.tokenizerTarget = NULL;
  args.grammarTarget = NULL;
  args.outputTarget = NULL;
  args.h = 0;
  args.i = 0;
  args.d = 0;
  args.u = 0;
  args.t = 0;
  args.g = 0;
  args.m = 0;
  args.l = 0;

  char exitmsg[] =
      "pgen - A tokenizer and parser generator.\n"
      "    pgen [OPTION]... INPUT_TOK [INPUT_PEG] [-o OUTPUT_PATH]           \n"
      "                                                                      \n"
      "  Options:                                                            \n"
      "    -h, --help               Display this help message and exit.      \n"
      "    -i, --interactive        Generate an interactive parser.          \n"
      "    -d, --debug              Generate checks for runtime errors.      \n"
      "    -u, --unsafe             Don't check for errors in generated code.\n"
      "    -t, --tokenizer-debug    Troubleshoot .tok syntax errors.         \n"
      "    -g, --grammar-debug      Troubleshoot .peg syntax errors.         \n"
      "    -m, --memdebug           Debug the generated memory allocator.    \n"
      "    -l, --lines              Generate #line directives.               \n"
      ;

  // Unsafe strips comments, disables checking the result of malloc, and removes
  // validation of the ast for faster compile times, as the grammar is assumed
  // to be correct.

  for (int i = 1; i < argc; i++) {
    char *a = argv[i];
    // Flags
    if (!strcmp(a, "-h") || !strcmp(a, "--help")) {
      args.h = 1;
    } else if (!strcmp(a, "-i") || !strcmp(a, "--interactive")) {
      args.i = 1;
    } else if (!strcmp(a, "-d") || !strcmp(a, "--debug")) {
      args.d = 1;
    } else if (!strcmp(a, "-u") || !strcmp(a, "--unsafe")) {
      args.u = 1;
    } else if (!strcmp(a, "-t") || !strcmp(a, "--tokenizer-debug")) {
      args.t = 1;
    } else if (!strcmp(a, "-g") || !strcmp(a, "--grammar-debug")) {
      args.g = 1;
    } else if (!strcmp(a, "-m") || !strcmp(a, "--memdebug")) {
      args.m = 1;
    } else if (!strcmp(a, "-l") || !strcmp(a, "--lines")) {
      args.l = 1;
    } else if (!strcmp(a, "-o") || !strcmp(a, "--output")) {
      if (i != argc - 1) {
        args.outputTarget = argv[++i];
      } else {
        ERROR("-o requires an argument.");
      }
    }
    // Targets
    else {
      const char errmsg[] = " file does not end in ";
      const char errend[] = ". Consider renaming it. Proceeding anyway.\n";
      if (!args.tokenizerTarget) {
        if (strlen(a) < 4 || strcmp(a + strlen(a) - 4, ".tok") != 0)
          fprintf(stderr, "%s%s%s%s", "Tokenizer", errmsg, ".tok", errend);
        args.tokenizerTarget = a;
      } else if (!args.grammarTarget) {
        if (strlen(a) < 4 || strcmp(a + strlen(a) - 4, ".peg") != 0)
          fprintf(stderr, "%s%s%s%s", "Grammar", errmsg, ".peg", errend);
        args.grammarTarget = a;
      } else {
        ERROR("Too many targets specified.");
      }
    }
  }

  // Help message
  if (args.h) {
    puts(exitmsg);
    exit(0);
  }

  if (!args.tokenizerTarget) {
    puts("Please provide a tokenizer file as an argument.");
    exit(1);
  }

  return args;
}

#endif /* PGEN_ARGPARSE */
