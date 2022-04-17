% pgen(1) | Daisho Programmer's Manual

# NAME

pgen - A tokenizer and parser generator.

# SYNOPSIS

`pgen [-h] [-g] INPUT_TOK [INPUT_PEG] [-o OUTPUT_PATH]`

# DESCRIPTION

`-h, --help`

:   Show this help message and exit

`-o`

:   Provide an output path for the generated tokenizer and/or parser.

# EXAMPLES

`pgen tokenizer.tok`

:   Generates the header file tokenizer.h.

`pgen tokenizer.tok parser.peg`

:   Generates header files tokenizer.h and parser.h.

`pgen tokenizer.tok parser.peg -o output.h`

:   Generates both the tokenizer and parser and concatenates the header files into output.h.

# NOTES

`The header files generated also contain the entire implementation as static inline functions. There is no .c file.`

`You can't generate just a parser with no tokenizer. It must have a token stream to operate on.`

# AUTHOR

`Written by apaz for the Daisho programming language.`

# REPORTING BUGS

`Create an issue at <https://github.com/apaz-cli/pgen>.`

# COPYRIGHT

Copyright © 2022 apaz. License GPLv3+.
