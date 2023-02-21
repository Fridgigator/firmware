#![feature(never_type)]
#![cfg_attr(not(test), no_std)]

#![feature(panic_info_message)]
extern crate core;

use core::panic::PanicInfo;

use core::time::Duration;

use core::num::TryFromIntError;
// We cannot use LockedHeap allocator in unit tests because there's no way to initialize it before use, as
// its first use is in main.
#[cfg(not(test))]
use linked_list_allocator::LockedHeap;
extern crate alloc;
use alloc::vec::Vec;

#[cfg(test)]
const HEAP_SIZE: usize = 1024 * 1024;

#[cfg(not(test))]
const HEAP_SIZE: usize = 1024 * 512;

#[cfg(not(test))]
static mut HEAP_START: [u8; HEAP_SIZE] = [0; HEAP_SIZE];


#[cfg(not(test))]
mod sys;

#[cfg(not(test))]
use crate::sys::sys_sleep;
#[cfg(not(test))]
use crate::sys::sys_print;
#[cfg(not(test))]
use crate::sys::sys_test_call;
#[cfg(not(test))]
use crate::sys::sys_get;

fn get(buf: &mut [u8]) -> Result<(), TryFromIntError> {
    unsafe {
        sys_get(buf.as_mut_ptr(), buf.len().try_into()?);
    }
    Ok(())
}

#[cfg(test)]
fn sys_sleep(ms: u32) {
    use std::thread;
    thread::sleep(Duration::from_millis(100 as u64));
}

#[cfg(test)]
fn sys_get_time() -> u64 {
    use std::time::SystemTime;
    SystemTime::now().duration_since(SystemTime::UNIX_EPOCH).unwrap().as_secs()
}

#[cfg(test)]
fn sys_print(address: *const u8, size: usize) {
    use std::ffi::c_char;
    for c in 0..size {
        print!("{}", unsafe { *address.add(c)});
    }
}

#[cfg(test)]
fn sys_get(address: *const u8, size: u32) {}

#[cfg(test)]
fn sys_test_call() {}

pub fn test_call() {
    unsafe {
        sys_test_call()
    }
}

pub fn print(text: &str) {
    unsafe {
        sys_print(text.as_ptr(), text.bytes().len());
    }
}


#[cfg(not(test))]
#[global_allocator]
static ALLOCATOR: LockedHeap = LockedHeap::empty();

/// wasm_main is the entry point to the module. It will not return.
#[no_mangle]
pub extern "C" fn wasm_main() {
    // Initialize the allocator BEFORE you use it
    #[cfg(not(test))]
    unsafe {
        ALLOCATOR.lock().init(HEAP_START.as_mut_ptr(), HEAP_SIZE);
    }
    main();
}

fn sleep(t: Duration) -> Result<(), TryFromIntError> {
    unsafe {
        sys_sleep(t.as_micros().try_into()?);
        Ok(())
    }
}

fn main() {
    loop {
        print("Ping\n");
        let _ = sleep(Duration::from_secs(5));
        let mut buf = Vec::with_capacity(1024);
        get(&mut buf).unwrap();
        if let Ok(utf8) = core::str::from_utf8(&buf) {
            print(utf8);
            print("\n");
        } else {
            print("Unable to print\n");
        }
    }
}

#[cfg(not(debug_assertions))]
#[panic_handler]
fn panic(info: &PanicInfo) -> ! {
    print("Panic! ");
    loop {}
}

#[cfg(not(test))]
#[cfg(debug_assertions)]
#[panic_handler]
fn panic(info: &PanicInfo) -> ! {
    print("Panic! ");

    if let Some(msg) = info.message() {
        if let Some(msg) = msg.as_str() {
            print(msg);
        }
    }
    if let Some(location) = info.location() {
        use numtoa::NumToA;
        print(" In file ");
        print(location.file());
        print(" : Line ");
        let mut buf = [0u8; 11];
        location.line().numtoa_str(10, &mut buf);
        print(core::str::from_utf8(&buf).unwrap_or("bl"));
        print(":");
        location.column().numtoa_str(10, &mut buf);
        print(core::str::from_utf8(&buf).unwrap_or("bc"));
    }
    print("\n");
    loop {}
}

#[cfg(test)]
mod test {
    use std::time::Duration;

    #[test]
    fn a() {}
}