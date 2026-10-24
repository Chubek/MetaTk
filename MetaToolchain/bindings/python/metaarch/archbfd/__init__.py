"""Object-format descriptions (ArchBFD) via nanobind.

The catalogue describes the container formats that BFD (binutils/bfd)
implements: ELF, COFF, PE, Mach-O, ECOFF and WebAssembly.  See
bindings/python/README.md and manual/bfd.md for the complete usage guide.
"""
from enum import IntEnum, IntFlag
from os import fspath

from .._native import (ArchBFDCatalogue, ArchBFDConstant, ArchBFDConstantGroup,
                        ArchBFDEncoding, ArchBFDError, ArchBFDObject,
                        ArchBFDObjectRelocation, ArchBFDObjectSection,
                        ArchBFDObjectSymbol, ArchBFDRecord, ArchBFDRelocation,
                        ArchBFDSection, ArchBFDSource, ArchBFDSpec)


class DocumentFormat(IntEnum):
    """The wire format of a document."""

    AUTO = 0
    YAML = 1
    JSON = 2
    XML = 3
    SEXPR = 4


class Format(IntEnum):
    """The object format family a document describes."""

    ELF = 0
    COFF = 1
    PE = 2
    MACHO = 3
    ECOFF = 4
    WASM = 5
    OTHER = 6


class ByteOrder(IntEnum):
    LITTLE = 0
    BIG = 1


class Overflow(IntEnum):
    """BFD's overflow-checking policy for a relocation."""

    DONT = 0
    BITFIELD = 1
    SIGNED = 2
    UNSIGNED = 3
    TRUNCATE = 4
    ERROR = 5


class LoadFlags(IntFlag):
    NONE = 0
    STRICT = 1
    LINT = 2


def load(path, format=DocumentFormat.AUTO, flags=LoadFlags.NONE):
    """Load a document (str or text PathLike); return an owning Spec."""
    return ArchBFDSpec.load(fspath(path), int(format), int(flags))


def load_buffer(data, format, path="<memory>", flags=LoadFlags.NONE):
    """Parse UTF-8 str/bytes. format must be explicit; path locates diagnostics."""
    return ArchBFDSpec.load_buffer(data, int(format), fspath(path), int(flags))


def open_catalogue(path):
    """Open a catalogue file; a directory is scanned instead."""
    return ArchBFDCatalogue.open(fspath(path))


def scan_catalogue(directory):
    """Build a catalogue from every document in a directory."""
    return ArchBFDCatalogue.scan(fspath(directory))


__all__ = ["ArchBFDCatalogue", "ArchBFDConstant", "ArchBFDConstantGroup",
           "ArchBFDEncoding", "ArchBFDError", "ArchBFDObject",
           "ArchBFDObjectRelocation", "ArchBFDObjectSection",
           "ArchBFDObjectSymbol", "ArchBFDRecord", "ArchBFDRelocation",
           "ArchBFDSection", "ArchBFDSource", "ArchBFDSpec", "ByteOrder",
           "DocumentFormat", "Format", "LoadFlags", "Overflow", "load",
           "load_buffer", "open_catalogue", "scan_catalogue"]
