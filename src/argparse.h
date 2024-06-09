#ifndef PGEN_ARGPARSE
#define PGEN_ARGPARSE

#include "util.h"

typedef struct {
  char *grammarTarget; // (path to .peg) May be null
  char *outputTarget;  // (path to output) May be null
  char *pythonTarget;  // (path to module folder) May be null, may not exist.
  bool h : 1;          // Help
  bool i : 1;          // Interactive
  bool d : 1;          // Debug runtime errors
  bool u : 1;          // Generate Unsafe (but fast) code
  bool g : 1;          // Grammar debug
  bool s : 1;          // Grammar debug automatically
  bool m : 1;          // Memory allocator debugging
  bool l : 1;          // Line directives
} Args;

static inline Args argparse(int argc, char **argv) {
  Args args;
  args.grammarTarget = NULL;
  args.outputTarget = NULL;
  args.pythonTarget = NULL;
  args.h = 0;
  args.i = 0;
  args.d = 0;
  args.u = 0;
  args.g = 0;
  args.s = 0;
  args.m = 0;
  args.l = 0;

  char helpmsg[] =
      "pgen - A tokenizer and parser generator.\n"
      "    pgen [OPTION]... GRAMMAR_FILE [-o OUTPUT_PATH]                    \n"
      "                                                                      \n"
      "  Options:                                                            \n"
      "    -h, --help               Display this help message and exit.      \n"
      "    -i, --interactive        Generate an interactive parser.          \n"
      "    -d, --debug              Generate checks for runtime errors.      \n"
      "    -u, --unsafe             Don't check for errors in generated code.\n"
      "    -g, --grammar-debug      Show .peg syntax errors.                 \n"
      "    -s, --grammar-step       Step through your .peg file as it parses.\n"
      "    -m, --memdebug           Debug the generated memory allocator.    \n"
      "    -l, --lines              Generate #line directives.               \n"
      "    -p, --python             Generate a python module for your parser.\n"
      "\n";

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
    } else if (!strcmp(a, "-g") || !strcmp(a, "--grammar-debug")) {
      args.g = 1;
    } else if (!strcmp(a, "-s") || !strcmp(a, "--grammar-show")) {
      args.s = 1;
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
    } else if (!strcmp(a, "-p") || !strcmp(a, "--python")) {
      if (i != argc - 1) {
        args.pythonTarget = argv[++i];
      } else {
        ERROR("-p requires an argument, the path to the module folder.");
      }
    }
    // Unrecognized
    else if (strlen(a) && a[0] == '-') {
      ERROR("Unrecognized option \"%s\"", a);
    }
    // Targets
    else {
      if (!args.grammarTarget) {
        if (strlen(a) < 4 || strcmp(a + strlen(a) - 4, ".peg") != 0)
          fprintf(stderr,
                  "Grammar file does not end in \".peg\". Consider renaming "
                  "it. Proceeding anyway.\n");
        args.grammarTarget = a;
      } else {
        ERROR("Too many targets specified.");
      }
    }
  }

  // Help message
  if (args.h || argc == 1) {
    puts(helpmsg);
    exit(0);
  }

  if (!args.grammarTarget) {
    ERROR("Please provide a grammar file as an argument.");
  }

  if (args.outputTarget && args.pythonTarget) {
    ERROR("Cannot specify both -o and -p.");
  }

  return args;
}

#endif /* PGEN_ARGPARSE */
