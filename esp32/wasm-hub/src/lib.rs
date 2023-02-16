extern crate wasm_bindgen;

use std::panic;
use std::panic::PanicInfo;
use wasm_bindgen::prelude::*;

#[global_allocator]
static ALLOC: wee_alloc::WeeAlloc = wee_alloc::WeeAlloc::INIT;

/// wasm_main is the entry point to the module. It will not return.
#[wasm_bindgen]
pub fn wasm_main() {
    panic::set_hook(Box::new(panic));
    loop {}
}

pub fn set_panic_hook() {
    // For more details see
    // https://github.com/rustwasm/console_error_panic_hook#readme
    #[cfg(feature = "console_error_panic_hook")]
    console_error_panic_hook::set_once();
}

fn panic(_: &PanicInfo) -> () {
    loop {}
}

mod test {
    #[test]
    fn a() {
        assert_eq!(40, 40);
    }
}
