package metaarch

import (
	"bytes"
	"errors"
	"os"
	"sync"
	"testing"
)

const fixture = "../../tests/fixtures/api.yaml"

func loadTest(t *testing.T) *Spec {
	t.Helper()
	s, err := Load(fixture, Auto, LoadStrict|LoadLint)
	if err != nil {
		t.Fatal(err)
	}
	t.Cleanup(func() { s.Close() })
	return s
}

func TestMetadataAndEncoding(t *testing.T) {
	if ABIVersion() != 1 {
		t.Fatal("ABI version")
	}
	s := loadTest(t)
	counts, err := s.Counts()
	if err != nil || counts != (Counts{2, 2, 3, 3}) {
		t.Fatal(counts, err)
	}
	target, err := s.Target(0)
	if err != nil || target.Arch != "test" || target.Subarch != "v1" || target.AddressWidth != 32 || target.ObjectFormat != "ELF32" || target.ABI != "test" {
		t.Fatal(target, err)
	}
	reg, err := s.FindRegister("r1")
	if err != nil || reg.Encoding != 1 || reg.Width != 32 || reg.Class != "gpr" {
		t.Fatal(reg, err)
	}
	form, err := s.Form(0)
	if err != nil || form.OperandCount != 3 || form.VisibleOperandCount != 2 || form.Mnemonic != "pair" || form.Encoder != "pair" {
		t.Fatal(form, err)
	}
	name, err := s.EncoderName(1)
	if err != nil || name != "branch" {
		t.Fatal(name, err)
	}
	index, err := s.FindEncoder("branch")
	if err != nil || index != 1 {
		t.Fatal(index, err)
	}
	out, err := s.Encode("pair", []Binding{{"dst", 1}, {"imm", -1}}, 0)
	if err != nil || !bytes.Equal(out.Bytes, []byte{1, 255, 0, 0}) {
		t.Fatal(out, err)
	}
	out, err = s.EncodeForm(0, []int64{1, -1}, 1)
	if err != nil || !bytes.Equal(out.Bytes, []byte{0, 0, 255, 1}) {
		t.Fatal(out, err)
	}
	if _, err := s.EncodeForm(0, []int64{1, 11}, 0); err == nil {
		t.Fatal("range accepted")
	}
	ret, err := s.Encode("ret", nil, 0)
	if err != nil || !bytes.Equal(ret.Bytes, []byte{0xc3}) {
		t.Fatal(ret, err)
	}
	relocated, err := s.Encode("branch", []Binding{{"symbol", 42}}, 0)
	if err != nil || len(relocated.Relocations) != 1 {
		t.Fatal(relocated, err)
	}
	s.Close()
	s.Close()
	r := relocated.Relocations[0]
	if r.Name != "R_FIX" || r.Offset != 1 || r.Size != 4 || r.BitOffset != 5 || r.BitWidth != 20 || r.ByteOrder != BigEndian || r.SymbolValue != 42 {
		t.Fatal(r)
	}
	if _, err := s.Counts(); !errors.Is(err, ErrClosed) {
		t.Fatal(err)
	}
	if _, err := s.Write(JSON, 0); !errors.Is(err, ErrClosed) {
		t.Fatal(err)
	}
}

func TestRoundTripAndFailures(t *testing.T) {
	s := loadTest(t)
	for _, f := range []Format{YAML, JSON, XML, SExpr} {
		data, err := s.Write(f, WriteRefBackend)
		if err != nil {
			t.Fatal(err)
		}
		copy, err := Parse(data, f, "copy", 0)
		if err != nil {
			t.Fatal(err)
		}
		counts, err := copy.Counts()
		copy.Close()
		if err != nil || counts.Forms != 3 {
			t.Fatal(counts, err)
		}
	}
	if err := s.LoadBackend(); err != nil {
		t.Fatal(err)
	}
	data, err := os.ReadFile(fixture)
	if err != nil {
		t.Fatal(err)
	}
	copy, err := Parse(data, YAML, fixture, 0)
	if err != nil {
		t.Fatal(err)
	}
	copy.Close()
	if len(s.Diagnostics()) != 0 {
		t.Fatal(s.Diagnostics())
	}
	_, err = Parse([]byte("bad: ["), YAML, "bad.yaml", 0)
	var diagnosticError *Error
	if !errors.As(err, &diagnosticError) || len(diagnosticError.Diagnostics) == 0 {
		t.Fatal(err)
	}
	if diagnosticError.Diagnostics[0].Line == 0 {
		t.Fatal("missing parse position")
	}
	if _, err := Load("bad\x00path", Auto, 0); err == nil {
		t.Fatal("embedded NUL")
	}
	if _, err := s.Encode("pair", []Binding{{"dst", 1}, {"dst", 2}}, 0); err == nil {
		t.Fatal("duplicate name")
	}
	if _, err := s.Encode("pair", []Binding{{"bad\x00name", 1}}, 0); err == nil {
		t.Fatal("embedded NUL operand")
	}
	if _, err := s.Target(^uint(0)); err == nil {
		t.Fatal("target bounds")
	}
	if _, err := s.Register(^uint(0)); err == nil {
		t.Fatal("register bounds")
	}
	if _, err := s.FindRegister("missing"); err == nil {
		t.Fatal("unknown register")
	}
	var nilSpec *Spec
	if err := nilSpec.Close(); err != nil {
		t.Fatal(err)
	}
	if _, err := nilSpec.EncodeForm(0, nil, 0); !errors.Is(err, ErrClosed) {
		t.Fatal(err)
	}
}

func TestConcurrentReadAndClose(t *testing.T) {
	s := loadTest(t)
	var wg sync.WaitGroup
	started := make(chan struct{}, 8)
	for i := 0; i < 8; i++ {
		wg.Add(1)
		go func() {
			defer wg.Done()
			if _, err := s.Encode("pair", []Binding{{"dst", 1}, {"imm", -1}}, 0); err != nil {
				t.Error(err)
			}
			started <- struct{}{}
			for j := 0; j < 30; j++ {
				_, err := s.Encode("pair", []Binding{{"dst", 1}, {"imm", -1}}, 0)
				if err != nil && !errors.Is(err, ErrClosed) {
					t.Error(err)
				}
			}
		}()
	}
	for i := 0; i < 8; i++ {
		<-started
	}
	s.Close()
	wg.Wait()
}
