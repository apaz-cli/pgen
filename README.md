# pgen
A PEG tokenizer/parser-generator.

This is the program that generates the tokenizer and parser for the [Daisho](https://github.com/apaz-cli/Daisho)
programming language. Right now, it's a WIP.

The syntax of pgen is based on the paper
["Parsing Expression Grammars: A Recognition-Based Syntactic Foundation"](https://bford.info/pub/lang/peg.pdf)
by [Bryan Ford](https://scholar.google.com/citations?hl=en&user=TwyzQP4AAAAJ). It's bootstrapped from
[packcc](https://github.com/arithy/packcc) by [Arihiro Yoshida](https://github.com/arithy), and also implements
some of the same extensions. It also implements some extra custom extensions for constructing ASTs
([Abstract Syntax Trees](https://en.wikipedia.org/wiki/Abstract_syntax_tree)) effortlessly, since that's notably
lacking in packcc.

## Roadmap

[x] Working PEG grammar
[ ] PackCC extensions
[ ] Tokenizer pgen extensions
[ ] AST pgen extensions
[ ] PEG symbol tables
[ ] Left-recursion elmimination
[ ] Tokenizer code generataion
[ ] Parser code generation
[ ] Daisho grammar
[ ] Daisho symbol tables


## Building

Inside the main project folder, run `./build`. This will invoke packcc, combine the outputs, and compile it.
As time goes on and more roadmap items are complete, there will be more steps.


## License

GPLv3, or contact me with your use case.
If this code is used to train deep learning models, they must be released under GPL (any version).

