#define PY_SSIZE_T_CLEAN


#if __has_include(<Python.h>)
#include <Python.h>
#else
#include <python3.11/Python.h> // linter
#endif

#include <stdint.h>
#include <stdio.h>

#include "pl0.h"

_Static_assert(sizeof(int32_t) == sizeof(int), "int32_t is not int");

static PyObject *ast_to_python_dict(pl0_astnode_t *node) {
  if (!node)
    Py_RETURN_NONE;

  // Create a Python dictionary to hold the AST node data
  PyObject *dict = PyDict_New();
  if (!dict)
    return NULL;

  // Add kind to the dictionary
  const char *kind_str = pl0_nodekind_name[node->kind];
  PyObject *kind = PyUnicode_InternFromString(kind_str);
  if (!kind) {
    Py_DECREF(dict);
    return NULL;
  }
  PyDict_SetItemString(dict, "kind", kind);
  Py_DECREF(kind);

  // Convert codepoint array to Python string
  PyObject *tok_repr_str = PyUnicode_FromKindAndData(
      PyUnicode_4BYTE_KIND, node->tok_repr, (ssize_t)node->repr_len);
  if (!tok_repr_str) {
    Py_DECREF(dict);
    return NULL;
  }
  PyDict_SetItemString(dict, "tok_repr", tok_repr_str);
  Py_DECREF(tok_repr_str);

  // Add children to the dictionary
  PyObject *children_list = PyList_New(node->num_children);
  if (!children_list) {
    Py_DECREF(dict);
    return NULL;
  }
  for (uint16_t i = 0; i < node->num_children; i++) {
    PyObject *child = ast_to_python_dict(node->children[i]);
    if (!child) {
      Py_DECREF(children_list);
      Py_DECREF(dict);
      return NULL;
    }
    PyList_SetItem(children_list, (Py_ssize_t)i, child);
  }
  PyDict_SetItemString(dict, "children", children_list);
  Py_DECREF(children_list);

  // TODO: Add other items to the node. Extension point.

  return dict;
}

static PyObject *pl0_ext_parse_program(PyObject *self, PyObject *args) {

  // Extract args
  const char *input_str;
  size_t input_len;
  if (!PyArg_ParseTuple(args, "s#", &input_str, &input_len)) {
    return NULL;
  }

  // Convert input string to UTF-32 codepoints
  codepoint_t *cps = NULL;
  size_t cpslen = 0;
  if (!UTF8_decode((char *)input_str, input_len, &cps, &cpslen)) {
    PyErr_SetString(PyExc_RuntimeError, "Could not decode to UTF32.");
    return NULL;
  }

  // Initialize tokenizer
  pl0_tokenizer tokenizer;
  pl0_tokenizer_init(&tokenizer, cps, cpslen);

  // Token list
  static const size_t initial_cap = 4096 * 8;
  struct {
    pl0_token *buf;
    size_t size;
    size_t cap;
  } toklist = {(pl0_token *)malloc(sizeof(pl0_token) * initial_cap), 0, initial_cap};
  if (!toklist.buf) {
    free(cps);
    PyErr_SetString(PyExc_RuntimeError, "Out of memory allocating token list.");
    return NULL;
  }

  // Parse tokens
  pl0_token tok;
  do {
    tok = pl0_nextToken(&tokenizer);
    if (!(tok.kind == PL0_TOK_STREAMEND || tok.kind == PL0_TOK_WS || tok.kind == PL0_TOK_MLCOM || tok.kind == PL0_TOK_SLCOM)) {
      if (toklist.size == toklist.cap) {
        toklist.buf = realloc(toklist.buf, toklist.cap *= 2);
        if (!toklist.buf) {
          free(cps);
          PyErr_SetString(PyExc_RuntimeError,
                          "Out of memory reallocating token list.");
          return NULL;
        }
      }
      toklist.buf[toklist.size++] = tok;
    }
  } while (tok.kind != PL0_TOK_STREAMEND);

  // Initialize parser
  pgen_allocator allocator = pgen_allocator_new();
  pl0_parser_ctx parser;
  pl0_parser_ctx_init(&parser, &allocator, toklist.buf, toklist.size);

  // Parse AST
  pl0_astnode_t *ast = pl0_parse_program(&parser);

  // Create result dictionary
  PyObject *result_dict = PyDict_New();
  if (!result_dict) {
    pgen_allocator_destroy(&allocator);
    free(toklist.buf);
    free(cps);
    return NULL;
  }

  // Convert AST to Python dictionary
  PyObject *ast_dict = ast_to_python_dict(ast);
  PyDict_SetItemString(result_dict, "ast", ast_dict ? ast_dict : Py_None);
  Py_XDECREF(ast_dict);

  // Create error list
  PyObject *error_list = PyList_New((Py_ssize_t)parser.num_errors);
  if (!error_list) {
    Py_DECREF(result_dict);
    pgen_allocator_destroy(&allocator);
    free(toklist.buf);
    free(cps);
    return NULL;
  }
  char *err_sev_str[] = {"info", "warning", "error", "fatal"};
  for (size_t i = 0; i < parser.num_errors; i++) {
    pl0_parse_err error = parser.errlist[i];
    PyObject *error_dict = PyDict_New();
    if (!error_dict) {
      Py_DECREF(result_dict);
      Py_DECREF(error_list);
      pgen_allocator_destroy(&allocator);
      free(toklist.buf);
      free(cps);
      return NULL;
    }

    // Set error info
    PyDict_SetItemString(error_dict, "msg", PyUnicode_FromString(error.msg));
    PyDict_SetItemString(
        error_dict, "severity",
        PyUnicode_InternFromString(err_sev_str[error.severity]));
    PyDict_SetItemString(error_dict, "line", PyLong_FromSize_t(error.line));
    PyDict_SetItemString(error_dict, "col", PyLong_FromSize_t(error.col));
    PyList_SetItem(error_list, (Py_ssize_t)i, error_dict);
  }
  PyDict_SetItemString(result_dict, "error_list", error_list);
  Py_DECREF(error_list);

  // Clean up
  pgen_allocator_destroy(&allocator);
  free(toklist.buf);
  free(cps);

  return result_dict;
}

static PyObject *pl0_ext_parse_vdef(PyObject *self, PyObject *args) {

  // Extract args
  const char *input_str;
  size_t input_len;
  if (!PyArg_ParseTuple(args, "s#", &input_str, &input_len)) {
    return NULL;
  }

  // Convert input string to UTF-32 codepoints
  codepoint_t *cps = NULL;
  size_t cpslen = 0;
  if (!UTF8_decode((char *)input_str, input_len, &cps, &cpslen)) {
    PyErr_SetString(PyExc_RuntimeError, "Could not decode to UTF32.");
    return NULL;
  }

  // Initialize tokenizer
  pl0_tokenizer tokenizer;
  pl0_tokenizer_init(&tokenizer, cps, cpslen);

  // Token list
  static const size_t initial_cap = 4096 * 8;
  struct {
    pl0_token *buf;
    size_t size;
    size_t cap;
  } toklist = {(pl0_token *)malloc(sizeof(pl0_token) * initial_cap), 0, initial_cap};
  if (!toklist.buf) {
    free(cps);
    PyErr_SetString(PyExc_RuntimeError, "Out of memory allocating token list.");
    return NULL;
  }

  // Parse tokens
  pl0_token tok;
  do {
    tok = pl0_nextToken(&tokenizer);
    if (!(tok.kind == PL0_TOK_STREAMEND || tok.kind == PL0_TOK_WS || tok.kind == PL0_TOK_MLCOM || tok.kind == PL0_TOK_SLCOM)) {
      if (toklist.size == toklist.cap) {
        toklist.buf = realloc(toklist.buf, toklist.cap *= 2);
        if (!toklist.buf) {
          free(cps);
          PyErr_SetString(PyExc_RuntimeError,
                          "Out of memory reallocating token list.");
          return NULL;
        }
      }
      toklist.buf[toklist.size++] = tok;
    }
  } while (tok.kind != PL0_TOK_STREAMEND);

  // Initialize parser
  pgen_allocator allocator = pgen_allocator_new();
  pl0_parser_ctx parser;
  pl0_parser_ctx_init(&parser, &allocator, toklist.buf, toklist.size);

  // Parse AST
  pl0_astnode_t *ast = pl0_parse_vdef(&parser);

  // Create result dictionary
  PyObject *result_dict = PyDict_New();
  if (!result_dict) {
    pgen_allocator_destroy(&allocator);
    free(toklist.buf);
    free(cps);
    return NULL;
  }

  // Convert AST to Python dictionary
  PyObject *ast_dict = ast_to_python_dict(ast);
  PyDict_SetItemString(result_dict, "ast", ast_dict ? ast_dict : Py_None);
  Py_XDECREF(ast_dict);

  // Create error list
  PyObject *error_list = PyList_New((Py_ssize_t)parser.num_errors);
  if (!error_list) {
    Py_DECREF(result_dict);
    pgen_allocator_destroy(&allocator);
    free(toklist.buf);
    free(cps);
    return NULL;
  }
  char *err_sev_str[] = {"info", "warning", "error", "fatal"};
  for (size_t i = 0; i < parser.num_errors; i++) {
    pl0_parse_err error = parser.errlist[i];
    PyObject *error_dict = PyDict_New();
    if (!error_dict) {
      Py_DECREF(result_dict);
      Py_DECREF(error_list);
      pgen_allocator_destroy(&allocator);
      free(toklist.buf);
      free(cps);
      return NULL;
    }

    // Set error info
    PyDict_SetItemString(error_dict, "msg", PyUnicode_FromString(error.msg));
    PyDict_SetItemString(
        error_dict, "severity",
        PyUnicode_InternFromString(err_sev_str[error.severity]));
    PyDict_SetItemString(error_dict, "line", PyLong_FromSize_t(error.line));
    PyDict_SetItemString(error_dict, "col", PyLong_FromSize_t(error.col));
    PyList_SetItem(error_list, (Py_ssize_t)i, error_dict);
  }
  PyDict_SetItemString(result_dict, "error_list", error_list);
  Py_DECREF(error_list);

  // Clean up
  pgen_allocator_destroy(&allocator);
  free(toklist.buf);
  free(cps);

  return result_dict;
}

static PyObject *pl0_ext_parse_block(PyObject *self, PyObject *args) {

  // Extract args
  const char *input_str;
  size_t input_len;
  if (!PyArg_ParseTuple(args, "s#", &input_str, &input_len)) {
    return NULL;
  }

  // Convert input string to UTF-32 codepoints
  codepoint_t *cps = NULL;
  size_t cpslen = 0;
  if (!UTF8_decode((char *)input_str, input_len, &cps, &cpslen)) {
    PyErr_SetString(PyExc_RuntimeError, "Could not decode to UTF32.");
    return NULL;
  }

  // Initialize tokenizer
  pl0_tokenizer tokenizer;
  pl0_tokenizer_init(&tokenizer, cps, cpslen);

  // Token list
  static const size_t initial_cap = 4096 * 8;
  struct {
    pl0_token *buf;
    size_t size;
    size_t cap;
  } toklist = {(pl0_token *)malloc(sizeof(pl0_token) * initial_cap), 0, initial_cap};
  if (!toklist.buf) {
    free(cps);
    PyErr_SetString(PyExc_RuntimeError, "Out of memory allocating token list.");
    return NULL;
  }

  // Parse tokens
  pl0_token tok;
  do {
    tok = pl0_nextToken(&tokenizer);
    if (!(tok.kind == PL0_TOK_STREAMEND || tok.kind == PL0_TOK_WS || tok.kind == PL0_TOK_MLCOM || tok.kind == PL0_TOK_SLCOM)) {
      if (toklist.size == toklist.cap) {
        toklist.buf = realloc(toklist.buf, toklist.cap *= 2);
        if (!toklist.buf) {
          free(cps);
          PyErr_SetString(PyExc_RuntimeError,
                          "Out of memory reallocating token list.");
          return NULL;
        }
      }
      toklist.buf[toklist.size++] = tok;
    }
  } while (tok.kind != PL0_TOK_STREAMEND);

  // Initialize parser
  pgen_allocator allocator = pgen_allocator_new();
  pl0_parser_ctx parser;
  pl0_parser_ctx_init(&parser, &allocator, toklist.buf, toklist.size);

  // Parse AST
  pl0_astnode_t *ast = pl0_parse_block(&parser);

  // Create result dictionary
  PyObject *result_dict = PyDict_New();
  if (!result_dict) {
    pgen_allocator_destroy(&allocator);
    free(toklist.buf);
    free(cps);
    return NULL;
  }

  // Convert AST to Python dictionary
  PyObject *ast_dict = ast_to_python_dict(ast);
  PyDict_SetItemString(result_dict, "ast", ast_dict ? ast_dict : Py_None);
  Py_XDECREF(ast_dict);

  // Create error list
  PyObject *error_list = PyList_New((Py_ssize_t)parser.num_errors);
  if (!error_list) {
    Py_DECREF(result_dict);
    pgen_allocator_destroy(&allocator);
    free(toklist.buf);
    free(cps);
    return NULL;
  }
  char *err_sev_str[] = {"info", "warning", "error", "fatal"};
  for (size_t i = 0; i < parser.num_errors; i++) {
    pl0_parse_err error = parser.errlist[i];
    PyObject *error_dict = PyDict_New();
    if (!error_dict) {
      Py_DECREF(result_dict);
      Py_DECREF(error_list);
      pgen_allocator_destroy(&allocator);
      free(toklist.buf);
      free(cps);
      return NULL;
    }

    // Set error info
    PyDict_SetItemString(error_dict, "msg", PyUnicode_FromString(error.msg));
    PyDict_SetItemString(
        error_dict, "severity",
        PyUnicode_InternFromString(err_sev_str[error.severity]));
    PyDict_SetItemString(error_dict, "line", PyLong_FromSize_t(error.line));
    PyDict_SetItemString(error_dict, "col", PyLong_FromSize_t(error.col));
    PyList_SetItem(error_list, (Py_ssize_t)i, error_dict);
  }
  PyDict_SetItemString(result_dict, "error_list", error_list);
  Py_DECREF(error_list);

  // Clean up
  pgen_allocator_destroy(&allocator);
  free(toklist.buf);
  free(cps);

  return result_dict;
}

static PyObject *pl0_ext_parse_statement(PyObject *self, PyObject *args) {

  // Extract args
  const char *input_str;
  size_t input_len;
  if (!PyArg_ParseTuple(args, "s#", &input_str, &input_len)) {
    return NULL;
  }

  // Convert input string to UTF-32 codepoints
  codepoint_t *cps = NULL;
  size_t cpslen = 0;
  if (!UTF8_decode((char *)input_str, input_len, &cps, &cpslen)) {
    PyErr_SetString(PyExc_RuntimeError, "Could not decode to UTF32.");
    return NULL;
  }

  // Initialize tokenizer
  pl0_tokenizer tokenizer;
  pl0_tokenizer_init(&tokenizer, cps, cpslen);

  // Token list
  static const size_t initial_cap = 4096 * 8;
  struct {
    pl0_token *buf;
    size_t size;
    size_t cap;
  } toklist = {(pl0_token *)malloc(sizeof(pl0_token) * initial_cap), 0, initial_cap};
  if (!toklist.buf) {
    free(cps);
    PyErr_SetString(PyExc_RuntimeError, "Out of memory allocating token list.");
    return NULL;
  }

  // Parse tokens
  pl0_token tok;
  do {
    tok = pl0_nextToken(&tokenizer);
    if (!(tok.kind == PL0_TOK_STREAMEND || tok.kind == PL0_TOK_WS || tok.kind == PL0_TOK_MLCOM || tok.kind == PL0_TOK_SLCOM)) {
      if (toklist.size == toklist.cap) {
        toklist.buf = realloc(toklist.buf, toklist.cap *= 2);
        if (!toklist.buf) {
          free(cps);
          PyErr_SetString(PyExc_RuntimeError,
                          "Out of memory reallocating token list.");
          return NULL;
        }
      }
      toklist.buf[toklist.size++] = tok;
    }
  } while (tok.kind != PL0_TOK_STREAMEND);

  // Initialize parser
  pgen_allocator allocator = pgen_allocator_new();
  pl0_parser_ctx parser;
  pl0_parser_ctx_init(&parser, &allocator, toklist.buf, toklist.size);

  // Parse AST
  pl0_astnode_t *ast = pl0_parse_statement(&parser);

  // Create result dictionary
  PyObject *result_dict = PyDict_New();
  if (!result_dict) {
    pgen_allocator_destroy(&allocator);
    free(toklist.buf);
    free(cps);
    return NULL;
  }

  // Convert AST to Python dictionary
  PyObject *ast_dict = ast_to_python_dict(ast);
  PyDict_SetItemString(result_dict, "ast", ast_dict ? ast_dict : Py_None);
  Py_XDECREF(ast_dict);

  // Create error list
  PyObject *error_list = PyList_New((Py_ssize_t)parser.num_errors);
  if (!error_list) {
    Py_DECREF(result_dict);
    pgen_allocator_destroy(&allocator);
    free(toklist.buf);
    free(cps);
    return NULL;
  }
  char *err_sev_str[] = {"info", "warning", "error", "fatal"};
  for (size_t i = 0; i < parser.num_errors; i++) {
    pl0_parse_err error = parser.errlist[i];
    PyObject *error_dict = PyDict_New();
    if (!error_dict) {
      Py_DECREF(result_dict);
      Py_DECREF(error_list);
      pgen_allocator_destroy(&allocator);
      free(toklist.buf);
      free(cps);
      return NULL;
    }

    // Set error info
    PyDict_SetItemString(error_dict, "msg", PyUnicode_FromString(error.msg));
    PyDict_SetItemString(
        error_dict, "severity",
        PyUnicode_InternFromString(err_sev_str[error.severity]));
    PyDict_SetItemString(error_dict, "line", PyLong_FromSize_t(error.line));
    PyDict_SetItemString(error_dict, "col", PyLong_FromSize_t(error.col));
    PyList_SetItem(error_list, (Py_ssize_t)i, error_dict);
  }
  PyDict_SetItemString(result_dict, "error_list", error_list);
  Py_DECREF(error_list);

  // Clean up
  pgen_allocator_destroy(&allocator);
  free(toklist.buf);
  free(cps);

  return result_dict;
}

static PyObject *pl0_ext_parse_condition(PyObject *self, PyObject *args) {

  // Extract args
  const char *input_str;
  size_t input_len;
  if (!PyArg_ParseTuple(args, "s#", &input_str, &input_len)) {
    return NULL;
  }

  // Convert input string to UTF-32 codepoints
  codepoint_t *cps = NULL;
  size_t cpslen = 0;
  if (!UTF8_decode((char *)input_str, input_len, &cps, &cpslen)) {
    PyErr_SetString(PyExc_RuntimeError, "Could not decode to UTF32.");
    return NULL;
  }

  // Initialize tokenizer
  pl0_tokenizer tokenizer;
  pl0_tokenizer_init(&tokenizer, cps, cpslen);

  // Token list
  static const size_t initial_cap = 4096 * 8;
  struct {
    pl0_token *buf;
    size_t size;
    size_t cap;
  } toklist = {(pl0_token *)malloc(sizeof(pl0_token) * initial_cap), 0, initial_cap};
  if (!toklist.buf) {
    free(cps);
    PyErr_SetString(PyExc_RuntimeError, "Out of memory allocating token list.");
    return NULL;
  }

  // Parse tokens
  pl0_token tok;
  do {
    tok = pl0_nextToken(&tokenizer);
    if (!(tok.kind == PL0_TOK_STREAMEND || tok.kind == PL0_TOK_WS || tok.kind == PL0_TOK_MLCOM || tok.kind == PL0_TOK_SLCOM)) {
      if (toklist.size == toklist.cap) {
        toklist.buf = realloc(toklist.buf, toklist.cap *= 2);
        if (!toklist.buf) {
          free(cps);
          PyErr_SetString(PyExc_RuntimeError,
                          "Out of memory reallocating token list.");
          return NULL;
        }
      }
      toklist.buf[toklist.size++] = tok;
    }
  } while (tok.kind != PL0_TOK_STREAMEND);

  // Initialize parser
  pgen_allocator allocator = pgen_allocator_new();
  pl0_parser_ctx parser;
  pl0_parser_ctx_init(&parser, &allocator, toklist.buf, toklist.size);

  // Parse AST
  pl0_astnode_t *ast = pl0_parse_condition(&parser);

  // Create result dictionary
  PyObject *result_dict = PyDict_New();
  if (!result_dict) {
    pgen_allocator_destroy(&allocator);
    free(toklist.buf);
    free(cps);
    return NULL;
  }

  // Convert AST to Python dictionary
  PyObject *ast_dict = ast_to_python_dict(ast);
  PyDict_SetItemString(result_dict, "ast", ast_dict ? ast_dict : Py_None);
  Py_XDECREF(ast_dict);

  // Create error list
  PyObject *error_list = PyList_New((Py_ssize_t)parser.num_errors);
  if (!error_list) {
    Py_DECREF(result_dict);
    pgen_allocator_destroy(&allocator);
    free(toklist.buf);
    free(cps);
    return NULL;
  }
  char *err_sev_str[] = {"info", "warning", "error", "fatal"};
  for (size_t i = 0; i < parser.num_errors; i++) {
    pl0_parse_err error = parser.errlist[i];
    PyObject *error_dict = PyDict_New();
    if (!error_dict) {
      Py_DECREF(result_dict);
      Py_DECREF(error_list);
      pgen_allocator_destroy(&allocator);
      free(toklist.buf);
      free(cps);
      return NULL;
    }

    // Set error info
    PyDict_SetItemString(error_dict, "msg", PyUnicode_FromString(error.msg));
    PyDict_SetItemString(
        error_dict, "severity",
        PyUnicode_InternFromString(err_sev_str[error.severity]));
    PyDict_SetItemString(error_dict, "line", PyLong_FromSize_t(error.line));
    PyDict_SetItemString(error_dict, "col", PyLong_FromSize_t(error.col));
    PyList_SetItem(error_list, (Py_ssize_t)i, error_dict);
  }
  PyDict_SetItemString(result_dict, "error_list", error_list);
  Py_DECREF(error_list);

  // Clean up
  pgen_allocator_destroy(&allocator);
  free(toklist.buf);
  free(cps);

  return result_dict;
}

static PyObject *pl0_ext_parse_expression(PyObject *self, PyObject *args) {

  // Extract args
  const char *input_str;
  size_t input_len;
  if (!PyArg_ParseTuple(args, "s#", &input_str, &input_len)) {
    return NULL;
  }

  // Convert input string to UTF-32 codepoints
  codepoint_t *cps = NULL;
  size_t cpslen = 0;
  if (!UTF8_decode((char *)input_str, input_len, &cps, &cpslen)) {
    PyErr_SetString(PyExc_RuntimeError, "Could not decode to UTF32.");
    return NULL;
  }

  // Initialize tokenizer
  pl0_tokenizer tokenizer;
  pl0_tokenizer_init(&tokenizer, cps, cpslen);

  // Token list
  static const size_t initial_cap = 4096 * 8;
  struct {
    pl0_token *buf;
    size_t size;
    size_t cap;
  } toklist = {(pl0_token *)malloc(sizeof(pl0_token) * initial_cap), 0, initial_cap};
  if (!toklist.buf) {
    free(cps);
    PyErr_SetString(PyExc_RuntimeError, "Out of memory allocating token list.");
    return NULL;
  }

  // Parse tokens
  pl0_token tok;
  do {
    tok = pl0_nextToken(&tokenizer);
    if (!(tok.kind == PL0_TOK_STREAMEND || tok.kind == PL0_TOK_WS || tok.kind == PL0_TOK_MLCOM || tok.kind == PL0_TOK_SLCOM)) {
      if (toklist.size == toklist.cap) {
        toklist.buf = realloc(toklist.buf, toklist.cap *= 2);
        if (!toklist.buf) {
          free(cps);
          PyErr_SetString(PyExc_RuntimeError,
                          "Out of memory reallocating token list.");
          return NULL;
        }
      }
      toklist.buf[toklist.size++] = tok;
    }
  } while (tok.kind != PL0_TOK_STREAMEND);

  // Initialize parser
  pgen_allocator allocator = pgen_allocator_new();
  pl0_parser_ctx parser;
  pl0_parser_ctx_init(&parser, &allocator, toklist.buf, toklist.size);

  // Parse AST
  pl0_astnode_t *ast = pl0_parse_expression(&parser);

  // Create result dictionary
  PyObject *result_dict = PyDict_New();
  if (!result_dict) {
    pgen_allocator_destroy(&allocator);
    free(toklist.buf);
    free(cps);
    return NULL;
  }

  // Convert AST to Python dictionary
  PyObject *ast_dict = ast_to_python_dict(ast);
  PyDict_SetItemString(result_dict, "ast", ast_dict ? ast_dict : Py_None);
  Py_XDECREF(ast_dict);

  // Create error list
  PyObject *error_list = PyList_New((Py_ssize_t)parser.num_errors);
  if (!error_list) {
    Py_DECREF(result_dict);
    pgen_allocator_destroy(&allocator);
    free(toklist.buf);
    free(cps);
    return NULL;
  }
  char *err_sev_str[] = {"info", "warning", "error", "fatal"};
  for (size_t i = 0; i < parser.num_errors; i++) {
    pl0_parse_err error = parser.errlist[i];
    PyObject *error_dict = PyDict_New();
    if (!error_dict) {
      Py_DECREF(result_dict);
      Py_DECREF(error_list);
      pgen_allocator_destroy(&allocator);
      free(toklist.buf);
      free(cps);
      return NULL;
    }

    // Set error info
    PyDict_SetItemString(error_dict, "msg", PyUnicode_FromString(error.msg));
    PyDict_SetItemString(
        error_dict, "severity",
        PyUnicode_InternFromString(err_sev_str[error.severity]));
    PyDict_SetItemString(error_dict, "line", PyLong_FromSize_t(error.line));
    PyDict_SetItemString(error_dict, "col", PyLong_FromSize_t(error.col));
    PyList_SetItem(error_list, (Py_ssize_t)i, error_dict);
  }
  PyDict_SetItemString(result_dict, "error_list", error_list);
  Py_DECREF(error_list);

  // Clean up
  pgen_allocator_destroy(&allocator);
  free(toklist.buf);
  free(cps);

  return result_dict;
}

static PyObject *pl0_ext_parse_term(PyObject *self, PyObject *args) {

  // Extract args
  const char *input_str;
  size_t input_len;
  if (!PyArg_ParseTuple(args, "s#", &input_str, &input_len)) {
    return NULL;
  }

  // Convert input string to UTF-32 codepoints
  codepoint_t *cps = NULL;
  size_t cpslen = 0;
  if (!UTF8_decode((char *)input_str, input_len, &cps, &cpslen)) {
    PyErr_SetString(PyExc_RuntimeError, "Could not decode to UTF32.");
    return NULL;
  }

  // Initialize tokenizer
  pl0_tokenizer tokenizer;
  pl0_tokenizer_init(&tokenizer, cps, cpslen);

  // Token list
  static const size_t initial_cap = 4096 * 8;
  struct {
    pl0_token *buf;
    size_t size;
    size_t cap;
  } toklist = {(pl0_token *)malloc(sizeof(pl0_token) * initial_cap), 0, initial_cap};
  if (!toklist.buf) {
    free(cps);
    PyErr_SetString(PyExc_RuntimeError, "Out of memory allocating token list.");
    return NULL;
  }

  // Parse tokens
  pl0_token tok;
  do {
    tok = pl0_nextToken(&tokenizer);
    if (!(tok.kind == PL0_TOK_STREAMEND || tok.kind == PL0_TOK_WS || tok.kind == PL0_TOK_MLCOM || tok.kind == PL0_TOK_SLCOM)) {
      if (toklist.size == toklist.cap) {
        toklist.buf = realloc(toklist.buf, toklist.cap *= 2);
        if (!toklist.buf) {
          free(cps);
          PyErr_SetString(PyExc_RuntimeError,
                          "Out of memory reallocating token list.");
          return NULL;
        }
      }
      toklist.buf[toklist.size++] = tok;
    }
  } while (tok.kind != PL0_TOK_STREAMEND);

  // Initialize parser
  pgen_allocator allocator = pgen_allocator_new();
  pl0_parser_ctx parser;
  pl0_parser_ctx_init(&parser, &allocator, toklist.buf, toklist.size);

  // Parse AST
  pl0_astnode_t *ast = pl0_parse_term(&parser);

  // Create result dictionary
  PyObject *result_dict = PyDict_New();
  if (!result_dict) {
    pgen_allocator_destroy(&allocator);
    free(toklist.buf);
    free(cps);
    return NULL;
  }

  // Convert AST to Python dictionary
  PyObject *ast_dict = ast_to_python_dict(ast);
  PyDict_SetItemString(result_dict, "ast", ast_dict ? ast_dict : Py_None);
  Py_XDECREF(ast_dict);

  // Create error list
  PyObject *error_list = PyList_New((Py_ssize_t)parser.num_errors);
  if (!error_list) {
    Py_DECREF(result_dict);
    pgen_allocator_destroy(&allocator);
    free(toklist.buf);
    free(cps);
    return NULL;
  }
  char *err_sev_str[] = {"info", "warning", "error", "fatal"};
  for (size_t i = 0; i < parser.num_errors; i++) {
    pl0_parse_err error = parser.errlist[i];
    PyObject *error_dict = PyDict_New();
    if (!error_dict) {
      Py_DECREF(result_dict);
      Py_DECREF(error_list);
      pgen_allocator_destroy(&allocator);
      free(toklist.buf);
      free(cps);
      return NULL;
    }

    // Set error info
    PyDict_SetItemString(error_dict, "msg", PyUnicode_FromString(error.msg));
    PyDict_SetItemString(
        error_dict, "severity",
        PyUnicode_InternFromString(err_sev_str[error.severity]));
    PyDict_SetItemString(error_dict, "line", PyLong_FromSize_t(error.line));
    PyDict_SetItemString(error_dict, "col", PyLong_FromSize_t(error.col));
    PyList_SetItem(error_list, (Py_ssize_t)i, error_dict);
  }
  PyDict_SetItemString(result_dict, "error_list", error_list);
  Py_DECREF(error_list);

  // Clean up
  pgen_allocator_destroy(&allocator);
  free(toklist.buf);
  free(cps);

  return result_dict;
}

static PyObject *pl0_ext_parse_factor(PyObject *self, PyObject *args) {

  // Extract args
  const char *input_str;
  size_t input_len;
  if (!PyArg_ParseTuple(args, "s#", &input_str, &input_len)) {
    return NULL;
  }

  // Convert input string to UTF-32 codepoints
  codepoint_t *cps = NULL;
  size_t cpslen = 0;
  if (!UTF8_decode((char *)input_str, input_len, &cps, &cpslen)) {
    PyErr_SetString(PyExc_RuntimeError, "Could not decode to UTF32.");
    return NULL;
  }

  // Initialize tokenizer
  pl0_tokenizer tokenizer;
  pl0_tokenizer_init(&tokenizer, cps, cpslen);

  // Token list
  static const size_t initial_cap = 4096 * 8;
  struct {
    pl0_token *buf;
    size_t size;
    size_t cap;
  } toklist = {(pl0_token *)malloc(sizeof(pl0_token) * initial_cap), 0, initial_cap};
  if (!toklist.buf) {
    free(cps);
    PyErr_SetString(PyExc_RuntimeError, "Out of memory allocating token list.");
    return NULL;
  }

  // Parse tokens
  pl0_token tok;
  do {
    tok = pl0_nextToken(&tokenizer);
    if (!(tok.kind == PL0_TOK_STREAMEND || tok.kind == PL0_TOK_WS || tok.kind == PL0_TOK_MLCOM || tok.kind == PL0_TOK_SLCOM)) {
      if (toklist.size == toklist.cap) {
        toklist.buf = realloc(toklist.buf, toklist.cap *= 2);
        if (!toklist.buf) {
          free(cps);
          PyErr_SetString(PyExc_RuntimeError,
                          "Out of memory reallocating token list.");
          return NULL;
        }
      }
      toklist.buf[toklist.size++] = tok;
    }
  } while (tok.kind != PL0_TOK_STREAMEND);

  // Initialize parser
  pgen_allocator allocator = pgen_allocator_new();
  pl0_parser_ctx parser;
  pl0_parser_ctx_init(&parser, &allocator, toklist.buf, toklist.size);

  // Parse AST
  pl0_astnode_t *ast = pl0_parse_factor(&parser);

  // Create result dictionary
  PyObject *result_dict = PyDict_New();
  if (!result_dict) {
    pgen_allocator_destroy(&allocator);
    free(toklist.buf);
    free(cps);
    return NULL;
  }

  // Convert AST to Python dictionary
  PyObject *ast_dict = ast_to_python_dict(ast);
  PyDict_SetItemString(result_dict, "ast", ast_dict ? ast_dict : Py_None);
  Py_XDECREF(ast_dict);

  // Create error list
  PyObject *error_list = PyList_New((Py_ssize_t)parser.num_errors);
  if (!error_list) {
    Py_DECREF(result_dict);
    pgen_allocator_destroy(&allocator);
    free(toklist.buf);
    free(cps);
    return NULL;
  }
  char *err_sev_str[] = {"info", "warning", "error", "fatal"};
  for (size_t i = 0; i < parser.num_errors; i++) {
    pl0_parse_err error = parser.errlist[i];
    PyObject *error_dict = PyDict_New();
    if (!error_dict) {
      Py_DECREF(result_dict);
      Py_DECREF(error_list);
      pgen_allocator_destroy(&allocator);
      free(toklist.buf);
      free(cps);
      return NULL;
    }

    // Set error info
    PyDict_SetItemString(error_dict, "msg", PyUnicode_FromString(error.msg));
    PyDict_SetItemString(
        error_dict, "severity",
        PyUnicode_InternFromString(err_sev_str[error.severity]));
    PyDict_SetItemString(error_dict, "line", PyLong_FromSize_t(error.line));
    PyDict_SetItemString(error_dict, "col", PyLong_FromSize_t(error.col));
    PyList_SetItem(error_list, (Py_ssize_t)i, error_dict);
  }
  PyDict_SetItemString(result_dict, "error_list", error_list);
  Py_DECREF(error_list);

  // Clean up
  pgen_allocator_destroy(&allocator);
  free(toklist.buf);
  free(cps);

  return result_dict;
}

static PyMethodDef pl0_methods[] = {
    {"parse_program", pl0_ext_parse_program, METH_VARARGS, "Parse a program and return the AST."},
    {"parse_vdef", pl0_ext_parse_vdef, METH_VARARGS, "Parse a vdef and return the AST."},
    {"parse_block", pl0_ext_parse_block, METH_VARARGS, "Parse a block and return the AST."},
    {"parse_statement", pl0_ext_parse_statement, METH_VARARGS, "Parse a statement and return the AST."},
    {"parse_condition", pl0_ext_parse_condition, METH_VARARGS, "Parse a condition and return the AST."},
    {"parse_expression", pl0_ext_parse_expression, METH_VARARGS, "Parse a expression and return the AST."},
    {"parse_term", pl0_ext_parse_term, METH_VARARGS, "Parse a term and return the AST."},
    {"parse_factor", pl0_ext_parse_factor, METH_VARARGS, "Parse a factor and return the AST."},
    {NULL, NULL, 0, NULL}
};

static struct PyModuleDef pl0module = {PyModuleDef_HEAD_INIT, "pl0parser", NULL, -1, pl0_methods};

PyMODINIT_FUNC PyInit_pl0_parser(void) { return PyModule_Create(&pl0module); }
