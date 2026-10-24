// Package metaarch wraps MetaArch ABI v1 using cgo. Link against libmetaarch.
// Specs own C memory and must be closed. Results and metadata are Go-owned
// copies, so they remain valid after Close. Concurrent reads and Close are
// serialized safely; a Spec must not be copied after first use.
package metaarch

/*
#cgo CFLAGS: -I${SRCDIR}/../..
#cgo LDFLAGS: -lmetaarch
#include "MetaArchABI.h"
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

type Format int32

const (
	Auto              Format = C.MA_AUTO
	YAML              Format = C.MA_YAML
	JSON              Format = C.MA_JSON
	XML               Format = C.MA_XML
	SExpr             Format = C.MA_SEXPR
	LoadBackend       uint32 = C.MA_LOAD_BACKEND
	LoadNoCompile     uint32 = C.MA_LOAD_NO_COMPILE
	LoadStrict        uint32 = C.MA_LOAD_STRICT
	LoadLint          uint32 = C.MA_LOAD_LINT
	WriteCompact      uint32 = C.MA_WRITE_COMPACT
	WriteEmbedBackend uint32 = C.MA_WRITE_EMBED_BACKEND
	WriteRefBackend   uint32 = C.MA_WRITE_REF_BACKEND
	WriteNoHeader     uint32 = C.MA_WRITE_NO_HEADER
	WriteSortKeys     uint32 = C.MA_WRITE_SORT_KEYS
	LittleEndian      uint32 = C.MA_LITTLE_ENDIAN
	BigEndian         uint32 = C.MA_BIG_ENDIAN
	Note              int32  = C.MA_NOTE
	Warning           int32  = C.MA_WARNING
	SeverityError     int32  = C.MA_ERROR
)

var ErrClosed = errors.New("metaarch: spec is closed")

type Diagnostic struct {
	Severity            int32
	Line, Column        uint32
	File, Path, Message string
}
type Error struct{ Diagnostics []Diagnostic }

func (e *Error) Error() string {
	if len(e.Diagnostics) == 0 {
		return "metaarch: operation failed"
	}
	return "metaarch: " + e.Diagnostics[len(e.Diagnostics)-1].Message
}

type Target struct {
	Arch, Subarch, ObjectFormat, ABI string
	AddressWidth                     int64
	ByteOrder                        uint32
}
type Register struct {
	Name, Class     string
	Width, Encoding int64
}
type Form struct {
	Mnemonic, Encoder                 string
	OperandCount, VisibleOperandCount uint64
}
type Binding struct {
	Name  string
	Value int64
}
type Relocation struct {
	Name                                                          string
	Offset, Size, BitOffset, BitWidth, ByteOrder, SymbolParameter uint32
	SymbolValue                                                   int64
}
type Encoded struct {
	Bytes       []byte
	Relocations []Relocation
}
type Counts struct{ Targets, Registers, Encoders, Forms uint64 }

type Spec struct {
	mu          sync.RWMutex
	ptr         *C.MaSpec
	diagnostics []Diagnostic
}

func ABIVersion() uint32 { return uint32(C.metaarch_abi_version()) }
func cstring(s string) (*C.char, error) {
	if strings.IndexByte(s, 0) >= 0 {
		return nil, errors.New("metaarch: embedded NUL in name/path")
	}
	return C.CString(s), nil
}
func diagnostics(d *C.MaDiagnostics) []Diagnostic {
	n := C.metaarch_diagnostics_count(d)
	var result []Diagnostic
	for i := C.size_t(0); i < n; i++ {
		var v C.MaDiagnosticView
		if C.metaarch_diagnostics_get(d, i, &v) != C.MA_OK {
			continue
		}
		result = append(result, Diagnostic{int32(v.severity), uint32(v.line), uint32(v.column),
			C.GoString(v.file), C.GoString(v.path), C.GoString(v.message)})
	}
	return result
}
func newDiagnostics() (*C.MaDiagnostics, error) {
	d := C.metaarch_diagnostics_create()
	if d == nil {
		return nil, errors.New("metaarch: cannot allocate diagnostics")
	}
	return d, nil
}

// Load reads a native spec file; use Auto to infer format from its extension.
func Load(path string, format Format, flags uint32) (*Spec, error) {
	p, err := cstring(path)
	if err != nil {
		return nil, err
	}
	defer C.free(unsafe.Pointer(p))
	d, err := newDiagnostics()
	if err != nil {
		return nil, err
	}
	defer C.metaarch_diagnostics_free(d)
	s := C.metaarch_spec_load(p, C.MaFormat(format), C.uint32_t(flags), d)
	ds := diagnostics(d)
	if s == nil {
		return nil, &Error{ds}
	}
	return &Spec{ptr: s, diagnostics: ds}, nil
}

// Parse borrows data only for this call. path provides diagnostic/backend context.
func Parse(data []byte, format Format, path string, flags uint32) (*Spec, error) {
	p, err := cstring(path)
	if err != nil {
		return nil, err
	}
	defer C.free(unsafe.Pointer(p))
	d, err := newDiagnostics()
	if err != nil {
		return nil, err
	}
	defer C.metaarch_diagnostics_free(d)
	var bytes unsafe.Pointer
	if len(data) > 0 {
		bytes = unsafe.Pointer(&data[0])
	}
	s := C.metaarch_spec_parse(bytes, C.size_t(len(data)), C.MaFormat(format), p, C.uint32_t(flags), d)
	ds := diagnostics(d)
	if s == nil {
		return nil, &Error{ds}
	}
	return &Spec{ptr: s, diagnostics: ds}, nil
}

// Close is idempotent and waits for ongoing methods. There is no GC finalizer;
// callers should defer Close immediately after successful Load/Parse.
func (s *Spec) Close() error {
	if s == nil {
		return nil
	}
	s.mu.Lock()
	defer s.mu.Unlock()
	C.metaarch_spec_free(s.ptr)
	s.ptr = nil
	return nil
}
func (s *Spec) Diagnostics() []Diagnostic {
	if s == nil {
		return nil
	}
	s.mu.RLock()
	defer s.mu.RUnlock()
	return append([]Diagnostic(nil), s.diagnostics...)
}
func (s *Spec) Counts() (Counts, error) {
	if s == nil {
		return Counts{}, ErrClosed
	}
	s.mu.RLock()
	defer s.mu.RUnlock()
	if s.ptr == nil {
		return Counts{}, ErrClosed
	}
	return Counts{uint64(C.metaarch_spec_target_count(s.ptr)), uint64(C.metaarch_spec_register_count(s.ptr)),
		uint64(C.metaarch_spec_encoder_count(s.ptr)), uint64(C.metaarch_spec_form_count(s.ptr))}, nil
}
func (s *Spec) Target(index uint) (Target, error) {
	if s == nil {
		return Target{}, ErrClosed
	}
	s.mu.RLock()
	defer s.mu.RUnlock()
	if s.ptr == nil {
		return Target{}, ErrClosed
	}
	var v C.MaTargetView
	if C.metaarch_spec_target(s.ptr, C.size_t(index), &v) != C.MA_OK {
		return Target{}, fmt.Errorf("metaarch: target index %d out of range", index)
	}
	return Target{C.GoString(v.arch), C.GoString(v.subarch), C.GoString(v.object_format), C.GoString(v.abi), int64(v.address_width), uint32(v.byte_order)}, nil
}
func (s *Spec) Register(index uint) (Register, error) {
	if s == nil {
		return Register{}, ErrClosed
	}
	s.mu.RLock()
	defer s.mu.RUnlock()
	if s.ptr == nil {
		return Register{}, ErrClosed
	}
	return s.register(index)
}
func (s *Spec) register(index uint) (Register, error) {
	var v C.MaRegisterView
	if C.metaarch_spec_register(s.ptr, C.size_t(index), &v) != C.MA_OK {
		return Register{}, fmt.Errorf("metaarch: register index %d out of range", index)
	}
	return Register{C.GoString(v.name), C.GoString(v.reg_class), int64(v.width), int64(v.encoding)}, nil
}
func (s *Spec) FindRegister(name string) (Register, error) {
	if s == nil {
		return Register{}, ErrClosed
	}
	s.mu.RLock()
	defer s.mu.RUnlock()
	if s.ptr == nil {
		return Register{}, ErrClosed
	}
	p, err := cstring(name)
	if err != nil {
		return Register{}, err
	}
	defer C.free(unsafe.Pointer(p))
	i := C.metaarch_spec_find_register(s.ptr, p)
	if i < 0 {
		return Register{}, fmt.Errorf("metaarch: register %q not found", name)
	}
	return s.register(uint(i))
}
func (s *Spec) Form(index uint) (Form, error) {
	if s == nil {
		return Form{}, ErrClosed
	}
	s.mu.RLock()
	defer s.mu.RUnlock()
	if s.ptr == nil {
		return Form{}, ErrClosed
	}
	var v C.MaFormView
	if C.metaarch_spec_form(s.ptr, C.size_t(index), &v) != C.MA_OK {
		return Form{}, fmt.Errorf("metaarch: form index %d out of range", index)
	}
	return Form{C.GoString(v.mnemonic), C.GoString(v.encoder), uint64(v.operand_count), uint64(v.visible_operand_count)}, nil
}
func (s *Spec) EncoderName(index uint) (string, error) {
	if s == nil {
		return "", ErrClosed
	}
	s.mu.RLock()
	defer s.mu.RUnlock()
	if s.ptr == nil {
		return "", ErrClosed
	}
	p := C.metaarch_spec_encoder_name(s.ptr, C.size_t(index))
	if p == nil {
		return "", fmt.Errorf("metaarch: encoder index %d out of range", index)
	}
	return C.GoString(p), nil
}
func (s *Spec) FindEncoder(name string) (int32, error) {
	if s == nil {
		return -1, ErrClosed
	}
	s.mu.RLock()
	defer s.mu.RUnlock()
	if s.ptr == nil {
		return -1, ErrClosed
	}
	p, err := cstring(name)
	if err != nil {
		return -1, err
	}
	defer C.free(unsafe.Pointer(p))
	return int32(C.metaarch_spec_find_encoder(s.ptr, p)), nil
}
func (s *Spec) LoadBackend() error {
	if s == nil {
		return ErrClosed
	}
	s.mu.Lock()
	defer s.mu.Unlock()
	if s.ptr == nil {
		return ErrClosed
	}
	d, err := newDiagnostics()
	if err != nil {
		return err
	}
	defer C.metaarch_diagnostics_free(d)
	if C.metaarch_spec_load_backend(s.ptr, d) == 0 {
		return &Error{diagnostics(d)}
	}
	return nil
}
func resultBytes(r *C.MaResult) ([]byte, error) {
	n := uint64(C.metaarch_result_size(r))
	if n > uint64(^uint(0)>>1) {
		return nil, errors.New("metaarch: result exceeds Go slice size")
	}
	if n == 0 {
		return []byte{}, nil
	}
	p := C.metaarch_result_data(r)
	if p == nil {
		return nil, errors.New("metaarch: missing result bytes")
	}
	return append([]byte(nil), unsafe.Slice((*byte)(unsafe.Pointer(p)), int(n))...), nil
}
func encoded(r *C.MaResult, d *C.MaDiagnostics) (Encoded, error) {
	if r == nil {
		return Encoded{}, &Error{diagnostics(d)}
	}
	defer C.metaarch_result_free(r)
	bytes, err := resultBytes(r)
	if err != nil {
		return Encoded{}, err
	}
	out := Encoded{Bytes: bytes}
	for i := C.size_t(0); i < C.metaarch_result_relocation_count(r); i++ {
		var v C.MaRelocationView
		if C.metaarch_result_relocation(r, i, &v) != C.MA_OK {
			return Encoded{}, errors.New("metaarch: invalid relocation result")
		}
		out.Relocations = append(out.Relocations, Relocation{C.GoString(v.name), uint32(v.offset), uint32(v.size),
			uint32(v.bit_offset), uint32(v.bit_width), uint32(v.byte_order), uint32(v.symbol_parameter), int64(v.symbol_value)})
	}
	return out, nil
}
func (s *Spec) Write(format Format, flags uint32) ([]byte, error) {
	if s == nil {
		return nil, ErrClosed
	}
	s.mu.RLock()
	defer s.mu.RUnlock()
	if s.ptr == nil {
		return nil, ErrClosed
	}
	d, err := newDiagnostics()
	if err != nil {
		return nil, err
	}
	defer C.metaarch_diagnostics_free(d)
	r := C.metaarch_spec_write(s.ptr, C.MaFormat(format), C.uint32_t(flags), d)
	if r == nil {
		return nil, &Error{diagnostics(d)}
	}
	defer C.metaarch_result_free(r)
	return resultBytes(r)
}

// Encode binds every encoder parameter by name, without form metadata checks.
func (s *Spec) Encode(encoder string, bindings []Binding, target uint) (Encoded, error) {
	if s == nil {
		return Encoded{}, ErrClosed
	}
	s.mu.RLock()
	defer s.mu.RUnlock()
	if s.ptr == nil {
		return Encoded{}, ErrClosed
	}
	p, err := cstring(encoder)
	if err != nil {
		return Encoded{}, err
	}
	defer C.free(unsafe.Pointer(p))
	names := make([]*C.char, len(bindings))
	values := make([]C.int64_t, len(bindings))
	defer func() {
		for _, name := range names {
			C.free(unsafe.Pointer(name))
		}
	}()
	for i, binding := range bindings {
		names[i], err = cstring(binding.Name)
		if err != nil {
			return Encoded{}, err
		}
		values[i] = C.int64_t(binding.Value)
	}
	var np **C.char
	var vp *C.int64_t
	if len(bindings) > 0 {
		np = &names[0]
		vp = &values[0]
	}
	d, err := newDiagnostics()
	if err != nil {
		return Encoded{}, err
	}
	defer C.metaarch_diagnostics_free(d)
	return encoded(C.metaarch_spec_encode(s.ptr, p, C.size_t(target), np, vp, C.size_t(len(bindings)), d), d)
}

// EncodeForm checks visible positional operands, ranges and register metadata.
func (s *Spec) EncodeForm(form uint, values []int64, target uint) (Encoded, error) {
	if s == nil {
		return Encoded{}, ErrClosed
	}
	s.mu.RLock()
	defer s.mu.RUnlock()
	if s.ptr == nil {
		return Encoded{}, ErrClosed
	}
	converted := make([]C.int64_t, len(values))
	for i, v := range values {
		converted[i] = C.int64_t(v)
	}
	var vp *C.int64_t
	if len(values) > 0 {
		vp = &converted[0]
	}
	d, err := newDiagnostics()
	if err != nil {
		return Encoded{}, err
	}
	defer C.metaarch_diagnostics_free(d)
	return encoded(C.metaarch_spec_encode_form(s.ptr, C.size_t(form), C.size_t(target), vp, C.size_t(len(values)), d), d)
}
