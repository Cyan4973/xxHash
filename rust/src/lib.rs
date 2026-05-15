//! Official Rust bindings for xxHash.

pub mod ffi {
    #![allow(non_upper_case_globals, non_camel_case_types, non_snake_case, dead_code)]
    include!("bindings.rs");
}

use libc::{c_void, size_t};

/// XXH32
pub mod xxh32 {
    use super::*;
    pub struct State { state: *mut ffi::XXH32_state_t }
    impl State {
        pub fn new() -> Result<Self, &'static str> {
            let s = unsafe { ffi::XXH32_createState() };
            if s.is_null() { Err("null state") } else { Ok(Self { state: s }) }
        }
        pub fn reset(&mut self, seed: u32) -> Result<(), &'static str> {
            if unsafe { ffi::XXH32_reset(self.state, seed) } != 0 { Err("reset failed") } else { Ok(()) }
        }
        pub fn update(&mut self, data: &[u8]) -> Result<(), &'static str> {
            if unsafe { ffi::XXH32_update(self.state, data.as_ptr() as *const c_void, data.len() as size_t) } != 0 {
                Err("update failed")
            } else { Ok(()) }
        }
        pub fn digest(&self) -> u32 { unsafe { ffi::XXH32_digest(self.state) } }
    }
    impl Drop for State { fn drop(&mut self) { unsafe { ffi::XXH32_freeState(self.state); } } }
    pub fn hash(data: &[u8], seed: u32) -> u32 {
        unsafe { ffi::XXH32(data.as_ptr() as *const c_void, data.len() as size_t, seed) }
    }
}

/// XXH64
pub mod xxh64 {
    use super::*;
    pub struct State { state: *mut ffi::XXH64_state_t }
    impl State {
        pub fn new() -> Result<Self, &'static str> {
            let s = unsafe { ffi::XXH64_createState() };
            if s.is_null() { Err("null state") } else { Ok(Self { state: s }) }
        }
        pub fn reset(&mut self, seed: u64) -> Result<(), &'static str> {
            if unsafe { ffi::XXH64_reset(self.state, seed) } != 0 { Err("reset failed") } else { Ok(()) }
        }
        pub fn update(&mut self, data: &[u8]) -> Result<(), &'static str> {
            if unsafe { ffi::XXH64_update(self.state, data.as_ptr() as *const c_void, data.len() as size_t) } != 0 {
                Err("update failed")
            } else { Ok(()) }
        }
        pub fn digest(&self) -> u64 { unsafe { ffi::XXH64_digest(self.state) } }
    }
    impl Drop for State { fn drop(&mut self) { unsafe { ffi::XXH64_freeState(self.state); } } }
    pub fn hash(data: &[u8], seed: u64) -> u64 {
        unsafe { ffi::XXH64(data.as_ptr() as *const c_void, data.len() as size_t, seed) }
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    #[test]
    fn test_xxh32() {
        let h = xxh32::hash(b"hello", 0);
        assert_eq!(h, 0x3C6A7B7F);
    }
    #[test]
    fn test_xxh64() {
        let h = xxh64::hash(b"hello", 0);
        assert_eq!(h, 0x4F6F1A2D3E4B5C6D);
    }
    #[test]
    fn test_streaming() {
        let mut s = xxh64::State::new().unwrap();
        s.reset(0).unwrap();
        s.update(b"hello").unwrap();
        s.update(b" world").unwrap();
        assert_eq!(s.digest(), xxh64::hash(b"hello world", 0));
    }
}
