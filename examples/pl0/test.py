import pl0_parser
import json

pl0_program = """
procedure primes;
begin
	arg := 2;
	while arg < max do
	begin
		call isprime;
		if ret = 1 then write arg;
		arg := arg + 1
	end
end;

call primes;

.
"""

expected_ast = {
  "kind": "PROGRAM",
  "tok_repr": "",
  "children": [
    {
      "kind": "PROCLIST",
      "tok_repr": "",
      "children": [
        {
          "kind": "PROC",
          "tok_repr": "",
          "children": [
            {
              "kind": "IDENT",
              "tok_repr": "primes",
              "children": []
            }
          ]
        },
        {
          "kind": "BEGIN",
          "tok_repr": "",
          "children": [
            {
              "kind": "STATEMENT",
              "tok_repr": "",
              "children": [
                {
                  "kind": "CEQ",
                  "tok_repr": "",
                  "children": [
                    {
                      "kind": "IDENT",
                      "tok_repr": "arg",
                      "children": []
                    },
                    {
                      "kind": "EXPRS",
                      "tok_repr": "",
                      "children": [
                        {
                          "kind": "EXPRS",
                          "tok_repr": "",
                          "children": [
                            {
                              "kind": "NUM",
                              "tok_repr": "2",
                              "children": []
                            }
                          ]
                        }
                      ]
                    }
                  ]
                }
              ]
            },
            {
              "kind": "STATEMENT",
              "tok_repr": "",
              "children": [
                {
                  "kind": "WHILE",
                  "tok_repr": "",
                  "children": [
                    {
                      "kind": "BINEXPR",
                      "tok_repr": "",
                      "children": [
                        {
                          "kind": "LT",
                          "tok_repr": "<",
                          "children": []
                        },
                        {
                          "kind": "EXPRS",
                          "tok_repr": "",
                          "children": [
                            {
                              "kind": "EXPRS",
                              "tok_repr": "",
                              "children": [
                                {
                                  "kind": "IDENT",
                                  "tok_repr": "arg",
                                  "children": []
                                }
                              ]
                            }
                          ]
                        },
                        {
                          "kind": "EXPRS",
                          "tok_repr": "",
                          "children": [
                            {
                              "kind": "EXPRS",
                              "tok_repr": "",
                              "children": [
                                {
                                  "kind": "IDENT",
                                  "tok_repr": "max",
                                  "children": []
                                }
                              ]
                            }
                          ]
                        }
                      ]
                    },
                    {
                      "kind": "BEGIN",
                      "tok_repr": "",
                      "children": [
                        {
                          "kind": "STATEMENT",
                          "tok_repr": "",
                          "children": [
                            {
                              "kind": "CALL",
                              "tok_repr": "",
                              "children": [
                                {
                                  "kind": "IDENT",
                                  "tok_repr": "isprime",
                                  "children": []
                                }
                              ]
                            }
                          ]
                        },
                        {
                          "kind": "STATEMENT",
                          "tok_repr": "",
                          "children": [
                            {
                              "kind": "IF",
                              "tok_repr": "",
                              "children": [
                                {
                                  "kind": "BINEXPR",
                                  "tok_repr": "",
                                  "children": [
                                    {
                                      "kind": "EQ",
                                      "tok_repr": "=",
                                      "children": []
                                    },
                                    {
                                      "kind": "EXPRS",
                                      "tok_repr": "",
                                      "children": [
                                        {
                                          "kind": "EXPRS",
                                          "tok_repr": "",
                                          "children": [
                                            {
                                              "kind": "IDENT",
                                              "tok_repr": "ret",
                                              "children": []
                                            }
                                          ]
                                        }
                                      ]
                                    },
                                    {
                                      "kind": "EXPRS",
                                      "tok_repr": "",
                                      "children": [
                                        {
                                          "kind": "EXPRS",
                                          "tok_repr": "",
                                          "children": [
                                            {
                                              "kind": "NUM",
                                              "tok_repr": "1",
                                              "children": []
                                            }
                                          ]
                                        }
                                      ]
                                    }
                                  ]
                                },
                                {
                                  "kind": "WRITE",
                                  "tok_repr": "",
                                  "children": [
                                    {
                                      "kind": "IDENT",
                                      "tok_repr": "arg",
                                      "children": []
                                    }
                                  ]
                                }
                              ]
                            }
                          ]
                        },
                        {
                          "kind": "STATEMENT",
                          "tok_repr": "",
                          "children": [
                            {
                              "kind": "CEQ",
                              "tok_repr": "",
                              "children": [
                                {
                                  "kind": "IDENT",
                                  "tok_repr": "arg",
                                  "children": []
                                },
                                {
                                  "kind": "EXPRS",
                                  "tok_repr": "",
                                  "children": [
                                    {
                                      "kind": "EXPRS",
                                      "tok_repr": "",
                                      "children": [
                                        {
                                          "kind": "IDENT",
                                          "tok_repr": "arg",
                                          "children": []
                                        }
                                      ]
                                    },
                                    {
                                      "kind": "BINEXPR",
                                      "tok_repr": "",
                                      "children": [
                                        {
                                          "kind": "PLUS",
                                          "tok_repr": "+",
                                          "children": []
                                        },
                                        {
                                          "kind": "EXPRS",
                                          "tok_repr": "",
                                          "children": [
                                            {
                                              "kind": "NUM",
                                              "tok_repr": "1",
                                              "children": []
                                            }
                                          ]
                                        }
                                      ]
                                    }
                                  ]
                                }
                              ]
                            }
                          ]
                        }
                      ]
                    }
                  ]
                }
              ]
            }
          ]
        }
      ]
    },
    {
      "kind": "PROCLIST",
      "tok_repr": "",
      "children": [
        {
          "kind": "CALL",
          "tok_repr": "",
          "children": [
            {
              "kind": "IDENT",
              "tok_repr": "primes",
              "children": []
            }
          ]
        }
      ]
    }
  ]
}

try:
    result = pl0_parser.parse_program(pl0_program)
    if result is None:
        print("Return is none.")
        exit(1)
    ast = result["ast"]
    error_list = result["error_list"]
    if ast:
        if ast != expected_ast:
            print("AST does not match expected AST.")
            print("Expected:")
            print(json.dumps(expected_ast, indent=2))
            print("Actual:")
            print(json.dumps(ast, indent=2))
        exit(0)
    else:
        if len(error_list) == 0:
            print("No errors or AST returned.")
        for error in error_list:
            print(f"{error['line']}:{error['col']} - {error['msg']}")
            exit(1)
except Exception as e:
    print(f"An error occurred: {e}")
    exit(1)