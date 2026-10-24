// Package archbfd wraps the MetaArch BFD object-format catalogue using cgo.
// Link against libmetaarch.
//
// The catalogue owns the documents it loads: a Document returned by Load stays
// valid until the Catalogue is closed.  The values the API returns are Go
// copies, so they outlive the catalogue.  Concurrent reads are safe; closing
// a catalogue while a document is in use is not.
package archbfd

/*
#cgo CFLAGS: -I${SRCDIR}/../../..
#cgo LDFLAGS: -lmetaarch
#include "ArchBFD.h"
#include <stdlib.h>
*/
import "C"

import (
	"errors"
	"fmt"
	"strings"
	"sync"
	"unsafe"
)

// Format is an object format family.
type Format int32

// ByteOrder is the byte order of a container.
type ByteOrder int32

// Overflow is BFD's overflow-checking policy for a relocation.
type Overflow int32

const (
	FormatELF   Format = C.ARCHBFD_FMT_ELF
	FormatCOFF  Format = C.ARCHBFD_FMT_COFF
	FormatPE    Format = C.ARCHBFD_FMT_PE
	FormatMachO Format = C.ARCHBFD_FMT_MACHO
	FormatECOFF Format = C.ARCHBFD_FMT_ECOFF
	FormatWASM  Format = C.ARCHBFD_FMT_WASM
	FormatOther Format = C.ARCHBFD_FMT_OTHER
)

const (
	ByteOrderLittle ByteOrder = C.ARCHBFD_BO_LITTLE
	ByteOrderBig    ByteOrder = C.ARCHBFD_BO_BIG
)

const (
	OverflowDont     Overflow = C.ARCHBFD_OVF_DONT
	OverflowBitfield Overflow = C.ARCHBFD_OVF_BITFIELD
	OverflowSigned   Overflow = C.ARCHBFD_OVF_SIGNED
	OverflowUnsigned Overflow = C.ARCHBFD_OVF_UNSIGNED
	OverflowTruncate Overflow = C.ARCHBFD_OVF_TRUNCATE
	OverflowError    Overflow = C.ARCHBFD_OVF_ERROR
)

const (
	LoadStrict uint32 = C.ARCHBFD_LOAD_STRICT
	LoadLint   uint32 = C.ARCHBFD_LOAD_LINT
)

// ErrClosed reports a use of a closed catalogue or document.
var ErrClosed = errors.New("archbfd: catalogue is closed")

// Diagnostic is one positioned message of the core library.
type Diagnostic struct {
	Severity     int32
	Line, Column uint32
	File, Path   string
	Message      string
}

// Error carries the diagnostics of a failed call.
type Error struct{ Diagnostics []Diagnostic }

func (e *Error) Error() string {
	if len(e.Diagnostics) == 0 {
		return "archbfd: operation failed"
	}
	return "archbfd: " + e.Diagnostics[len(e.Diagnostics)-1].Message
}

// Field is one field of a record layout.
type Field struct {
	Name         string
	Offset, Size int64
	Description  string
}

// Encoding says how a packed field is split.
type Encoding struct {
	Field, Target string
	Mask          uint64
	Shift         int64
}

// Record is a fixed-layout structure of a container.
type Record struct {
	Name, Description string
	Size              int64
	Fields            []Field
	Encodings         []Encoding
}

// Constant is one named number of a group.
type Constant struct {
	Name        string
	Value       int64
	Description string
}

// ConstantGroup is a set of named numbers.
type ConstantGroup struct {
	Name, Description string
	Items             []Constant
}

// Relocation is one entry of the BFD relocation (howto) table.
type Relocation struct {
	Name                  string
	Number, Size, Bitsize int64
	PCRelative            bool
	PCRelativeOffset      bool
	Partial               bool
	Overflow              Overflow
	Rightshift, Bitpos    int64
	SrcMask, DstMask      uint64
	Description           string
}

// Section is a well-known section name of a format.
type Section struct {
	Name, Description string
}

// Source records where a description was taken from.
type Source struct {
	File, Note string
}

// Catalogue owns the documents it loads.
type Catalogue struct {
	mu  sync.RWMutex
	ptr *C.ArchBFD
}

// Open reads a catalogue file; a directory is scanned instead.
func Open(path string) (*Catalogue, error) {
	p, err := cstring(path)
	if err != nil {
		return nil, err
	}
	defer C.free(unsafe.Pointer(p))
	var diags C.DsymDiags
	C.dsym_diags_init(&diags)
	defer C.dsym_diags_free(&diags)
	db := C.archbfd_open(p, &diags)
	if db == nil {
		return nil, &Error{diagnostics(&diags)}
	}
	return &Catalogue{ptr: db}, nil
}

// Scan builds a catalogue from every document in a directory.
func Scan(dir string) (*Catalogue, error) {
	p, err := cstring(dir)
	if err != nil {
		return nil, err
	}
	defer C.free(unsafe.Pointer(p))
	var diags C.DsymDiags
	C.dsym_diags_init(&diags)
	defer C.dsym_diags_free(&diags)
	db := C.archbfd_scan(p, &diags)
	if db == nil {
		return nil, &Error{diagnostics(&diags)}
	}
	return &Catalogue{ptr: db}, nil
}

// Close releases the catalogue and every document it owns.
func (c *Catalogue) Close() error {
	if c == nil {
		return nil
	}
	c.mu.Lock()
	defer c.mu.Unlock()
	if c.ptr != nil {
		C.archbfd_close(c.ptr)
		c.ptr = nil
	}
	return nil
}

// Count returns the number of documents in the catalogue.
func (c *Catalogue) Count() (int, error) {
	if c == nil {
		return 0, ErrClosed
	}
	c.mu.RLock()
	defer c.mu.RUnlock()
	if c.ptr == nil {
		return 0, ErrClosed
	}
	return int(C.archbfd_count(c.ptr)), nil
}

// Entry describes one catalogue entry.
type Entry struct {
	Name         string
	Path         string
	Format       string
	TargetVector string
}

// Entry returns the entry at an index.
func (c *Catalogue) Entry(index int) (Entry, error) {
	if c == nil {
		return Entry{}, ErrClosed
	}
	c.mu.RLock()
	defer c.mu.RUnlock()
	if c.ptr == nil {
		return Entry{}, ErrClosed
	}
	e := C.archbfd_entry(c.ptr, C.size_t(index))
	if e == nil {
		return Entry{}, fmt.Errorf("archbfd: entry index %d out of range", index)
	}
	return Entry{C.GoString(e.name), C.GoString(e.path), C.GoString(e.format),
		C.GoString(e.target_vector)}, nil
}

// Find looks an entry up by name, target vector or architecture.
func (c *Catalogue) Find(name string) (Entry, error) {
	if c == nil {
		return Entry{}, ErrClosed
	}
	c.mu.RLock()
	defer c.mu.RUnlock()
	if c.ptr == nil {
		return Entry{}, ErrClosed
	}
	p, err := cstring(name)
	if err != nil {
		return Entry{}, err
	}
	defer C.free(unsafe.Pointer(p))
	e := C.archbfd_find(c.ptr, p)
	if e == nil {
		return Entry{}, fmt.Errorf("archbfd: no document named %q", name)
	}
	return Entry{C.GoString(e.name), C.GoString(e.path), C.GoString(e.format),
		C.GoString(e.target_vector)}, nil
}

// Document is a read-only view of a loaded description.  It stays valid
// while its catalogue is open.
type Document struct {
	ptr *C.ArchBFDSpec
}

// Load returns the document of a catalogue entry.
func (c *Catalogue) Load(entry Entry, flags uint32) (*Document, error) {
	if c == nil {
		return nil, ErrClosed
	}
	c.mu.RLock()
	defer c.mu.RUnlock()
	if c.ptr == nil {
		return nil, ErrClosed
	}
	var diags C.DsymDiags
	C.dsym_diags_init(&diags)
	defer C.dsym_diags_free(&diags)
	name, err := cstring(entry.Name)
	if err != nil {
		return nil, err
	}
	defer C.free(unsafe.Pointer(name))
	found := C.archbfd_find(c.ptr, name)
	if found == nil {
		return nil, fmt.Errorf("archbfd: no document named %q", entry.Name)
	}
	spec := C.archbfd_load(c.ptr, found, C.uint32_t(flags), &diags)
	if spec == nil {
		return nil, &Error{diagnostics(&diags)}
	}
	return &Document{ptr: spec}, nil
}

// LoadName returns the document with the given catalogue name.
func (c *Catalogue) LoadName(name string, flags uint32) (*Document, error) {
	if c == nil {
		return nil, ErrClosed
	}
	c.mu.RLock()
	defer c.mu.RUnlock()
	if c.ptr == nil {
		return nil, ErrClosed
	}
	p, err := cstring(name)
	if err != nil {
		return nil, err
	}
	defer C.free(unsafe.Pointer(p))
	var diags C.DsymDiags
	C.dsym_diags_init(&diags)
	defer C.dsym_diags_free(&diags)
	spec := C.archbfd_load_name(c.ptr, p, C.uint32_t(flags), &diags)
	if spec == nil {
		return nil, &Error{diagnostics(&diags)}
	}
	return &Document{ptr: spec}, nil
}

func (d *Document) spec() *C.ArchBFDSpec {
	if d == nil {
		return nil
	}
	return d.ptr
}

// Name returns the catalogue key of the document.
func (d *Document) Name() string { return C.GoString(d.spec().name) }

// Format returns the object format family.
func (d *Document) Format() string {
	return C.GoString(C.archbfd_format_name(d.spec().format))
}

// ByteOrder returns the byte order of the container.
func (d *Document) ByteOrder() string {
	return C.GoString(C.archbfd_byte_order_name(d.spec().byte_order))
}

// TargetVector returns the BFD target vector of the document.
func (d *Document) TargetVector() string { return C.GoString(d.spec().target_vector) }

// Architecture returns the emulated architecture.
func (d *Document) Architecture() string { return C.GoString(d.spec().arch) }

// Container returns the name of the inherited container document.
func (d *Document) Container() string { return C.GoString(d.spec().container) }

// Magic returns the magic bytes in hexadecimal.
func (d *Document) Magic() string { return C.GoString(d.spec().magic) }

// Description returns the free-text description.
func (d *Document) Description() string { return C.GoString(d.spec().description) }

// RecordCount returns the number of record layouts.
func (d *Document) RecordCount() int { return int(d.spec().record_count) }

// RelocationCount returns the number of relocation entries.
func (d *Document) RelocationCount() int { return int(d.spec().reloc_count) }

// ConstantGroupCount returns the number of constant groups.
func (d *Document) ConstantGroupCount() int { return int(d.spec().constant_group_count) }

// SectionCount returns the number of well-known section names.
func (d *Document) SectionCount() int { return int(d.spec().section_count) }

// SourceCount returns the number of provenance entries.
func (d *Document) SourceCount() int { return int(d.spec().source_count) }

// Source returns one provenance entry.
func (d *Document) Source(index int) (Source, error) {
	if index < 0 || index >= d.SourceCount() {
		return Source{}, fmt.Errorf("archbfd: source index %d out of range", index)
	}
	s := &sources(d.spec())[index]
	return Source{C.GoString(s.file), C.GoString(s.note)}, nil
}

// Record returns one record layout.
func (d *Document) Record(index int) (Record, error) {
	if index < 0 || index >= d.RecordCount() {
		return Record{}, fmt.Errorf("archbfd: record index %d out of range", index)
	}
	r := &records(d.spec())[index]
	return recordOf(r), nil
}

// Relocation returns one relocation entry.
func (d *Document) Relocation(index int) (Relocation, error) {
	if index < 0 || index >= d.RelocationCount() {
		return Relocation{}, fmt.Errorf("archbfd: relocation index %d out of range", index)
	}
	r := &relocations(d.spec())[index]
	return Relocation{C.GoString(r.name), int64(r.number), int64(r.size), int64(r.bitsize),
		bool(r.pcrel), bool(r.pcrel_offset), bool(r.partial), Overflow(r.overflow),
		int64(r.rightshift), int64(r.bitpos), uint64(r.src_mask), uint64(r.dst_mask),
		C.GoString(r.desc)}, nil
}

// ConstantGroup returns one group of named numbers.
func (d *Document) ConstantGroup(index int) (ConstantGroup, error) {
	if index < 0 || index >= d.ConstantGroupCount() {
		return ConstantGroup{}, fmt.Errorf("archbfd: constant group index %d out of range", index)
	}
	g := &constantGroups(d.spec())[index]
	out := ConstantGroup{C.GoString(g.name), C.GoString(g.desc), nil}
	for _, c := range constants(g) {
		out.Items = append(out.Items, Constant{C.GoString(c.name), int64(c.value), C.GoString(c.desc)})
	}
	return out, nil
}

// Section returns one well-known section name.
func (d *Document) Section(index int) (Section, error) {
	if index < 0 || index >= d.SectionCount() {
		return Section{}, fmt.Errorf("archbfd: section index %d out of range", index)
	}
	s := &sections(d.spec())[index]
	return Section{C.GoString(s.name), C.GoString(s.desc)}, nil
}

// FindRelocation looks a relocation up by name.
func (d *Document) FindRelocation(name string) (int32, error) {
	p, err := cstring(name)
	if err != nil {
		return -1, err
	}
	defer C.free(unsafe.Pointer(p))
	return int32(C.archbfd_find_reloc(d.spec(), p)), nil
}

// FindRelocationNumber looks a relocation up by number.
func (d *Document) FindRelocationNumber(number int64) (int32, error) {
	return int32(C.archbfd_find_reloc_number(d.spec(), C.int64_t(number))), nil
}

// FindRecord looks a record layout up by name.
func (d *Document) FindRecord(name string) (Record, error) {
	p, err := cstring(name)
	if err != nil {
		return Record{}, err
	}
	defer C.free(unsafe.Pointer(p))
	r := C.archbfd_find_record(d.spec(), p)
	if r == nil {
		return Record{}, fmt.Errorf("archbfd: no record named %q", name)
	}
	return recordOf(r), nil
}

// Object is a read-only view of an object file.
type Object struct {
	ptr *C.ArchBFDObject
}

// OpenObject reads an object file with a description.
func OpenObject(path string, doc *Document) (*Object, error) {
	p, err := cstring(path)
	if err != nil {
		return nil, err
	}
	defer C.free(unsafe.Pointer(p))
	var diags C.DsymDiags
	C.dsym_diags_init(&diags)
	defer C.dsym_diags_free(&diags)
	obj := C.archbfd_object_open(p, doc.spec(), &diags)
	if obj == nil {
		return nil, &Error{diagnostics(&diags)}
	}
	return &Object{ptr: obj}, nil
}

// Close releases the object.
func (o *Object) Close() error {
	if o == nil {
		return nil
	}
	if o.ptr != nil {
		C.archbfd_object_close(o.ptr)
		o.ptr = nil
	}
	return nil
}

// Kind returns the container kind, e.g. "elf64".
func (o *Object) Kind() string { return C.GoString(C.archbfd_object_kind(o.ptr)) }

// Machine returns the machine type the object declares.
func (o *Object) Machine() int64 { return int64(C.archbfd_object_machine(o.ptr)) }

// Type returns the object type the object declares.
func (o *Object) Type() int64 { return int64(C.archbfd_object_type(o.ptr)) }

// SectionCount returns the number of sections.
func (o *Object) SectionCount() int { return int(C.archbfd_object_section_count(o.ptr)) }

// SymbolCount returns the number of symbols.
func (o *Object) SymbolCount() int { return int(C.archbfd_object_symbol_count(o.ptr)) }

// RelocationCount returns the number of relocations.
func (o *Object) RelocationCount() int { return int(C.archbfd_object_reloc_count(o.ptr)) }

// ObjectSection describes one section of an object file.
type ObjectSection struct {
	Name                            string
	Type, Offset, Size, Addr, Align int64
	Link, Info, Entsize             int64
	RelOffset, RelCount             int64
	Flags                           uint64
}

// Section returns one section of the object.
func (o *Object) Section(index int) (ObjectSection, error) {
	if index < 0 || index >= o.SectionCount() {
		return ObjectSection{}, fmt.Errorf("archbfd: section index %d out of range", index)
	}
	s := C.archbfd_object_section(o.ptr, C.size_t(index))
	return ObjectSection{C.GoString(s.name), int64(s._type), int64(s.offset), int64(s.size),
		int64(s.addr), int64(s.align), int64(s.link), int64(s.info), int64(s.entsize),
		int64(s.rel_offset), int64(s.rel_count), uint64(s.flags)}, nil
}

// ObjectSymbol describes one symbol of an object file.
type ObjectSymbol struct {
	Name                  string
	Value, Size, Shndx    int64
	Bind, Type            string
	TypeValue, ClassValue int64
}

// Symbol returns one symbol of the object.
func (o *Object) Symbol(index int) (ObjectSymbol, error) {
	if index < 0 || index >= o.SymbolCount() {
		return ObjectSymbol{}, fmt.Errorf("archbfd: symbol index %d out of range", index)
	}
	s := C.archbfd_object_symbol(o.ptr, C.size_t(index))
	return ObjectSymbol{C.GoString(s.name), int64(s.value), int64(s.size), int64(s.shndx),
		C.GoString(s.bind), C.GoString(s._type), int64(s.type_value), int64(s.class_value)}, nil
}

// ObjectRelocation describes one relocation of an object file.
type ObjectRelocation struct {
	TypeName, SymbolName string
	Offset, Type, Symbol int64
	Addend               int64
}

// Relocation returns one relocation of the object.
func (o *Object) Relocation(index int) (ObjectRelocation, error) {
	if index < 0 || index >= o.RelocationCount() {
		return ObjectRelocation{}, fmt.Errorf("archbfd: relocation index %d out of range", index)
	}
	r := C.archbfd_object_reloc(o.ptr, C.size_t(index))
	return ObjectRelocation{C.GoString(r.type_name), C.GoString(r.symbol_name),
		int64(r.offset), int64(r._type), int64(r.symbol), int64(r.addend)}, nil
}

// The C model keeps its arrays as pointers with a separate count; these
// helpers expose them as Go slices.
func sources(spec *C.ArchBFDSpec) []C.ArchBFDSource {
	if spec.sources == nil || spec.source_count == 0 {
		return nil
	}
	return (*[1 << 20]C.ArchBFDSource)(unsafe.Pointer(spec.sources))[:spec.source_count:spec.source_count]
}

func records(spec *C.ArchBFDSpec) []C.ArchBFDRecord {
	if spec.records == nil || spec.record_count == 0 {
		return nil
	}
	return (*[1 << 20]C.ArchBFDRecord)(unsafe.Pointer(spec.records))[:spec.record_count:spec.record_count]
}

func relocations(spec *C.ArchBFDSpec) []C.ArchBFDReloc {
	if spec.relocations == nil || spec.reloc_count == 0 {
		return nil
	}
	return (*[1 << 20]C.ArchBFDReloc)(unsafe.Pointer(spec.relocations))[:spec.reloc_count:spec.reloc_count]
}

func constantGroups(spec *C.ArchBFDSpec) []C.ArchBFDConstantGroup {
	if spec.constants == nil || spec.constant_group_count == 0 {
		return nil
	}
	return (*[1 << 20]C.ArchBFDConstantGroup)(unsafe.Pointer(spec.constants))[:spec.constant_group_count:spec.constant_group_count]
}

func constants(group *C.ArchBFDConstantGroup) []C.ArchBFDConstant {
	if group.items == nil || group.count == 0 {
		return nil
	}
	return (*[1 << 20]C.ArchBFDConstant)(unsafe.Pointer(group.items))[:group.count:group.count]
}

func sections(spec *C.ArchBFDSpec) []C.ArchBFDSection {
	if spec.sections == nil || spec.section_count == 0 {
		return nil
	}
	return (*[1 << 20]C.ArchBFDSection)(unsafe.Pointer(spec.sections))[:spec.section_count:spec.section_count]
}

func fields(record *C.ArchBFDRecord) []C.ArchBFDField {
	if record.fields == nil || record.field_count == 0 {
		return nil
	}
	return (*[1 << 20]C.ArchBFDField)(unsafe.Pointer(record.fields))[:record.field_count:record.field_count]
}

func encodings(record *C.ArchBFDRecord) []C.ArchBFDEncoding {
	if record.encodings == nil || record.encoding_count == 0 {
		return nil
	}
	return (*[1 << 20]C.ArchBFDEncoding)(unsafe.Pointer(record.encodings))[:record.encoding_count:record.encoding_count]
}

// recordOf copies one C record into its Go counterpart.
func recordOf(r *C.ArchBFDRecord) Record {
	out := Record{C.GoString(r.name), C.GoString(r.desc), int64(r.size), nil, nil}
	for _, f := range fields(r) {
		out.Fields = append(out.Fields, Field{C.GoString(f.name), int64(f.offset),
			int64(f.size), C.GoString(f.desc)})
	}
	for _, e := range encodings(r) {
		out.Encodings = append(out.Encodings, Encoding{C.GoString(e.field),
			C.GoString(e.target), uint64(e.mask), int64(e.shift)})
	}
	return out
}

func cstring(s string) (*C.char, error) {
	if strings.IndexByte(s, 0) >= 0 {
		return nil, errors.New("archbfd: embedded NUL in name/path")
	}
	return C.CString(s), nil
}

func diagnostics(d *C.DsymDiags) []Diagnostic {
	n := uint64(d.count)
	if n == 0 {
		return nil
	}
	items := (*[1 << 20]C.DsymDiag)(unsafe.Pointer(d.items))[:n:n]
	result := make([]Diagnostic, 0, n)
	for i := range items {
		item := &items[i]
		result = append(result, Diagnostic{int32(item.severity), uint32(item.line),
			uint32(item.col), C.GoString(item.file), C.GoString(item.path),
			C.GoString(item.msg)})
	}
	return result
}
