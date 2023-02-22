// We cannot use LockedHeap allocator in unit tests because there's no way to initialize it before use, as
// its first use is in main.
use crate::system::{FFIMessage, Ffi};
use linked_list_allocator::LockedHeap;

const HEAP_SIZE: usize = 1024 * 512;
static mut HEAP_START: [u8; HEAP_SIZE] = [0; HEAP_SIZE];

#[global_allocator]
static ALLOCATOR: LockedHeap = LockedHeap::empty();

pub unsafe fn init_allocator() {
    ALLOCATOR.lock().init(HEAP_START.as_mut_ptr(), HEAP_SIZE);
}

#[cfg(not(debug_assertions))]
#[panic_handler]
fn panic(_: &core::panic::PanicInfo) -> ! {
    use crate::system::ESP32;
    let esp32 = ESP32::new();
    esp32.send_message(FFIMessage::PanicErr);
    loop {}
}

#[cfg(debug_assertions)]
#[panic_handler]
fn panic(info: &core::panic::PanicInfo) -> ! {
    use crate::system::ESP32;
    let esp32 = ESP32::new();
    esp32.print("Panic! ");

    if let Some(msg) = info.message() {
        if let Some(msg) = msg.as_str() {
            esp32.print(msg);
        }
    }
    if let Some(location) = info.location() {
        use numtoa::NumToA;
        esp32.print(" In file ");
        esp32.print(location.file());
        esp32.print(" : Line ");
        let mut buf = [0u8; 11];
        location.line().numtoa_str(10, &mut buf);
        esp32.print(core::str::from_utf8(&buf).unwrap_or("bl"));
        esp32.print(":");
        location.column().numtoa_str(10, &mut buf);
        esp32.print(core::str::from_utf8(&buf).unwrap_or("bc"));
    }
    esp32.print("\n");
    esp32.send_message(FFIMessage::PanicErr);
    loop {}
}
