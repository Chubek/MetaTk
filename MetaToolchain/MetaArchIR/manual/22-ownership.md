# Chapter 22 — Ownership, lifetimes and concurrency

## Owned program and image data

Programs own statement names, operands and attached source filenames. Parsing
does not leave views into the input text. Images own their section bytes, symbol
names and relocation strings. Copying a Program or Image copies that data;
moving it transfers the owning containers.

The `statements()` accessor returns a const reference to the Program's vector.
An append can reallocate the vector, and `clear()` destroys its statements. Do
not retain references or iterators while mutating their owner.

## Shared target ownership

```cpp
auto original = mair::target("amd64", "MetaArchDB");
auto another_owner = original;
auto reg = original.register_info("rax");
std::string owned_name = reg.name;
```

Target copies share an immutable specification instead of repeatedly loading
the database. `register_info` returns a record by value, but the record's string
pointers still borrow from that specification. `spec()` and `description()` also
expose borrowed access. Keep a Target owner alive, or copy the particular strings
and values that must outlive it.

A default-constructed or moved-from Target has no loaded specification and
rejects database access. Move operations do not turn its remaining name text
into a valid substitute for the missing handle.

## Concurrent reads

Independent Programs can be built on different threads. Const rendering and
assembly can share a loaded Target, provided callers do not mutate the underlying
C records through exposed pointers. MetaArchIR does not lazily load backend data
on these paths.

Do not mutate one Program while another thread reads it; provide external
synchronization if that is required. Likewise, coordinate writes to shared output
streams or paths in your application. Shared ownership protects the target's
lifetime, not arbitrary external I/O or user mutations performed through an
escape hatch in a lower-level API.

[Previous](21-parsing-and-errors.md) · [Contents](README.md) · [Next](23-rendering.md)
