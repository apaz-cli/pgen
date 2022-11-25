# pgen
A PEG tokenizer/parser-generator.

This is the program that generates the tokenizer and parser for the
[Daisho](https://github.com/apaz-cli/Daisho) programming language.

Given the specification of a grammar, `pgen` generates a very fast
tokenizer and parser for that grammar.


The syntax of pgen is based on the paper ["Parsing Expression Grammars: A Recognition-Based Syntactic Foundation"](https://bford.info/pub/lang/peg.pdf)
by [Bryan Ford](https://scholar.google.com/citations?hl=en&user=TwyzQP4AAAAJ).
This specific parser is inspired by [packcc](https://github.com/arithy/packcc) by [Arihiro Yoshida](https://github.com/arithy).
You may see many commonalities. The main difference is that while `packcc` does away with the
lexer (tokenizer), `pgen` re-introduces it.

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



## Tokenizer Syntax
```
/* Single and multiline C style comments */

// Literal tokens:
// <uppercase unique name> : <string literal> ;

CLASS: "class";
PLUS:  "+";

// State machine tokens:
// <uppercase unique name> : <set of accepting states> {
//   ( <set of from states> , <on set of characters> ) -> <new state> ;
//   ...
// }

// A state machine that parses single line comments.
SLCOM: (2, 3) {
  (0, '/') -> 1;
  (1, '/') -> 2;
  (2, [^\n]) -> 2;
  (2, [\n]) -> 3;
};

/*
For more details on the grammar of tokenizer files, see `grammar/tok_grammar.peg`.
For an example tokenizer, see `examples/pl0.tok`.
*/
```

## Parser Syntax
```peg
/* pgen's syntax, written in itself. */

// Operators:
// /  - Try to match the left side, then try to match the right side. Returns the first that matches. Otherwise fail.
// &  - Try to parse, perform the match, but rewind back to the starting position and return SUCC. Otherwise fail as usual.
// !  - Try to parse, return SUCC on no match and fail on match.
// ?  - Optionally match, returning either the result, or SUCC if no match. Does not cause the rule to fail.
// *  - Match zero or more. Returns SUCC.
// +  - Match one or more. Returns SUCC, or fails if no matches.
// () - Matches if all expressions inside match. Returns SUCC or the single match within if there's only one.
// {} - Code to insert into the parser. Assign to `ret` for the return value of this expression, or `rule` for the rule.
// :  - Capture the info from a match inside a variable in the current rule.

// Directives:
// %oom         - Define the action that should be taken when out of memory
// %node        - Define an ASTNode kind
// %preinclude  - Include a file before astnode, but after support libs
// %include     - Include a file after astnode, but before the parser
// %postinclude - Include a file after the parser
// %predefine   - #define something before astnode, but after support libs
// %define      - #define something after astnode, but before the parser
// %postdefine  - #define something after the parser
// %precode     - Insert code before astnode, but after support libs
// %code        - Insert code after astnode, but before the parser
// %postcode    - Insert code after the parser
// %extra       - Add fields to the astnode
// %extrainit   - Add initialization to the astnode

// C Builtins:
// rec(label)              - Record the parser's state to a label
// rew(label)              - Rewind the parser's state to a label
// node(kind, children...) - Create an astnode with a kind name and fixed number of children
// kind(name)              - Get the enum value of an astnode kind name
// list(kind)              - Create an astnode with a kind name and a dynamic number of children
// leaf(kind)              - Create an astnode with no children
// add(list, node)         - Add an astnode as a child to an astnode created by list()
// has(node)               - 0 if the node is NULL or SUCC, 1 otherwise.
// repr(node, ofnode)      - Set the string representation of the current node to another node's
// srepr(node, string)     - Set the string representation of the current node to a string
// rret(node)              - return node from the rule immediately, without cleanup

// Notes:
// Instead of using an unbalancing { or } inside a codeexpr, use the macros LB or RB.
// Instead of using "{" or "}" use the macros LBSTR or RBSTR.


grammar <- (directive / definition)*

directive <- PERCENT LOWERIDENT (&(!EOL) WS)* EOL

definition <- LOWERIDENT ARROW slashexpr

slashexpr <- modexprlist (DIV modexprlist)

modexprlist <- modexpr*

modexpr <- (LOWERIDENT COLON)? (AMPERSAND / EXCLAIMATION)* baseexpr (QUESTION / STAR / PLUS)*

baseexpr <- UPPERIDENT
          / LOWERIDENT !ARROW
          / CODEEXPR
          / OPENPAREN slashexpr CLOSEPAREN

```

Realistically, you're not going to figure out the syntax on your own.
Talk to me, submit an issues, send me an email, or find me on Discord, and I can walk you through how to use it.


## Roadmap

- [x] Completed `tok` grammar
- [x] Completed `peg` grammar
- [x] Handwritten `tok` parser
- [x] Handwritten `peg` parser
- [x] Validity checking
- [x] Automaton construction and minimization
- [x] Tokenizer codegen
- [x] Optimizations
- [x] Parser codegen
- [x] Error checking
- [ ] Documentation


## TODO
* Provide a warning about left recursion.
* Implement support in the syntax for error handling.


## License

The license for `pgen` is GPLv3. The license applies only to the files already in this repository.
The code that you generate using `pgen` belongs to you (or whoever has the copyright to the
`.tok`/`.peg` files it was generated from.

However, if you modify or distribute `pgen` itself then that must still follow the rules of the GPL.

If these terms are not acceptable for you, please contact me with your use case.

