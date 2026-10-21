// Package domgobind supplies byte conversion and panic containment at a cgo boundary.
// C-owned memory is borrowed only for the duration of CopyBytes. Go pointers
// must not be retained by C; use runtime/cgo.Handle for persistent state.
package domgobind

import (
	"errors"
	"unsafe"
)

const (
	OK      = 0
	Invalid = -1
	Panic   = -2
)

// CopyBytes copies a C allocation into Go-owned storage, preserving embedded NULs.
// data must address at least size readable bytes while this call runs.
func CopyBytes(data unsafe.Pointer, size uintptr) ([]byte, error) {
	if size == 0 {
		return []byte{}, nil
	}
	if data == nil || size > uintptr(^uint(0)>>1) {
		return nil, errors.New("domgobind: invalid byte view")
	}
	return append([]byte(nil), unsafe.Slice((*byte)(data), int(size))...), nil
}

// Boundary turns recoverable panics into status codes before returning to C.
// Runtime fatal errors and process termination cannot be recovered.
func Boundary(fn func() int) (status int) {
	status = Panic
	defer func() {
		if recover() != nil {
			status = Panic
		}
	}()
	return fn()
}
