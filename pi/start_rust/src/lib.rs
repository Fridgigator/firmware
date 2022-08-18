//! `start_rust` is the basic crate that passes control to the main library and catches panics, which is then also passed to the main library.

#![no_std]
#![warn(missing_docs)]

/// `run_rust_main` is the entry point to the rust code. The C scaffold sets up some basic code before
/// and hands off the rest to rust.
/// This function just passes on control to the main library's `main` function.

#[no_mangle]
pub extern "C" fn run_rust_main() {
    rust_lib::main();
}

#[panic_handler]
fn my_panic(_info: &core::panic::PanicInfo) -> ! {
    loop {
        rust_lib::panic();
    }
}
