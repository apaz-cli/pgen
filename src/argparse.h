#ifndef PGEN_ARGPARSE
#define PGEN_ARGPARSE

#include "util.h"

typedef struct {
  char *tokenizerTarget; // May not be null
  char *grammarTarget;   // May be null
  char *outputTarget;    // May be null
  bool h;                // Help
  bool d;                // Debug prompt
  bool t;                // Tokenizer debug prompt
  bool g;                // Grammar debug prompt
  bool m;                // Memory allocator debugging
  bool u;                // Generate Unsafe (but fast) code.
} Args;

static inline Args argparse(int argc, char **argv) {
  Args args;
  args.tokenizerTarget = NULL;
  args.grammarTarget = NULL;
  args.outputTarget = NULL;
  args.h = 0;
  args.d = 0;
  args.t = 0;
  args.g = 0;
  args.m = 0;
  args.u = 0;

  char exitmsg[] =
      "pgen - A tokenizer and parser generator.\n"
      "    pgen [OPTION]... INPUT_TOK [INPUT_PEG] [-o OUTPUT_PATH]\n"
      "  where:\n"
      "    -h, --help               Display this help message and exit.  \n"
      "    -d, --debug              Display this help message and exit.  \n"
      "    -t, --tokenizer-debug    Generate an interactive tokenizer.   \n"
      "    -g, --grammar-debug      Generate an interactive parser.      \n"
      "    -m, --memdebug           Debug the generated memory allocator.\n"
      "    -u, --unsafe             Don't check for errors. Much faster. \n";

  for (int i = 1; i < argc; i++) {
    char *a = argv[i];
    // Flags
    if (!strcmp(a, "-h") || !strcmp(a, "--help")) {
      args.h = 1;
    } else if (!strcmp(a, "-d") || !strcmp(a, "--debug")) {
      args.d = 1;
    } else if (!strcmp(a, "-t") || !strcmp(a, "--tokenizer-debug")) {
      args.t = 1;
    } else if (!strcmp(a, "-g") || !strcmp(a, "--grammar-debug")) {
      args.g = 1;
    } else if (!strcmp(a, "-m") || !strcmp(a, "--memdebug")) {
      args.m = 1;
    } else if (!strcmp(a, "-u") || !strcmp(a, "--unsafe")) {
      args.u = 1;
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
  bool h; // Help
  bool d; // Debug prompt
  bool t; // Tokenizer debug prompt
  bool g; // Grammar debug prompt
  bool m; // Memory allocator debugging
  bool u; // Generate Unsafe (but fast) code.
  if (!args.tokenizerTarget) {
    puts("Please provide a tokenizer file as an argument.");
    exit(1);
  }

  return args;
}

#endif /* PGEN_ARGPARSE */
