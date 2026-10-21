package domgobind

import (
	"bytes"
	"testing"
	"unsafe"
)

func TestCopy(t *testing.T) {
	input := []byte{'a', 0, 'b'}
	output, err := CopyBytes(unsafe.Pointer(&input[0]), uintptr(len(input)))
	if err != nil || !bytes.Equal(input, output) {
		t.Fatal(output, err)
	}
	input[0] = 'x'
	if output[0] != 'a' {
		t.Fatal("copy aliases input")
	}
	if _, err := CopyBytes(nil, 1); err == nil {
		t.Fatal("accepted nil")
	}
	if output, err := CopyBytes(nil, 0); err != nil || len(output) != 0 {
		t.Fatal(output, err)
	}
}
func TestBoundary(t *testing.T) {
	if Boundary(func() int { panic("test") }) != Panic {
		t.Fatal("panic escaped")
	}
	if Boundary(func() int { return OK }) != OK {
		t.Fatal("success lost")
	}
}
