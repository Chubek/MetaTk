# Parzek Documentation

Parzek is a C++20 parser-generator component in MetaTk. It accepts compact grammar
specifications, preprocesses macro/meta constructs, validates lexical and parser
rules, and emits C++ parser artifacts plus optional visitor scaffolding.

## Manual

- [Introduction](manual/01-introduction.md)
- [Installation and Build](manual/02-installation-and-build.md)
- [Grammar Basics](manual/03-grammar-basics.md)
- [Lexical Rules and Channels](manual/04-lexical-rules-and-channels.md)
- [PEG Operators and Joining](manual/05-peg-operators-and-joining.md)
- [Predicates, Meta Variables, and Helpers](manual/06-predicates-meta-variables-and-helpers.md)
- [Code Generation, CLI, and Library API](manual/07-code-generation-cli-and-library-api.md)
- [Visitors, Testing, and Bootstrapping](manual/08-visitors-testing-and-bootstrapping.md)

## API Reference

The generated reference covers `Parzek.hpp`, `Parzek.cpp`, `main.cpp`, and the
manual pages so CLI behavior and embeddable API contracts remain synchronized.
