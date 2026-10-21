use domrustbind::{boundary, ByteView, Status};

/// # Safety
/// `data` must address `size` readable bytes; `out` must address a writable u64.
#[no_mangle]
pub unsafe extern "C" fn domrust_sum(data: *const u8, size: usize, out: *mut u64) -> i32 {
    boundary(|| {
        if out.is_null() {
            return Err(Status::Invalid);
        }
        let bytes = unsafe { ByteView { data, size }.as_slice()? };
        let result = bytes.iter().map(|b| u64::from(*b)).sum();
        unsafe {
            *out = result;
        }
        Ok(())
    })
}
fn main() {}
