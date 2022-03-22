# pgen
A PEG tokenizer/parser-generator.

This is the program that generates the tokenizer and parser for the [Daisho](https://github.com/apaz-cli/Daisho)
programming language. Right now, it's a WIP.

The syntax of pgen is based on the paper ["Parsing Expression Grammars: A Recognition-Based Syntactic Foundation"](https://bford.info/pub/lang/peg.pdf)
by [Bryan Ford](https://scholar.google.com/citations?hl=en&user=TwyzQP4AAAAJ). It's bootstrapped from and
inspired by [packcc](https://github.com/arithy/packcc) by [Arihiro Yoshida](https://github.com/arithy).
However, its purpose is completely different.

Whereas parser-generators like `yacc` and `packcc` allow you to execute actions when code is matched (which
could be used to tediously build an AST), `pgen` is essentially a virtual machine that generates an AST for
you. It can execute your actions not only at the beginning or end of matching a rule, but at any time. Also,
unlike `packcc` (but like `yacc`), `pgen` builds a tokenizer for you and operates on those, instead of raw
text. This means that if your language has a preprocessor (like C), you can run it inbetween, combining token streams from multiple files in any way you want before stringing them together into your
abstract syntax tree.


It also implements some extra custom extensions for constructing ASTs
([Abstract Syntax Trees](https://en.wikipedia.org/wiki/Abstract_syntax_tree)) effortlessly, since that's
notably lacking in packcc.

## Roadmap

- [x] Working `packcc` grammar for `pgen`
- [x] Completed `pgen` grammar for `pgen`
- [ ] Shared symbol table code
- [ ] Finalized `pgen` grammar written in itself
- [ ] `pccpgen` -> `pccpgen` AST
- [ ] `pccpgen` AST -> normalized `pgen` symbol table
- [ ] `pgen` AST -> normalized `pgen` symbol table
- [ ] Symbol table -> Tokenizer code generation
- [ ] Symbol table -> Parser code generation
- [ ] Working `pgen` abstract machine
- [ ] `pgen` can parse itself
- [ ] `pgen` can generate itself


## The perks of using pgen



## The API

| Type                                                     | Description                    |
| -------------------------------------------------------- | ------------------------------ |
| `pgen_list_##type`                                       | List class with buf, len, cap. |
| `enum pgen_toktype`                                      | The names of all the tokens.   |
| `struct pgen_token`                                      | `toktype` and match.           |
| `struct pgen_astnode`                                    |                                |

| Function                                                 | Description                    |
| -------------------------------------------------------- | ------------------------------ |
| `char* pgen_readfile(char*)`                             |                                |
| `pgen_list_pgen_token pgen_name_tokenize(char*, size_t)` |                                |
| `pgen_astnode* pgen_name_parse(pgen_list_pgen_token)`    |                                |

| Variables available inside an action    | Type           | Description                    |
| --------------------------------------- | -------------- | ------------------------------ |
| ... | |


## The pgen abstract machine

Starting with the rule called `Grammar`, the Tokenizer rules are


## Tips on writing grammars

While pgen solves the grammar production determinism problem with its abstract machine, it does not excempt
the user from thinking about the problem. Otherwise, it may just do the wrong thing every time.

The common thing to do is to think about the beginning of



## Bootstrapping

First, build the latest version of [packcc](https://github.com/arithy/packcc). Put the binary in
`packcc/packcc`. The code should already be included in this repo at `packcc/packcc.c`. A linux x86_64
ELF binary is also included. Overwrite it if you're on another platform. Compiling packcc should not
require any special flags. To update/refetch and recompile packcc, it should just be:
```sh
cd packcc/

wget https://raw.githubusercontent.com/arithy/packcc/master/src/packcc.c

cc packcc.c -o packcc

cd ..
```

Once you have your `packcc` binary, you should be able to just put it in the main project folder and type:
```sh
./build
```

In order, this will:
1. Use `packcc` to generate a parser for pgen grammar from a packcc grammar.
2. Combine the `.h` and `.c` files that were generated.
3. Compile the combined file into a bootstrap pgen version.
4. Use the bootstrap pgen version to generate a new tokenizer and parser from a grammar written in pgen itself.
5. Compile the new tokenizer and parser into a new pgen version.
6. Use the new pgen version to parse its own syntax and regenerate the tokenizer and parser for its own binary.
7. Recompile using the new tokenizer and parser.

At the end of it, you should be left with the final `pgen` binary at the root of the project folder.




## License

The code and a binary for `packcc` are included in this repo, but are not a part of the final `pgen`
executable or code generated by `pgen`. Packcc is public domain (to the extent allowable by law). See
the main repo ([https://github.com/arithy/packcc](https://github.com/arithy/packcc)) and PPC_LICENSE for
details.

The license for `pgen` is GPLv3. The license applies only to the files already in this repository (other
than `packcc.c` and `packcc`). The license for the code generated by `pgen` is the license of the grammar
that it was generated from. For example, since the parser for `pgen` is itself generated from `pgen.peg`
which is GPLv3, the resulting `pgen.h` from running `pgen pgen.peg` is also GPLv3. If `pgen.peg` were MIT
licensed, then `pgen.h` would be MIT licensed as well. If you write a grammar, you own the code that
`pgen` generates from it. But if you modify `pgen` itself and distribute it, it has to be released under
GPLv3.

If these terms are not acceptable for you, please contact me with your use case.

