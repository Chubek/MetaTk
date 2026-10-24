# 34. Hashing and URI utilities

Hash provides deterministic byte hashing, CRC32, SHA-256 rendering, integer combination, and hexadecimal formatting. Choose according to purpose: FNV-1a and DJB2 are simple noncryptographic hashes, CRC32 detects accidental corruption, and SHA-256 produces a digest. The in-tree implementation is not a promise of a hardened cryptographic subsystem or constant-time authentication.

```metaml
Test.equal "FNV-1a" (Hash.hex (Hash.fnv1a "hello")) "a430d84680aabd0b"
Test.equal "CRC32" (Hash.crc32 "123456789") 3421780262
Test.equal "SHA-256" (Hash.sha256 "abc") "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad"
```

Hash results represented as Int may appear negative because the language exposes signed 64-bit integers. `Hash.hex` displays all bits as 16 hexadecimal digits. `Hash.value` hashes a displayed MetaML representation; it is not a durable schema-independent content identifier or a substitute for explicit canonical serialization.

URI works with records containing scheme, user, host, port, path, query, and fragment. `parse` returns Option; unspecified ports use a negative sentinel. `render` assembles the fields, and parse normalizes supported scheme/host casing. This compact representation does not preserve every lexical distinction, such as all absent-versus-empty delimiters, and is not a full browser URL implementation.

```metaml
Test.equal "percent encoding" (URI.encode "a b/\u{e9}") "a%20b%2F%C3%A9"
Test.equal "bad escape" (URI.decode "%zz") None
Test.equal "query decoding" (URI.query_params "x=a+b&x=two") [("x", "a b"), ("x", "two")]
```

Percent decoding operates on bytes. Query decoding treats plus as space and preserves repeated keys as pairs. Do not treat decoded paths as safe filesystem paths or accepted origins without application-level validation.

**Sources:** [`Hash.h`](../metalib/cpp/Hash.h), [`URI.h`](../metalib/cpp/URI.h), and their D mirrors.

<!-- nav:begin -->
[Contents](README.md) · [Previous](33-tables.md) · [Next](35-json.md)
<!-- nav:end -->
