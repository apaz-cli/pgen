
The job of a parser is to turn a token stream into an abstract syntax tree. However, it's not that simple necessarily. It's also not what most
parser-generators provide. Instead, what they provide is a way to recognize the patterns provided in a grammar. The user of the parser-generator
is then instructed to build their own damn AST if they want one so bad.

First of all, what is the correct way to represent an AST? Should each rule have its own type, or should there only be one AST node type? What if I want
to stuff extra data into different kinds of nodes? Should each node have a hashmap in it? Isn't that a bit wasteful?

My resolution to this is that Token and ASTNode* are the same type.
