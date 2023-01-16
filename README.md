# pgen
A PEG tokenizer/parser-generator.

This is the program that generates the tokenizer and parser for the
[Daisho](https://github.com/apaz-cli/Daisho) programming language.

Given the specification of a grammar, `pgen` generates a very fast
tokenizer and parser for that grammar.


The syntax of pgen is based on the paper ["Parsing Expression Grammars: A Recognition-Based Syntactic Foundation"](https://bford.info/pub/lang/peg.pdf)
by [Bryan Ford](https://scholar.google.com/citations?hl=en&user=TwyzQP4AAAAJ).
This specific parser is inspired by [packcc](https://github.com/arithy/packcc)
by [Arihiro Yoshida](https://github.com/arithy).
You may see many commonalities. The main difference is that while `packcc`
does away with the lexer (tokenizer), `pgen` re-introduces it.

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
about the parentheses. I only want the info within, and I want it
structured in the way that I want it structured. The shape of the AST
that you're expecting at the end usually turns out to be very different
from the shape of the efficient grammar you wrote. There are also issues
of how to implement grammars efficiently with fixed sized allocations.

A major difference between `pgen` and peg grammars like the ones you would
write for `packcc` is that normal peg grammars operate on individual
characters of the input. For `pgen`, you define both a tokenizer and a
parser. The tokenizer recognizes and groups together sequences of characters
into tokens. It uses the [maximal munch](https://en.wikipedia.org/wiki/Maximal_munch)
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
/* pgen's syntax, written (approximately) in itself. */

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
# |  - Register an error using the string or expression on the right, and exit parsing.

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
// srepr(node, string)     - Set the string representation of node to a cstring
// cprepr(node, cps, len)  - Set the string representation of node to a codepoint string
// expect(kind, cap)       - Parses a token the same way `TOKEN` does. Returns the astnode if cap(tured).

// INFO(msg)               - Log an error to ctx->errlist with the position and severity 0.
// WARNING(msg)            - Log an error to ctx->errlist with the position and severity 1.
// ERROR(msg)              - Log an error to ctx->errlist with the position and severity 2.
// FATAL(msg)              - Log an error to ctx->errlist with the position and severity 3.
// INFO_F(msg, freefn)     - INFO(), but pgen_allocator_destroy(ctx->alloc) calls freefn(msg).
// WARNING_F(msg, freefn)  - WARNING(), but pgen_allocator_destroy(ctx->alloc) calls freefn(msg).
// ERROR_F(msg, freefn)    - ERROR(), but pgen_allocator_destroy(ctx->alloc) calls freefn(msg).
// FATAL_F(msg, freefn)    - FATAL(), but pgen_allocator_destroy(ctx->alloc) calls freefn(msg).


// Notes:
// Instead of using an unbalancing { or } inside a codeexpr, use the macros LB or RB.
// Instead of using "{" or "}" use the macros LBSTR or RBSTR.


grammar <- (directive / definition)*

directive <- PERCENT LOWERIDENT (&(!EOL) WS)* EOL

definition <- LOWERIDENT variables? ARROW slashexpr

variables <- LESSTHAN variable (COMMA variable)* GREATERTHAN

variable <- (!(GREATERTHAN / COMMA) {
              /* A demonstration of how to make a wildcard to match any token
                 (except in this case GREATERTHAN or COMMA) by hacking the parser context. */
              ret = pgen_astnode_leaf(ctx->alloc, ctx->tokens[ctx->pos++].kind);
            })*

slashexpr <- modexprlist (DIV modexprlist)*

modexprlist <- modexpr*

modexpr <- (LOWERIDENT COLON)?                    // Variable assignment
           (AMPERSAND / EXCLAIMATION)*            // Operators
           baseexpr                               // The modified expression
           (QUESTION / STAR / PLUS)*              // More Operators
           (PIPE (STRING / baseexpr))?            // Error handlers

baseexpr <- UPPERIDENT                            // Token to match
          / LOWERIDENT !(LT / ARROW)              // Rule to call
          / CODEEXPR                              // Code to execute
          / OPENPAREN slashexpr CLOSEPAREN

```

There's documentation now, but realistically you're not going to figure everything out on your own. Talk to me, submit an issue, send me an email, or find me on Discord, and I can walk you through how to use it.


## C API / Example

See `examples/pl0.c` for the full example put together. Your parser will be generated prefixed by the 

### 1. Load your file into a cstring, then decode it with the UTF8 -> UTF32 decoder.

```c
char *input_str = NULL;
size_t input_len = 0;
readFile("pl0.pl0", &input_str, &input_len);
```
```c
codepoint_t *cps = NULL;
size_t cpslen = 0;
if (!UTF8_decode(input_str, input_len, &cps, &cpslen))
  fprintf(stderr, "Could not decode to UTF32.\n"), exit(1);
```

### 2. Initialize the tokenizer, then run the tokenizer.

You will have to create some sort of list data structure to hold the tokens.
Here, we add a token to that list with `add_tok`. You will have to roll your
own.

This is also the step where you can discard any tokens you don't want.
This you can parse comments and whitespace as tokens, and then ignore them.

The `.kind` member of your token struct will contain what kind of token it is,
as described by your `.tok` file. When there are no more tokens left to parse,
`.kind` of the returned token will be `PL0_TOK_STREAMEND`. You can also create
and append your own `LANG_TOK_STREAMBEGIN` token at the beginning, if you wish.

```c
pl0_tokenizer tokenizer;
pl0_tokenizer_init(&tokenizer, cps, cpslen);
```
```c
pl0_token tok;
do {
  tok = pl0_nextToken(&tokenizer);

  // Discard whitespace and end of stream, add other tokens to the list.
  if (!(tok.kind == PL0_TOK_SLCOM | tok.kind == PL0_TOK_MLCOM |
        tok.kind == PL0_TOK_WS | tok.kind == PL0_TOK_STREAMEND))
    add_tok(tok);

} while (tok.kind != PL0_TOK_STREAMEND);
```

### 3. Initialize the allocator and parser.

```c
pgen_allocator allocator = pgen_allocator_new();
pl0_parser_ctx parser;
pl0_parser_ctx_init(&parser, &allocator, toklist.buf, toklist.size);
```

### 4. Call a rule to parse an AST.

Any rule can be an entry point for your parser.

```c
pl0_astnode_t *ast = pl0_parse_program(&parser);
```

### 5. When you're done with your AST, clean up the memory you used.
```c
pgen_allocator_destroy(&allocator); // The whole AST is freed with the allocator
free(toklist.buf);                  // The list of tokens (roll your own)
free(cps);                          // The file as UTF32
free(input_str);                    // The file as UTF8
```


## TODO
* Context position and allocator rewind independently
* Design an algorithm for merging state machines
* Provide a warning about left recursion
* Implement support in the syntax for error handling
* Multiple `%node` declarations in one
* `%context` and `%contextinit`
* Make sure that every SM transition is reachable
* Add regex rules in `.tok` files
* Add a flag to warn on token/astnode kinds not used in the parser
* Debug messages for parsing failures
* Token/Node print functions
* Segregate out lang-specific generated code from lang-independent

## License

The license for `pgen` is GPLv3. The license applies only to the files already in this repository. The code that you generate using `pgen` belongs to you (or whoever has the copyright to the `.tok`/`.peg` files it was generated from.

However, if you modify or distribute `pgen` itself then that must still follow the rules of the GPL.

If these terms are not acceptable for you, please contact me with your use case.

