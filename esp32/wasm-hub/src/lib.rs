#![feature(never_type)]
#![cfg_attr(not(test), no_std)]
#![feature(panic_info_message)]
extern crate core;

mod system;

/// This module contains functions that are only applicable in wasm mode and cannot be tested using rust's
/// testing framework.
#[cfg(not(test))]
mod setup;

use crate::system::{FFIMessage, Ffi, ReadError};
use alloc::vec::Vec;
use core::time;
use time::Duration;

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
    main(ESP32::new());
}

/// The real function. You must pass an FFI (either ESP32 or a mock FFI)
fn main<F: Ffi>(esp32: F) {
    // Flash LEDs on bootup
    for _ in 0..4 {
        esp32.set_led(18, true);
        esp32.set_led(19, true);
        esp32.set_led(20, true);
        esp32.set_led(21, true);
        // I'm passing a constant so it should never error
        if esp32.sleep(Duration::from_millis(500)).is_err() {
            esp32.send_message(FFIMessage::TryFromIntError);
            return;
        };
        esp32.set_led(18, false);
        esp32.set_led(19, false);
        esp32.set_led(20, false);
        esp32.set_led(21, false);
        // I'm passing a constant so it should never error
        if esp32.sleep(Duration::from_millis(500)).is_err() {
            esp32.send_message(FFIMessage::TryFromIntError);
            return;
        };
    }

    // Timer to ack to the server
    let mut next_timestamp = 0;
    // Message loop
    loop {
        // Every 60 seconds, send an ack to the server
        let cur_time = esp32.get_time();
        if cur_time > next_timestamp {
            // write ack
            let mut buf = Vec::new();

            protobufs::firmware_to_backend_packet::Type::Ping(protobufs::Ping::default())
                .encode(&mut buf);
            // set next time to call
            next_timestamp = cur_time + 60;
        }
        // Try to get data from the websocket
        let mut buf = [0u8; 256];
        match esp32.get_websocket_data(&mut buf) {
            // If all of the data fit into the buffer
            Ok(more) => {
                match get_packet_from_bytes(&buf) {
                    Ok(val) => esp32.print(
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
                    Err(_) => esp32.send_message(FFIMessage::GenericError),
                }
                // If there's more data waiting, don't sleep. If there is and Duration::from_micros(100)
                // is too large to fit into a 128 bit integer, "panic"
                if more && esp32.sleep(Duration::from_micros(100)).is_err() {
                    esp32.send_message(FFIMessage::TryFromIntError);
                    return;
                }
            }
            Err(t) => match t {
                ReadError::OutOfMemory => esp32.send_message(FFIMessage::TooMuchData),
            },
        }
    }
}

fn get_packet_from_bytes(buf: &[u8]) -> Result<protobufs::BackendToFirmwarePacket, DecodeError> {
    Message::decode(buf)
}

pub mod protobufs {
    include!(concat!(env!("OUT_DIR"), "/fridgigator.firmware_backend.rs"));
}

#[cfg(test)]
mod test {
    use crate::get_packet_from_bytes;
    use crate::protobufs::backend_to_firmware_packet::Type;
    use crate::protobufs::ClearSensorList;
    use crate::system::{FFIMessage, Ffi, ReadError};
    use alloc::vec::Vec;
    use core::num::TryFromIntError;
    use core::time::Duration;
    use prost::Message;

    struct Mock {
        test_call: Option<fn()>,
        print: Option<fn(&str)>,
        get: Option<fn(&mut [u8])>,
        get_websocket_data: Option<fn(&mut [u8]) -> Result<bool, ReadError>>,
        send_message: Option<fn(FFIMessage)>,
        sleep: Option<fn(Duration) -> Result<(), TryFromIntError>>,
        get_time: Option<fn() -> u64>,
        set_led: Option<fn(u8, bool)>,
    }

    impl Ffi for Mock {
        fn test_call(&self) {
            self.test_call.unwrap()();
        }

        fn print(&self, text: &str) {
            self.print.unwrap()(text);
        }

        fn get(&self, buf: &mut [u8]) {
            self.get.unwrap()(buf);
        }

        fn sleep(&self, t: Duration) -> Result<(), TryFromIntError> {
            self.sleep.unwrap()(t)
        }

        fn get_websocket_data(&self, buf: &mut [u8]) -> Result<bool, ReadError> {
            self.get_websocket_data.unwrap()(buf)
        }

        fn send_message(&self, msg: FFIMessage) {
            self.send_message.unwrap()(msg)
        }
        fn get_time(&self) -> u64 {
            self.get_time.unwrap()()
        }
        fn set_led(&self, which: u8, state: bool) {
            self.set_led.unwrap()(which, state);
        }
    }

    #[test]
    fn test_protobuf_fail() {
        let result = get_packet_from_bytes(&[0, 1, 2, 3, 4, 5]);
        if result.is_ok() {
            panic!("This shouldn't return a valid result")
        }
    }

    #[test]
    fn test_protobuf_succeed() {
        let mut v = Vec::new();
        let packet = crate::protobufs::BackendToFirmwarePacket {
            r#type: Some(Type::ClearSensorList(ClearSensorList::default())),
        };
        Message::encode(&packet, &mut v).unwrap();
        let result = get_packet_from_bytes(&v);
        assert!(result.is_ok());
        assert_eq!(
            result.unwrap().r#type,
            Some(Type::ClearSensorList(ClearSensorList::default()))
        );
    }

    #[test]
    fn test_protobuf_succeed_val_set_none() {
        let mut v = Vec::new();
        let packet = crate::protobufs::BackendToFirmwarePacket { r#type: None };
        Message::encode(&packet, &mut v).unwrap();
        let result = get_packet_from_bytes(&v);
        assert!(result.is_ok());
        assert_eq!(result.unwrap().r#type, None);
    }

    #[test]
    fn test_websocket_get_data() {
        let m = Mock {
            test_call: None,
            print: None,
            get: None,
            get_websocket_data: Some(|mut v| {
                let packet = crate::protobufs::BackendToFirmwarePacket { r#type: None };
                Message::encode(&packet, &mut v).unwrap();
                Ok(false)
            }),
            send_message: None,
            sleep: None,
            get_time: None,
            set_led: None,
        };
        let mut v = Vec::new();
        assert!(!m.get_websocket_data(&mut v).unwrap());
        let result = get_packet_from_bytes(&v);
        assert!(result.is_ok());
        assert_eq!(result.unwrap().r#type, None);
    }
}
