#![feature(never_type)]
#![cfg_attr(not(test), no_std)]
#![feature(panic_info_message)]
extern crate core;

mod system;

/// This module contains functions that are only applicable in wasm mode and cannot be tested using rust's
/// testing framework.
#[cfg(not(test))]
mod setup;

use core::time::Duration;

extern crate alloc;

use prost::DecodeError;
use prost::Message;

/// wasm_main is the entry point to the module. It will not return.
#[cfg(not(test))]
#[no_mangle]
pub extern "C" fn wasm_main() {
    // Initialize the allocator BEFORE you use it

    unsafe {
        setup::init_allocator();
    }

    use crate::system::ESP32;
    // wasm_main is only called when run in wasm mode (as in, not in testing mode). Thus use the "real"
    // extern functions
    main(ESP32);
}

/// The real function. You must pass an FFI (either ESP32 or a mock FFI)
fn main<F: Ffi>(_: F) {
    // Message loop
    loop {
        // Try to get data from the websocket
        let mut buf = [0u8; 256];
        match F::get_websocket_data(&mut buf) {
            Ok(more) => {
                match get_packet_from_bytes(&buf) {
                    Ok(val) => F::print(
                        val.r#type
                            .map(|t| match t {
                                protobufs::backend_to_firmware_packet::Type::Ack(_) => "ack",
                                protobufs::backend_to_firmware_packet::Type::AddSensor(_) => {
                                    "add_sensor"
                                }
                                protobufs::backend_to_firmware_packet::Type::ClearSensorList(_) => {
                                    "clear_sensor_list"
                                }
                                protobufs::backend_to_firmware_packet::Type::GetSensorsList(_) => {
                                    "get_sensor_list"
                                }
                            })
                            .unwrap_or_default(),
                    ),
                    Err(_) => F::send_message(FFIMessage::GenericError),
                }

                if more && F::sleep(Duration::from_micros(100)).is_err() {
                    F::send_message(FFIMessage::TryFromIntError);
                    return;
                }
            }
            Err(t) => match t {
                ReadError::OutOfMemory => F::send_message(FFIMessage::TooMuchData),
            },
        }
    }
}

fn get_packet_from_bytes(buf: &[u8]) -> Result<protobufs::BackendToFirmwarePacket, DecodeError> {
    Message::decode(buf)
}

use crate::system::{FFIMessage, Ffi, ReadError};

pub mod protobufs {
    include!(concat!(env!("OUT_DIR"), "/fridgigator.firmware_backend.rs"));
}

#[cfg(test)]
mod test {
    use crate::get_packet_from_bytes;

    #[test]
    fn test_protobuf_fail() {
        let result = get_packet_from_bytes(&[0, 1, 2, 3, 4, 5]);
        if result.is_ok() {
            panic!("This shouldn't return a valid result")
        }
    }
}
