//! Borrowed C ABI views and panic containment. Never free a C buffer with Rust's allocator.
use std::ffi::c_void;

#[repr(C)]
#[derive(Clone, Copy)]
pub struct ByteView {
    pub data: *const u8,
    pub size: usize,
}

impl ByteView {
    /// The view borrows `bytes`; the caller must not retain it beyond that slice.
    pub fn from_slice(bytes: &[u8]) -> Self {
        Self {
            data: bytes.as_ptr(),
            size: bytes.len(),
        }
    }

    /// # Safety
    /// A nonempty view must point to readable memory for `size` bytes in a single
    /// allocation and remain valid and immutable for the chosen lifetime.
    pub unsafe fn as_slice<'a>(self) -> Result<&'a [u8], Status> {
        if self.size == 0 {
            return Ok(&[]);
        }
        if self.data.is_null() || self.size > isize::MAX as usize {
            return Err(Status::Invalid);
        }
        Ok(unsafe { std::slice::from_raw_parts(self.data, self.size) })
    }
}

#[repr(i32)]
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum Status {
    Ok = 0,
    Invalid = -1,
    Panic = -2,
}

/// Catch unwinding panics before an extern "C" boundary (requires panic=unwind).
pub fn boundary(f: impl FnOnce() -> Result<(), Status>) -> i32 {
    match std::panic::catch_unwind(std::panic::AssertUnwindSafe(f)) {
        Ok(Ok(())) => Status::Ok as i32,
        Ok(Err(status)) => status as i32,
        Err(_) => Status::Panic as i32,
    }
}

#[repr(C)]
#[derive(Clone, Copy)]
pub struct Callback {
    pub function: Option<unsafe extern "C" fn(*mut c_void, *const u8, usize) -> i32>,
    pub context: *mut c_void,
}

impl Callback {
    /// # Safety
    /// The function and context must be live, callable on this thread, and must
    /// neither unwind nor retain the borrowed slice after returning.
    pub unsafe fn invoke(self, bytes: &[u8]) -> i32 {
        match self.function {
            Some(f) => unsafe { f(self.context, bytes.as_ptr(), bytes.len()) },
            None => Status::Invalid as i32,
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    #[test]
    fn binary_and_empty_views() {
        let bytes = b"a\0b";
        assert_eq!(
            unsafe { ByteView::from_slice(bytes).as_slice() }.unwrap(),
            bytes
        );
        assert_eq!(
            unsafe {
                ByteView {
                    data: std::ptr::null(),
                    size: 0,
                }
                .as_slice()
            }
            .unwrap(),
            b""
        );
        assert_eq!(
            unsafe {
                ByteView {
                    data: std::ptr::null(),
                    size: 1,
                }
                .as_slice()
            },
            Err(Status::Invalid)
        );
    }
    #[test]
    fn panics_do_not_cross_boundary() {
        assert_eq!(boundary(|| panic!("test panic")), Status::Panic as i32);
        assert_eq!(boundary(|| Ok(())), 0);
    }
}
