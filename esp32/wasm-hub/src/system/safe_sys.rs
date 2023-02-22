use crate::system::sys::{
    sys_get, sys_get_websocket_data, sys_print, sys_send_message, sys_sleep, sys_test_call,
};
use core::num::TryFromIntError;
use core::time::Duration;

/// FFIMessage allows passing quick and small messages through the FFI
pub enum FFIMessage {
    TooMuchData,
    GenericError,
    TryFromIntError,
    #[allow(dead_code)]
    PanicErr,
}

/// ReadError signifies the type of reading error coming in from FFI
pub enum ReadError {
    OutOfMemory,
}

/// The FFI trait allows for Dependency Injection and better testing
pub trait Ffi {
    fn test_call();
    fn print(text: &str);
    fn get(buf: &mut [u8]);

    /// sleep tells the OS to pause the WASM thread for the duration. Returns an error if the duration
    /// is more than 584,554 years (u64 in microseconds)
    fn sleep(t: Duration) -> Result<(), TryFromIntError>;

    /// get_websocket_data accepts a buffer to write to. It returns true if more data is available, false
    /// otherwise.
    fn get_websocket_data(buf: &mut [u8]) -> Result<bool, ReadError>;

    /// FFIMessage allows passing quick and small messages defined in [FFIMessage] through the FFI
    fn send_message(msg: FFIMessage);
}

/// The ESP32 device is passed when in production or in integration testing
pub struct ESP32;

impl Ffi for ESP32 {
    fn test_call() {
        unsafe { sys_test_call() }
    }
    fn print(text: &str) {
        unsafe {
            sys_print(text.as_ptr(), text.bytes().len());
        }
    }
    fn get(buf: &mut [u8]) {
        unsafe {
            sys_get(buf.as_mut_ptr(), buf.len());
        }
    }
    fn sleep(t: Duration) -> Result<(), TryFromIntError> {
        unsafe {
            sys_sleep(t.as_micros().try_into()?);
            Ok(())
        }
    }

    fn get_websocket_data(buf: &mut [u8]) -> Result<bool, ReadError> {
        let res = unsafe { sys_get_websocket_data(buf.as_mut_ptr(), buf.len()) };
        if res & 1 == 1 {
            Ok(true)
        } else if (res >> 1) & 1 == 1 {
            Err(ReadError::OutOfMemory)
        } else {
            Ok(false)
        }
    }

    fn send_message(msg: FFIMessage) {
        let msg = match msg {
            FFIMessage::GenericError => 0,
            FFIMessage::TooMuchData => 1,
            FFIMessage::TryFromIntError => 2,
            FFIMessage::PanicErr => 3,
        };
        unsafe {
            sys_send_message(msg);
        };
    }
}
