#if 0 // Memory debugging.
#define MEMDEBUG 1
#define PRINT_MEMALLOCS 1
#include <apaz-libc/memdebug.h>
#endif

#include "util.h"

int main(int argc, char **argv) {

  // Determine target
  char *filename = argc == 2 ? argv[1] : "src/pgen.peg";
  size_t fnamelen = strlen(filename);

  // Determine file type
  bool isGrammar;
  if (fnamelen >= 4) {
    isGrammar = strcmp(filename + fnamelen - 4, ".peg") == 0;
  } else {
    ERROR("Could not tell whether the file is for a tokenizer or a parser.\n"
          "Please rename with the .tok or .peg extension.");
  }

  // Read the file into lines.
  list_Codepoint_String_View lines = readFileCodepointLines(filename);

  for (size_t i = 0; i < lines.len; i++) {
    printCodepointStringView(list_Codepoint_String_View_get(&lines, i));
  }

  free(list_Codepoint_String_View_get(&lines, 0).str);
  list_Codepoint_String_View_clear(&lines);

  return 0;
}