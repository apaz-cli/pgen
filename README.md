# pgen
A PEG tokenizer/parser-generator.

This is the program that generates the tokenizer and parser for the
[Daisho](https://github.com/apaz-cli/Daisho) programming language.

Given the specification of a grammar, `pgen` generates a very fast
tokenizer and parser for that grammar.

## Usage Example

[usage example](pgen_example.gif)


## Token Syntax
```
// Keywords
CLASS: "class";
PLUS:  "+";

// A state machine that tokenizes single line comments.
SLCOM: (2, 3) {
  (0, '/') -> 1;
  (1, '/') -> 2;
  (2, [^\n]) -> 2;
  (2, [\n]) -> 3;
};

// A state machine that tokenizes whitespace.
WS: 1 {
  ((0, 1), [ \n\r\t]) -> 1;
};

/* Single and multiline C comments are allowed in `.peg` files. */

```


## Parser Syntax
```peg
/* pgen's syntax, written in itself. */

PERCENT: "%";
LESSTHAN "<";
GREATERTHAN: ">";
COMMA: "";
// And so on...

grammar <- (directive / definition)*

directive <- PERCENT LOWERIDENT (&(!eol) WS)* eol

definition <- LOWERIDENT variables? ARROW slashexpr

variables <- LESSTHAN variable (COMMA variable)* GREATERTHAN

variable <- (!(GREATERTHAN / COMMA) wildcard)*

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

%node EOL
eol <- {
    bool iseol = 0;
    if (ctx->pos >= ctx->len) {
      iseol = 1;
    } else if (ctx->tokens[ctx->pos - 1].line < ctx->tokens[ctx->pos].line) {
      iseol = 1;
    }
    ret = iseol ? leaf(EOL) : NULL;
}

wildcard <- {
    rule = pgen_astnode_leaf(ctx->alloc, ctx->tokens[ctx->pos++].kind);
}

```

For more a more precise description of the grammar, see `pgen_grammar.peg`. Or just find me and ask me.

The syntax of pgen is based on the paper ["Parsing Expression Grammars: A Recognition-Based Syntactic Foundation"](https://bford.info/pub/lang/peg.pdf)
by [Bryan Ford](https://scholar.google.com/citations?hl=en&user=TwyzQP4AAAAJ). This specific parser is inspired by [packcc](https://github.com/arithy/packcc)
by [Arihiro Yoshida](https://github.com/arithy). You may see many commonalities. The main difference is that while `packcc` parses at the source level,
`pgen` introduces a tokenizer and parses the token stream instead.


## Operators:

* `/`  - Try to match the left side, then try to match the right side. Returns the first that matches. Otherwise fail.
* `&`  - Try to parse, perform the match, but rewind back to the starting position and return SUCC. Otherwise fail as usual.
* `!`  - Try to parse, return SUCC on no match and fail on match.
* `?`  - Optionally match, returning either the result, or SUCC if no match. Does not cause the rule to fail.
* `*`  - Match zero or more. Returns SUCC.
* `+`  - Match one or more. Returns SUCC, or fails if no matches.
* `()` - Matches if all expressions inside match. Returns SUCC or the single match within if there's only one.
* `{}` - Code to insert into the parser. Assign to `ret` for the return value of this expression, or `rule` for the rule.
* `:`  - Capture the info from a match inside a variable in the current rule.
* `|`  - Register an error using the string or expression on the right, and exit all parsing.


## Directives:

* `%oom`         - Define the action that should be taken when out of memory
* `%node`        - Define an ASTNode kind
* `%preinclude`  - Include a file before astnode, but after support libs
* `%include`     - Include a file after astnode, but before the parser
* `%postinclude` - Include a file after the parser
* `%predefine`   - #define something before astnode, but after support libs
* `%define`      - #define something after astnode, but before the parser
* `%postdefine`  - #define something after the parser
* `%precode`     - Insert code before astnode, but after support libs
* `%code`        - Insert code after astnode, but before the parser
* `%postcode`    - Insert code after the parser
* `%extra`       - Add fields to the astnode
* `%extrainit`   - Add initialization to the astnode

## C Builtins:
* `rec(label)`              - Record the parser's state to a label
* `rew(label)`              - Rewind the parser's state to a label
* `node(kind, children...)` - Create an astnode with a kind name and fixed number of children
* `kind(name)`              - Get the enum value of an astnode kind name
* `list(kind)`              - Create an astnode with a kind name and a dynamic number of children
* `leaf(kind)`              - Create an astnode with no children
* `add(list, node)`         - Add an astnode as a child to an astnode created by list()
* `has(node)`               - 0 if the node is NULL or SUCC, 1 otherwise.
* `repr(node, ofnode)`      - Set the string representation of the current node to another node's
* `srepr(node, string)`     - Set the string representation of node to a cstring
* `cprepr(node, cps, len)`  - Set the string representation of node to a codepoint string
* `expect(kind, cap)       - Parses a token the same way `TOKEN` does. Returns the astnode if cap(tured).


### Error Logging Builtins

* `INFO(msg)`               - Log an error to ctx->errlist with the position and severity 0.
* `WARNING(msg)`            - Log an error to ctx->errlist with the position and severity 1.
* `ERROR(msg)`              - Log an error to ctx->errlist with the position and severity 2.
* `FATAL(msg)`              - Log an error to ctx->errlist with the position and severity 3, and sets ctx->exit = 1.
* `INFO_F(msg, freefn)`     - INFO(), but pgen_allocator_destroy(ctx->alloc) calls freefn(msg).
* `WARNING_F(msg, freefn)`  - WARNING(), but pgen_allocator_destroy(ctx->alloc) calls freefn(msg).
* `ERROR_F(msg, freefn)`    - ERROR(), but pgen_allocator_destroy(ctx->alloc) calls freefn(msg).
* `FATAL_F(msg, freefn)`    - FATAL(), but pgen_allocator_destroy(ctx->alloc) calls freefn(msg).


### Notes:

There's documentation, but realistically you're not going to figure everything out on your own. Talk to me, submit an issue, send me an email, or find me on Discord, and I can walk you through how to use it.

C code in Code expressions are parsed by matching left and right curly braces. Therefore, it could get confused if you write something like `{ ret = ...; ret->str = "}"; }`. Instead of using `"{"` or `"}"`, you can use the macros `LBSTR`/`RBSTR`.


## Generated Parser C API Example:

See `examples/pl0.c` for the full example put together.


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

pl0_token tok;
do {
  tok = pl0_nextToken(&tokenizer);

  // Discard whitespace, comments, and end of stream,
  // add other tokens to the list.
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

Any rule can be an entry point for your parser. The function generated for each rule has the signature:
```c
lang_astnode_t *lang_parse_rulename(lang_parser_ctx* ctx);
```

For our, `pl0` example, `program` is the rule we want, and we call it to parse the abstract syntax tree like so:
```c
pl0_astnode_t *ast = pl0_parse_program(&parser);
```

### 5. When you're done with your AST, clean up whatever memory you used.
```c
pgen_allocator_destroy(&allocator); // The whole AST is freed with the allocator
free(toklist.buf);                  // The list of tokens (provide your own)
free(cps);                          // The file as UTF32
free(input_str);                    // The file as UTF8
```

More comprehensive documentation on these things will come eventually.


## TODO

* Design an algorithm for merging state machines
* Multiple `%node` declarations in one
* State Machine automaton state reachability analysis
* Regex tokenizer rules.
* Add a flag to warn on token/astnode kinds not used in the parser
* Rethink Token/Node print functions
* PGEN_RUNTIME_INCLUDE scope guard
* Rewrite memory allocator with GC and option to leak.
* `%tokenkind`
* Compiler option to generate runner C file
* `%drop` tokens
* `%main` and `%input`, to complement [daisho-explorer](https://github.com/apaz-cli/daisho-explorer).


## License

The license for `pgen` is GPLv3. The license applies only to the files already in this repository. The code that you generate using `pgen` belongs to you (or whoever has the copyright to the `.peg` file it was generated from.

However, if you modify or distribute `pgen` itself then that must still follow the rules of the GPL.

If these terms are not acceptable for you, please contact me with your use case.

