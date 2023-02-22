use crate::system::sys::{
    sys_get, sys_get_time, sys_get_websocket_data, sys_print, sys_send_message,
    sys_sleep, sys_test_call,
};
use core::num::TryFromIntError;
use core::time::Duration;

/// FFIMessage allows passing quick and small messages through the FFI
#[derive(Debug)]
pub enum FFIMessage {
    TooMuchData,
    GenericError,
    TryFromIntError,
    #[allow(dead_code)]
    PanicErr,
}

/// ReadError signifies the type of reading error coming in from FFI
#[derive(Debug)]
pub enum ReadError {
    OutOfMemory,
}

/// The FFI trait allows for Dependency Injection and better testing
pub trait Ffi {
    fn test_call(&self);
    fn print(&self, text: &str);
    fn get(&self, buf: &mut [u8]);

    /// sleep tells the OS to pause the WASM thread for the duration. Returns an error if the duration
    /// is more than 584,554 years (u64 in microseconds)
    fn sleep(&self, t: Duration) -> Result<(), TryFromIntError>;

    /// get_websocket_data accepts a buffer to write to. It returns true if more data is available, false
    /// otherwise.
    fn get_websocket_data(&self, buf: &mut [u8]) -> Result<bool, ReadError>;

    /// FFIMessage allows passing quick and small messages defined in [FFIMessage] through the FFI
    fn send_message(&self, msg: FFIMessage);

    fn get_time(&self) -> u64;
}

/// The ESP32 device is passed when in production or in integration testing
pub struct ESP32 {
    sys_send_message: Option<unsafe fn(msg: i64)>,
    sys_get_websocket_data: Option<unsafe fn(data: *mut u8, size: usize) -> u8>,
    sys_sleep: Option<unsafe fn(micros: u64)>,
    sys_print: Option<unsafe fn(address: *const u8, size: usize)>,
    sys_get: Option<unsafe fn(address: *mut u8, size: usize)>,
    sys_test_call: Option<unsafe fn()>,
    sys_get_time: Option<unsafe fn() -> u64>,
}

impl ESP32 {
    pub fn new() -> Self {
        Self {
            sys_test_call: Some(|| unsafe { sys_test_call() }),
            sys_print: Some(|msg, size| unsafe { sys_print(msg, size) }),
            sys_get: Some(|addr, size| unsafe { sys_get(addr, size) }),
            sys_get_websocket_data: Some(|addr, size| unsafe { sys_get_websocket_data(addr, size) }),
            sys_send_message: Some(|msg| unsafe { sys_send_message(msg) }),
            sys_sleep: Some(|micros| unsafe { sys_sleep(micros) }),
            sys_get_time: Some(|| unsafe { sys_get_time() }),
        }
    }

}

impl Ffi for ESP32 {
    fn test_call(&self) {
        unsafe { self.sys_test_call.unwrap()() }
    }
    fn print(&self, text: &str) {
        unsafe {
            (self.sys_print.unwrap())(text.as_ptr(), text.bytes().len());
        }
    }
    fn get(&self, buf: &mut [u8]) {
        unsafe {
            (self.sys_get.unwrap())(buf.as_mut_ptr(), buf.len());
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
        };
        unsafe {
            (self.sys_send_message.unwrap())(msg);
        };
    }

    fn get_time(&self) -> u64 {
        unsafe {
            (self.sys_get_time.unwrap())()
        }
    }
}

#[cfg(test)]
mod test {
    use alloc::vec::Vec;
    use crate::system::{ESP32, Ffi};
    use std::sync::Mutex;

    impl ESP32 {
        pub fn new_without_call() -> Self {
            Self {
                sys_send_message: None,
                sys_get_websocket_data: None,
                sys_sleep: None,
                sys_print: None,
                sys_get: None,
                sys_test_call: None,
                sys_get_time: None,
            }
        }
    }
    static TEST_TEST_CALL_MUTEX: Mutex<bool> = Mutex::new(false);
    #[test]
    fn test_test_call() {
        *TEST_TEST_CALL_MUTEX.lock().unwrap() = false;
        let mut e = ESP32::new_without_call(

        );
        e.sys_test_call = Some(|| {
            let mut l = TEST_TEST_CALL_MUTEX.lock().unwrap();
            *l = true;
        });
        e.test_call();
        assert!(*TEST_TEST_CALL_MUTEX.lock().unwrap())
    }
    #[test]
    fn test_print() {
        let mut e = ESP32::new_without_call(        );
        e.sys_print = Some(|msg, size| {
            let s = "abcABC\n123😀";
            for i in 0..size {
                assert_eq!(
                    unsafe {*msg.add(i)}, s.as_bytes()[i]);
            }
        });
        e.print("abcABC\n123😀");
    }

    #[test]
    fn test_get_websocket_data_works() {
        let mut e = ESP32::new_without_call();
        e.sys_get_websocket_data = Some(|_, _| {
            0
        });
        let mut v = Vec::with_capacity(1024);
        assert!(!e.get_websocket_data(&mut v).unwrap());
    }

    #[test]
    fn test_get_websocket_data_server_has_more_data_points() {
        let mut e = ESP32::new_without_call();
        e.sys_get_websocket_data = Some(|_, _| {
            0b1
        });
        let mut v = Vec::with_capacity(1024);
        assert!(e.get_websocket_data(&mut v).unwrap());
    }

    #[test]
    fn test_get_websocket_data_server_data_entry_too_large() {
        let mut e = ESP32::new_without_call();
        e.sys_get_websocket_data = Some(|_, _| {
            0b10
        });
        let mut v = Vec::with_capacity(1024);
        assert!(e.get_websocket_data(&mut v).is_err());
    }
}
