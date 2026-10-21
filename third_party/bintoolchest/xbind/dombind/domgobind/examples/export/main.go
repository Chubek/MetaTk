package main

/*
#include <stdint.h>
#include <stddef.h>
*/
import "C"
import (
	"domweave/domgobind"
	"unsafe"
)

//export domgo_sum
func domgo_sum(data *C.uchar, size C.size_t, out *C.uint64_t) C.int {
	return C.int(domgobind.Boundary(func() int {
		if out == nil {
			return domgobind.Invalid
		}
		bytes, err := domgobind.CopyBytes(unsafe.Pointer(data), uintptr(size))
		if err != nil {
			return domgobind.Invalid
		}
		var sum uint64
		for _, b := range bytes {
			sum += uint64(b)
		}
		*out = C.uint64_t(sum)
		return domgobind.OK
	}))
}
func main() {}
