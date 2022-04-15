#include "list.h"
#include "util.h"

LIST_DECLARE(String_View);
LIST_DEFINE(String_View);

int main(int argc, char **argv) {

  // Determine target
  char *filename = argc == 2 ? argv[1] : "src/pgen.peg";

  // Load target file
  String_View target = readFile(filename);

  // Split into lines.
  list_String_View lines = list_String_View_new();
  size_t last_offset = 0;
  for (size_t i = 0; i < target.len; i++) {
    if ((target.str[i] == '\n') | (i == target.len - 1)) {
      String_View sv = {target.str + last_offset, i - last_offset};
      list_String_View_add(&lines, sv);
      last_offset = i;
    }
  }

  for (size_t i = 0; i < lines.len; i++) {
      printStringView(list_String_View_get(&lines, i));
  }

  return 0;
}