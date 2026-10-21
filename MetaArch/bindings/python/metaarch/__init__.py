"""Architecture specs, serialization and machine-code encoding via nanobind.

See bindings/python/README.md and manual/ for the complete usage guide.
"""
from enum import IntEnum, IntFlag
from os import fspath

from ._native import (Diagnostic, Encoded, Error, Form, Register, Relocation,
                      Spec, Target, abi_version)


class Format(IntEnum):
    AUTO = 0
    YAML = 1
    JSON = 2
    XML = 3
    SEXPR = 4


class LoadFlags(IntFlag):
    NONE = 0
    BACKEND = 1
    NO_COMPILE = 2
    STRICT = 4
    LINT = 8


class WriteFlags(IntFlag):
    NONE = 0
    COMPACT = 1
    EMBED_BACKEND = 2
    REF_BACKEND = 4
    NO_HEADER = 8
    SORT_KEYS = 16


class Severity(IntEnum):
    NOTE = 0
    WARNING = 1
    ERROR = 2


class ByteOrder(IntEnum):
    LITTLE = 0
    BIG = 1


def load(path, format=Format.AUTO, flags=LoadFlags.NONE):
    """Load a file (str or text PathLike); return an owning Spec context manager."""
    return Spec.load(fspath(path), format, flags)


def parse(data, format, path="<memory>", flags=LoadFlags.NONE):
    """Parse UTF-8 str/bytes. format must be explicit; path locates diagnostics."""
    return Spec.parse(data, format, fspath(path), flags)


__version__ = "1.0.0"
__all__ = ["ByteOrder", "Diagnostic", "Encoded", "Error", "Format", "Form",
           "LoadFlags", "Register", "Relocation", "Severity", "Spec", "Target",
           "WriteFlags", "abi_version", "load", "parse"]
