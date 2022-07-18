#ifndef PGEN_ARGPARSE
#define PGEN_ARGPARSE

#include "util.h"

typedef struct {
  char *tokenizerTarget; // May not be null
  char *grammarTarget;   // May be null
  char *outputTarget;    // May be null
  bool h;                // Help
  bool d;                // Debug prompt
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
  args.g = 0;
  args.m = 0;
  args.u = 0;

  for (int i = 1; i < argc; i++) {
    char *a = argv[i];
    // Flags
    if (!strcmp(a, "-h") || !strcmp(a, "--help")) {
      args.h = 1;
    } else if (!strcmp(a, "-d") || !strcmp(a, "--debug")) {
      args.d = 1;
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

#if 0
  printf("Args:\n");
  printf("tokenizerTarget: %s\n", args.tokenizerTarget);
  printf("grammarTarget: %s\n", args.grammarTarget);
  printf("outputTarget: %s\n", args.outputTarget);
  
  printf("h: %i\n", (int)args.h);
  printf("d: %i\n", (int)args.d);
  printf("m: %i\n", (int)args.m);
  printf("u: %i\n", (int)args.u);
  fflush(stdout);
#endif

  // Help message
  if (args.h) {
    puts("pgen - A tokenizer and parser generator.\n"
         "    pgen [-h] [-g] INPUT_TOK [INPUT_PEG] [-o OUTPUT_PATH]\n");
    exit(0);
  }

  if (!args.tokenizerTarget) {
    puts("Please provide a tokenizer file as an argument.");
    exit(1);
  }

  return args;
}

#endif /* PGEN_ARGPARSE */
