"""Type stubs for the object-format (ArchBFD) bindings."""
from enum import IntEnum, IntFlag
from typing import Any, List, Optional, Sequence, Tuple, Union

class DocumentFormat(IntEnum):
    AUTO = 0
    YAML = 1
    JSON = 2
    XML = 3
    SEXPR = 4

class Format(IntEnum):
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

class ArchBFDDiagnostic:
    severity: int
    line: int
    column: int
    file: str
    path: str
    message: str

class ArchBFDError(RuntimeError):
    diagnostics: List[ArchBFDDiagnostic]

class ArchBFDField:
    name: str
    offset: int
    size: int
    desc: str

class ArchBFDEncoding:
    field: str
    target: str
    mask: int
    shift: int

class ArchBFDRecord:
    name: str
    desc: str
    size: int
    fields: List[ArchBFDField]
    encodings: List[ArchBFDEncoding]

class ArchBFDConstant:
    name: str
    value: int
    desc: str

class ArchBFDConstantGroup:
    name: str
    desc: str
    items: List[ArchBFDConstant]

class ArchBFDRelocation:
    name: str
    number: int
    size: int
    bitsize: int
    pcrel: bool
    pcrel_offset: bool
    partial: bool
    overflow: int
    rightshift: int
    bitpos: int
    src_mask: int
    dst_mask: int
    desc: str

class ArchBFDSection:
    name: str
    desc: str

class ArchBFDSource:
    file: str
    note: str

class ArchBFDObjectSection:
    name: str
    type: int
    flags: int
    offset: int
    size: int
    addr: int
    align: int
    link: int
    info: int
    entsize: int
    rel_offset: int
    rel_count: int

class ArchBFDObjectSymbol:
    name: str
    value: int
    size: int
    shndx: int
    bind: str
    type: str
    type_value: int
    class_value: int

class ArchBFDObjectRelocation:
    type_name: str
    symbol_name: str
    offset: int
    type: int
    symbol: int
    addend: int

class ArchBFDSpec:
    @staticmethod
    def load(path: str, format: int = 0, flags: int = 0) -> "ArchBFDSpec": ...
    @staticmethod
    def load_buffer(data: Union[str, bytes], format: int, path: str = "<memory>",
                    flags: int = 0) -> "ArchBFDSpec": ...
    def close(self) -> None: ...
    def __enter__(self) -> "ArchBFDSpec": ...
    def __exit__(self, *args: Any) -> None: ...
    @property
    def diagnostics(self) -> List[ArchBFDDiagnostic]: ...
    @property
    def name(self) -> str: ...
    @property
    def description(self) -> str: ...
    @property
    def format(self) -> str: ...
    @property
    def byte_order(self) -> str: ...
    @property
    def target_vector(self) -> str: ...
    @property
    def arch(self) -> str: ...
    @property
    def container(self) -> str: ...
    @property
    def magic(self) -> str: ...
    @property
    def schema(self) -> int: ...
    @property
    def record_count(self) -> int: ...
    @property
    def reloc_count(self) -> int: ...
    @property
    def constant_group_count(self) -> int: ...
    @property
    def section_count(self) -> int: ...
    @property
    def source_count(self) -> int: ...
    def source(self, index: int) -> ArchBFDSource: ...
    def record(self, index: int) -> ArchBFDRecord: ...
    def relocation(self, index: int) -> ArchBFDRelocation: ...
    def constant_group(self, index: int) -> ArchBFDConstantGroup: ...
    def section(self, index: int) -> ArchBFDSection: ...
    def find_reloc(self, name: str) -> int: ...
    def find_reloc_number(self, number: int) -> int: ...
    def find_record(self, name: str) -> Optional[ArchBFDRecord]: ...
    def constant(self, group: str, name: str) -> Optional[int]: ...
    def write(self, format: int, flags: int = 0) -> str: ...

class ArchBFDCatalogue:
    @staticmethod
    def open(path: str) -> "ArchBFDCatalogue": ...
    @staticmethod
    def scan(directory: str) -> "ArchBFDCatalogue": ...
    def close(self) -> None: ...
    def __enter__(self) -> "ArchBFDCatalogue": ...
    def __exit__(self, *args: Any) -> None: ...
    @property
    def count(self) -> int: ...
    def entry_name(self, index: int) -> str: ...
    def entry_path(self, index: int) -> str: ...
    def entry_format(self, index: int) -> str: ...
    def entry_target_vector(self, index: int) -> str: ...
    def load(self, name: str, flags: int = 0) -> Any: ...

class ArchBFDObject:
    @staticmethod
    def open(path: str, spec: Any) -> "ArchBFDObject": ...
    @staticmethod
    def open_buffer(data: Union[str, bytes], spec: Any) -> "ArchBFDObject": ...
    def close(self) -> None: ...
    def __enter__(self) -> "ArchBFDObject": ...
    def __exit__(self, *args: Any) -> None: ...
    @property
    def kind(self) -> str: ...
    @property
    def machine(self) -> int: ...
    @property
    def type(self) -> int: ...
    @property
    def section_count(self) -> int: ...
    @property
    def symbol_count(self) -> int: ...
    @property
    def relocation_count(self) -> int: ...
    def section(self, index: int) -> ArchBFDObjectSection: ...
    def symbol(self, index: int) -> ArchBFDObjectSymbol: ...
    def relocation(self, index: int) -> ArchBFDObjectRelocation: ...

def load(path: str, format: int = 0, flags: int = 0) -> ArchBFDSpec: ...
def load_buffer(data: Union[str, bytes], format: int, path: str = "<memory>",
                flags: int = 0) -> ArchBFDSpec: ...
def open_catalogue(path: str) -> ArchBFDCatalogue: ...
def scan_catalogue(directory: str) -> ArchBFDCatalogue: ...
