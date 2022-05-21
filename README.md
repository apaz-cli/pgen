# pgen
A PEG tokenizer/parser-generator.

This is the program that generates the tokenizer and parser for the [Daisho](https://github.com/apaz-cli/Daisho)
programming language. Right now, it's a WIP. The project is not usable.

The syntax of pgen is based on the paper ["Parsing Expression Grammars: A Recognition-Based Syntactic Foundation"](https://bford.info/pub/lang/peg.pdf)
by [Bryan Ford](https://scholar.google.com/citations?hl=en&user=TwyzQP4AAAAJ).
This specific parser is inspired by [packcc](https://github.com/arithy/packcc) by [Arihiro Yoshida](https://github.com/arithy).
However, its purpose is completely different.

The one difference is that peg grammars like the ones you would
write for `packcc` operate on individual characters of the input. For
`pgen`, you define both a tokenizer and a parser. The tokenizer
recognizes and groups together sequences of characters into tokens.
It uses the [maximal munch](https://en.wikipedia.org/wiki/Maximal_munch)
heuristic and throws an error if there are ambiguities. Then the parser
strings together the token stream coming from the tokenizer into an
[abstract syntax tree](https://en.wikipedia.org/wiki/Abstract_syntax_tree).

Unlike other parser-generators such as `yacc` or `packcc`, `pgen` doesn't
hide the fact that it's a virtual machine, and arguably its own
programming language. Embracing this fact opens up the ability to
effortlessly generate Abstract Syntax Trees.


## Roadmap

- [x] Completed `tok` grammar
- [x] Completed `peg` grammar
- [x] Handwritten `tok` parser
- [x] Handwritten `peg` parser
- [x] Automaton construction and minimization
- [ ] Tokenizer codegen
- [ ] PEG AST rewriting pass
- [ ] Optimizations
- [ ] Parser codegen
- [ ] Finishing polish


## License

The license for `pgen` is GPLv3. The license applies only to the files already in this repository.
The code that you generate using `pgen` belongs to you (or whoever has the copyright to the
`.tok`/`.peg` files it was generated from.

However, if you modify or distribute `pgen` itself then that must still follow the rules of the GPL.

If these terms are not acceptable for you, please contact me with your use case.

