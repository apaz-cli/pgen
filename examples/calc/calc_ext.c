#define PY_SSIZE_T_CLEAN


#if __has_include(<Python.h>)
#include <Python.h>
#else
#include <python3.11/Python.h> // linter
#endif

#include <stdint.h>
#include <stdio.h>

#include "calc.h"

_Static_assert(sizeof(int32_t) == sizeof(int), "int32_t is not int");

static PyObject *ast_to_python_dict(calc_astnode_t *node) {
  if (!node)
    Py_RETURN_NONE;

  // Create a Python dictionary to hold the AST node data
  PyObject *dict = PyDict_New();
  if (!dict)
    return NULL;

  // Add kind to the dictionary
  const char *kind_str = calc_nodekind_name[node->kind];
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

static PyObject *calc_ext_parse_expr(PyObject *self, PyObject *args) {

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
  calc_tokenizer tokenizer;
  calc_tokenizer_init(&tokenizer, cps, cpslen);

  // Token list
  static const size_t initial_cap = 4096 * 8;
  struct {
    calc_token *buf;
    size_t size;
    size_t cap;
  } toklist = {(calc_token *)malloc(sizeof(calc_token) * initial_cap), 0, initial_cap};
  if (!toklist.buf) {
    free(cps);
    PyErr_SetString(PyExc_RuntimeError, "Out of memory allocating token list.");
    return NULL;
  }

  // Parse tokens
  calc_token tok;
  do {
    tok = calc_nextToken(&tokenizer);
    if (!(tok.kind == CALC_TOK_STREAMEND || tok.kind == CALC_TOK_WS)) {
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
  } while (tok.kind != CALC_TOK_STREAMEND);

  // Initialize parser
  pgen_allocator allocator = pgen_allocator_new();
  calc_parser_ctx parser;
  calc_parser_ctx_init(&parser, &allocator, toklist.buf, toklist.size);

  // Parse AST
  calc_astnode_t *ast = calc_parse_expr(&parser);

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
    calc_parse_err error = parser.errlist[i];
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

static PyObject *calc_ext_parse_sumexpr(PyObject *self, PyObject *args) {

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
  calc_tokenizer tokenizer;
  calc_tokenizer_init(&tokenizer, cps, cpslen);

  // Token list
  static const size_t initial_cap = 4096 * 8;
  struct {
    calc_token *buf;
    size_t size;
    size_t cap;
  } toklist = {(calc_token *)malloc(sizeof(calc_token) * initial_cap), 0, initial_cap};
  if (!toklist.buf) {
    free(cps);
    PyErr_SetString(PyExc_RuntimeError, "Out of memory allocating token list.");
    return NULL;
  }

  // Parse tokens
  calc_token tok;
  do {
    tok = calc_nextToken(&tokenizer);
    if (!(tok.kind == CALC_TOK_STREAMEND || tok.kind == CALC_TOK_WS)) {
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
  } while (tok.kind != CALC_TOK_STREAMEND);

  // Initialize parser
  pgen_allocator allocator = pgen_allocator_new();
  calc_parser_ctx parser;
  calc_parser_ctx_init(&parser, &allocator, toklist.buf, toklist.size);

  // Parse AST
  calc_astnode_t *ast = calc_parse_sumexpr(&parser);

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
    calc_parse_err error = parser.errlist[i];
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

static PyObject *calc_ext_parse_multexpr(PyObject *self, PyObject *args) {

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
  calc_tokenizer tokenizer;
  calc_tokenizer_init(&tokenizer, cps, cpslen);

  // Token list
  static const size_t initial_cap = 4096 * 8;
  struct {
    calc_token *buf;
    size_t size;
    size_t cap;
  } toklist = {(calc_token *)malloc(sizeof(calc_token) * initial_cap), 0, initial_cap};
  if (!toklist.buf) {
    free(cps);
    PyErr_SetString(PyExc_RuntimeError, "Out of memory allocating token list.");
    return NULL;
  }

  // Parse tokens
  calc_token tok;
  do {
    tok = calc_nextToken(&tokenizer);
    if (!(tok.kind == CALC_TOK_STREAMEND || tok.kind == CALC_TOK_WS)) {
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
  } while (tok.kind != CALC_TOK_STREAMEND);

  // Initialize parser
  pgen_allocator allocator = pgen_allocator_new();
  calc_parser_ctx parser;
  calc_parser_ctx_init(&parser, &allocator, toklist.buf, toklist.size);

  // Parse AST
  calc_astnode_t *ast = calc_parse_multexpr(&parser);

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
    calc_parse_err error = parser.errlist[i];
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

static PyObject *calc_ext_parse_baseexpr(PyObject *self, PyObject *args) {

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
  calc_tokenizer tokenizer;
  calc_tokenizer_init(&tokenizer, cps, cpslen);

  // Token list
  static const size_t initial_cap = 4096 * 8;
  struct {
    calc_token *buf;
    size_t size;
    size_t cap;
  } toklist = {(calc_token *)malloc(sizeof(calc_token) * initial_cap), 0, initial_cap};
  if (!toklist.buf) {
    free(cps);
    PyErr_SetString(PyExc_RuntimeError, "Out of memory allocating token list.");
    return NULL;
  }

  // Parse tokens
  calc_token tok;
  do {
    tok = calc_nextToken(&tokenizer);
    if (!(tok.kind == CALC_TOK_STREAMEND || tok.kind == CALC_TOK_WS)) {
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
  } while (tok.kind != CALC_TOK_STREAMEND);

  // Initialize parser
  pgen_allocator allocator = pgen_allocator_new();
  calc_parser_ctx parser;
  calc_parser_ctx_init(&parser, &allocator, toklist.buf, toklist.size);

  // Parse AST
  calc_astnode_t *ast = calc_parse_baseexpr(&parser);

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
    calc_parse_err error = parser.errlist[i];
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

static PyMethodDef calc_methods[] = {
    {"parse_expr", calc_ext_parse_expr, METH_VARARGS, "Parse a expr and return the AST."},
    {"parse_sumexpr", calc_ext_parse_sumexpr, METH_VARARGS, "Parse a sumexpr and return the AST."},
    {"parse_multexpr", calc_ext_parse_multexpr, METH_VARARGS, "Parse a multexpr and return the AST."},
    {"parse_baseexpr", calc_ext_parse_baseexpr, METH_VARARGS, "Parse a baseexpr and return the AST."},
    {NULL, NULL, 0, NULL}
};

static struct PyModuleDef calcmodule = {PyModuleDef_HEAD_INIT, "calcparser", NULL, -1, calc_methods};

PyMODINIT_FUNC PyInit_calc_parser(void) { return PyModule_Create(&calcmodule); }
