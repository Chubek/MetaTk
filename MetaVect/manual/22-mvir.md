# Chapter 22 — MVIR: Design and Contract

**Status:** scaffold (`mvir/MVIR.di`, `mvir/AST2MVIR-Visitor.d`,
`mvir/MVIR-RewriteEngine.d`, `mvir/MVIR-LuaLib.d`, `mvir/MVIR-Patch.d`,
`mvir/MVIR-LuaRewriteSpec.{d,lua}`). Nothing here executes yet.

## Intended role

MVIR is the typed intermediate representation between the validated AST
and backend emission: literals, variables, lambdas, applications, lets,
and vector-graphics primitives, all carrying types and source spans. Every
optimization in chapter 23 operates on MVIR terms, never on source text.

## Contracts on record

- `AST2MVIR-Visitor.d`: lower validated AST nodes to typed MVIR terms.
  Lowering requires validated input — unbound names and malformed trees
  must never reach it.
- `MVIR-RewriteEngine.d`: apply ordered, validated rewrite passes with a
  fuel limit, provenance records, and cycle detection.
- `MVIR-LuaRewriteSpec`: load declarative Lua specs (chapter 23) without
  executing them as code — spec strings are metadata, never `load`ed.
- `MVIR-Patch.d` / `MVIR-LuaLib.d`: represent and support term patches.

Treat these files as the specification of the next implementation tranche,
not as working code.
