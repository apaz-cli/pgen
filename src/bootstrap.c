#include <stdio.h>
#include "util.h"

int main(int argc, char** argv) {

  // Determine target
  char* filename = argc == 2 ? argv[1] : "src/pgen.peg";
  
  // Load target file
  String_View target = readFile(filename);

  // Split into lines.
  

  return 0;
}