use super::sys::{
    sys_get_device_from_scan, sys_get_time, sys_get_websocket_data, sys_print, sys_send_message,
    sys_set_led, sys_sleep, sys_start_remote_device_scan, sys_stop_remote_device_scan, sys_send_websocket_data,
};
use crate::backend_to_firmware::{Device, DeviceType};
use crate::protobufs::FirmwareToBackendPacket;
use alloc::string::String;
use alloc::{string::FromUtf8Error, vec::Vec};
use core::num::TryFromIntError;
use core::time::Duration;
use prost::{EncodeError, Message};

/// Allows passing quick and small messages through the FFI
#[derive(Debug, Clone)]
pub enum FFIMessage {
    TooMuchData,
    GenericError,
    TryFromIntError,
    TooManySensors,
    AssertWrongModelType,
    #[allow(dead_code)]
    PanicErr,
    FromUtf8Error,
    EncodeError,
}

/// Signifies the type of reading error coming in from FFI
#[derive(Debug)]
pub enum ReadError {
    OutOfMemory,
}

/// The FFI trait allows for Dependency Injection and better testing
pub trait Ffi {
    fn print(&self, text: &str);

    /// sleep tells the OS to pause the WASM thread for the duration. Returns an error if the duration
    /// is more than 584,554 years (u64 in microseconds)
    fn sleep(&self, t: Duration) -> Result<(), TryFromIntError>;

    /// This accepts a buffer to write to. It returns true if more data is available, false
    /// otherwise.
    fn get_websocket_data(&self, buf: &mut [u8]) -> Result<bool, ReadError>;

    /// This allows passing quick and small messages defined in `FFIMessage` through the FFI
    fn send_message(&self, msg: FFIMessage);

    fn get_time(&self) -> u64;

    fn set_led(&self, which: u8, state: bool);

    fn start_remote_device_scan(&self);
    fn get_remote_device_scan_result(&self) -> Option<Result<Device, FromUtf8Error>>;
    fn stop_remote_device_scan(&self);
    fn send_data(&self, packet: FirmwareToBackendPacket) -> Result<(), EncodeError>;
}

/// The ESP32 device is passed when in production or in integration testing
#[derive(Default)]
pub struct ESP32 {
    sys_send_message: Option<unsafe fn(msg: i64)>,
    sys_get_websocket_data: Option<unsafe fn(data: *mut u8, size: usize) -> u8>,
    sys_sleep: Option<unsafe fn(micros: u64)>,
    sys_print: Option<unsafe fn(address: *const u8, size: usize)>,
    sys_get_time: Option<unsafe fn() -> u64>,
    sys_set_led: Option<unsafe fn(which: u8, state: bool)>,
}

impl ESP32 {
    #[must_use]
    pub fn new() -> Self {
        Self {
            sys_print: Some(|msg, size| unsafe { sys_print(msg, size) }),
            sys_get_websocket_data: Some(|addr, size| unsafe {
                sys_get_websocket_data(addr, size)
            }),
            sys_send_message: Some(|msg| unsafe { sys_send_message(msg) }),
            sys_sleep: Some(|micros| unsafe { sys_sleep(micros) }),
            sys_get_time: Some(|| unsafe { sys_get_time() }),
            sys_set_led: Some(|state, which| unsafe { sys_set_led(which, state) }),
        }
    }
}

impl Ffi for ESP32 {
    fn print(&self, text: &str) {
        unsafe {
            (self.sys_print.unwrap())(text.as_ptr(), text.bytes().len());
        }
    }
    fn sleep(&self, t: Duration) -> Result<(), TryFromIntError> {
        unsafe {
            (self.sys_sleep.unwrap())(t.as_micros().try_into()?);
            Ok(())
        }
    }

    fn get_websocket_data(&self, buf: &mut [u8]) -> Result<bool, ReadError> {
        let res = unsafe { (self.sys_get_websocket_data.unwrap())(buf.as_mut_ptr(), buf.len()) };
        if res & 1 == 1 {
            Ok(true)
        } else if (res >> 1) & 1 == 1 {
            Err(ReadError::OutOfMemory)
        } else {
            Ok(false)
        }
    }

    fn send_message(&self, msg: FFIMessage) {
        let msg = match msg {
            FFIMessage::GenericError => 0,
            FFIMessage::TooMuchData => 1,
            FFIMessage::TryFromIntError => 2,
            FFIMessage::PanicErr => 3,
            FFIMessage::TooManySensors => 4,
            FFIMessage::AssertWrongModelType => 5,
            FFIMessage::FromUtf8Error => 6,
            FFIMessage::EncodeError => 7,
            
        };
        unsafe {
            (self.sys_send_message.unwrap())(msg);
        };
    }

    fn get_time(&self) -> u64 {
        unsafe { (self.sys_get_time.unwrap())() }
    }

    fn set_led(&self, which: u8, state: bool) {
        unsafe {
            (self.sys_set_led.unwrap())(which, state);
        }
    }

    fn start_remote_device_scan(&self) {
        unsafe {
            sys_start_remote_device_scan();
        }
    }

    fn get_remote_device_scan_result(&self) -> Option<Result<Device, FromUtf8Error>> {
        let mut address = [0; 8];
        // The max length of a ble device name is 29 chars: https://stackoverflow.com/questions/65568893/how-to-know-the-maximum-length-of-bt-name
        let mut name_vec = Vec::with_capacity(29);
        if unsafe {
            sys_get_device_from_scan(
                address.as_mut_ptr(),
                name_vec.as_mut_ptr(),
                name_vec.capacity(),
            )
        } == 0
        {
            match String::from_utf8(name_vec) {
                Ok(string) => {
                    return Some(Ok(Device::new(address, string, DeviceType::Unknown)));
                }
                Err(err) => {
                    return Some(Err(err));
                }
            };
        }
        None
    }

    fn stop_remote_device_scan(&self) {
        unsafe {
            sys_stop_remote_device_scan();
        }
    }

    fn send_data(&self, packet: FirmwareToBackendPacket) -> Result<(), EncodeError> {
        // write ack
        let mut buf = Vec::new();

        packet.encode(&mut buf)?;
        unsafe {
            sys_send_websocket_data(buf.as_ptr(), buf.len());
        }
        Ok(())
    }
}

#[cfg(test)]
mod test {
    use crate::system::{Ffi, ESP32};
    use alloc::vec::Vec;

    #[test]
    fn test_print() {
        let mut e = ESP32::default();
        e.sys_print = Some(|msg, size| {
            let s = "abcABC\n123😀";
            for i in 0..size {
                assert_eq!(unsafe { *msg.add(i) }, s.as_bytes()[i]);
            }
        });
        e.print("abcABC\n123😀");
    }

    #[test]
    fn test_get_websocket_data_works() {
        let mut e = ESP32::default();
        e.sys_get_websocket_data = Some(|_, _| 0);
        let mut v = Vec::with_capacity(1024);
        assert!(!e.get_websocket_data(&mut v).unwrap());
    }

    #[test]
    fn test_get_websocket_data_server_has_more_data_points() {
        let mut e = ESP32::default();
        e.sys_get_websocket_data = Some(|_, _| 0b1);
        let mut v = Vec::with_capacity(1024);
        assert!(e.get_websocket_data(&mut v).unwrap());
    }

    #[test]
    fn test_get_websocket_data_server_data_entry_too_large() {
        let mut e = ESP32::default();
        e.sys_get_websocket_data = Some(|_, _| 0b10);
        let mut v = Vec::with_capacity(1024);
        assert!(e.get_websocket_data(&mut v).is_err());
    }
}
