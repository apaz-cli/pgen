% pgen(1) | Daisho Language Manual

# NAME

pgen - A parser-generator.

# SYNOPSIS

`pgen [-h] [-g] [INPUT_TOK] [INPUT_PEG] [-o OUTPUT_PATH]`

# DESCRIPTION

`-h, --help`

:   Show this help message and exit

`-g`

:   Generate code using GCC/Clang-specific extensions

`-o`

:   Provide an output path for the generated tokenizer and/or parser.

# EXAMPLES

`pgen parser.peg`

:   Generates the header file parser.h.

`pgen tokenizer.tok`

:   Generates the header file tokenizer.h.

`pgen tokenizer.tok parser.peg`

:   Generates the header files tokenizer.h and parser.h.

`pgen tokenizer.tok parser.peg -o output.h`

:   Generates both the tokenizer and parser into the same header file.

# AUTHOR

`Written by apaz, for the Daisho programming language.`

# REPORTING BUGS

`Create an issue at <https://github.com/apaz-cli/pgen>.`

# COPYRIGHT

Copyright © 2022 apaz. License GPLv3+.
