from setuptools import setup, Extension

# Build the generated extension, silencing warnings for valid code that
# pgen generates and turning all other warnings into errors.
module = Extension(
    "pl0_parser",
    sources=["pl0_ext.c"],
    extra_compile_args=[
        "-Wall",
        "-Wextra",
        "-Wpedantic",
        "-Wno-unused-but-set-variable",
        "-Wno-unused-parameter",
        "-Wno-unused-variable",
        "-Wno-missing-field-initializers",
        "-Wconversion",
    ],
)

setup(
    name="pl0_parser",
    version="1.0",
    description="A parser for pl0 generated by pgen.",
    ext_modules=[module],
)