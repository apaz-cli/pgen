# pgen
A PEG tokenizer/parser-generator.

This is the program that generates the tokenizer and parser for the
[Daisho](https://github.com/apaz-cli/Daisho) programming language.
Right now, it's a WIP. The project is not usable.

The syntax of pgen is based on the paper ["Parsing Expression Grammars: A Recognition-Based Syntactic Foundation"](https://bford.info/pub/lang/peg.pdf)
by [Bryan Ford](https://scholar.google.com/citations?hl=en&user=TwyzQP4AAAAJ).
This specific parser is inspired by [packcc](https://github.com/arithy/packcc) by [Arihiro Yoshida](https://github.com/arithy).
You may see many commonalities. The difference is that while `packcc` does away with the lexer (tokenizer), 
`pgen` re-introduces it.

The job of a parser is to turn a token stream into an abstract syntax tree.
However, that's not what most parser-generators provide. Instead, what
they provide is a way to recognize the patterns provided in a grammar.
The user of the parser-generator is then instructed to build their own
damn AST if they want one so bad.

Originally, I thought that this would be an easy feature to add.
Shouldn't you be able to build an abstract syntax tree directly from the
grammar? Unfortunately, it's not that simple. For example, it turns out
that you want to be able to throw away information. I don't care about
the commas that I match in a list of function arguments. I don't care
about the parentheses. I only want the info within. The shape of the AST
that you want turns out to usually be very different from the shape of
the grammar you wrote.

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
- [x] Validity checking
- [x] Automaton construction and minimization
- [x] Tokenizer codegen
- [ ] Optimizations
- [ ] Parser codegen
- [ ] Finishing polish


## License

The license for `pgen` is GPLv3. The license applies only to the files already in this repository.
The code that you generate using `pgen` belongs to you (or whoever has the copyright to the
`.tok`/`.peg` files it was generated from.

However, if you modify or distribute `pgen` itself then that must still follow the rules of the GPL.

If these terms are not acceptable for you, please contact me with your use case.

