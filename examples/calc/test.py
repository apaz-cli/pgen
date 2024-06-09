import calc_parser
import json

input_str = "(1 + 2) * 3"
expected_ast = {
  "kind": "MULT",
  "tok_repr": "",
  "children": [
    {
      "kind": "PLUS",
      "tok_repr": "",
      "children": [
        {
          "kind": "NUMBER",
          "tok_repr": "1",
          "children": []
        },
        {
          "kind": "NUMBER",
          "tok_repr": "2",
          "children": []
        }
      ]
    },
    {
      "kind": "NUMBER",
      "tok_repr": "3",
      "children": []
    }
  ]
}

try:
    result = calc_parser.parse_expr(input_str)
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