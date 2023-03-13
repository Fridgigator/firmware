use super::sys::{
    sys_get_device_from_scan, sys_get_time, sys_get_websocket_data, sys_print, sys_send_message,
    sys_send_websocket_data, sys_set_led, sys_sleep, sys_start_remote_device_scan,
    sys_stop_remote_device_scan,
};
use crate::backend_to_firmware::{Device, DeviceType};
use crate::constants::MAX_BLE_SIZE_IN_BYTES;
use crate::libs::time::Time;
use crate::protobufs::FirmwareToBackendPacket;
use alloc::string::String;
use alloc::{string::FromUtf8Error, vec::Vec};
use core::num::TryFromIntError;
use core::time::Duration;
use prost::{EncodeError, Message};

/// Allows passing quick and small messages through the FFI
#[derive(Debug, Clone, PartialEq, Eq, PartialOrd, Ord)]
pub enum FFIMessage {
    /// The websocket data doesn't fit in the buffer
    TooMuchData,
    /// The Protobuf can't be decoded or encoded
    ProtobufDecodeError,
    /// Int conversion fails because it's out of bounds 
    TryFromIntError,
    /// The server sent us too many sensors
    TooManyDevices,
    /// Device sent does not fit requirements
    UnableToConvert,
    #[allow(dead_code)]
    /// There was a panic in our code
    PanicErr,
    /// The Protobuf can't beencoded
    ProtobufEncodeError,
    /// Futures should not finish. We get this message if they did
    AssertExitedFuture,
    /// A device name was not legal utf8
    BLENameUTF8Error,
    /// Time is too far in the future to be properly represented
    TimeOverflow,
}

/// Signifies the type of reading error coming in from FFI
#[derive(Debug)]
pub enum ReadError {
    /// The packet sent from the server doesn't fit in the buffer
    OutOfMemory,
}

/// The FFI trait allows for Dependency Injection and better testing
pub trait Ffi {

    /// Prints text to the screen or to the uart debug console
    fn print(&self, text: &str);

    /// Tells the OS to pause the WASM thread for the duration. Returns an error if the duration
    /// is more than 584,554 years (u64 in microseconds)
    fn sleep(&self, t: Duration) -> Result<(), TryFromIntError>;

    /// This accepts a buffer to write to. It returns true if more data is available, false
    /// otherwise.
    fn get_websocket_data(&self, buf: &mut [u8]) -> Result<bool, ReadError>;

    /// This allows passing quick and small messages defined in `FFIMessage` through the FFI
    fn send_message(&self, msg: FFIMessage);

    /// gets time from the device. It's always assumed to be in UTC.
    fn get_time(&self) -> Time;

    /// Sets an LED to be on or off
    fn set_led(&self, which: u8, state: bool);

    /// Starts the device scan (this is used when looking for a new device)
    fn start_remote_device_scan(&self);
    
    /// This is called when we are scanning for results. It will return a device, if found.
    fn get_remote_device_scan_result(&self) -> Option<Result<Device, FromUtf8Error>>;

    /// Stop scanning for devices. A device scan can be caused by a timeout, by the user selecting a device or by the user cancelling a scan
    fn stop_remote_device_scan(&self);

    /// Send data via websocket to the server
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
    /// Create a new esp32 device. This can be created as a global constant variable.
    #[must_use]
    pub const fn new() -> Self {
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
            FFIMessage::ProtobufDecodeError => 0,
            FFIMessage::TooMuchData => 1,
            FFIMessage::TryFromIntError => 2,
            FFIMessage::PanicErr => 3,
            FFIMessage::TooManyDevices => 4,
            FFIMessage::UnableToConvert => 5,
            FFIMessage::ProtobufEncodeError => 6,
            FFIMessage::AssertExitedFuture => 7,
            FFIMessage::BLENameUTF8Error => 8,
            FFIMessage::TimeOverflow => 9,
        };
        unsafe {
            (self.sys_send_message.unwrap())(msg);
        };
    }

    fn get_time(&self) -> Time {
        let time_in_ns_since_utc = unsafe { (self.sys_get_time.unwrap())() };
        Time::new(Duration::from_nanos(time_in_ns_since_utc))
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
        let mut name_vec = [0u8; MAX_BLE_SIZE_IN_BYTES];
        if unsafe {
            sys_get_device_from_scan(address.as_mut_ptr(), name_vec.as_mut_ptr(), name_vec.len())
        } == 0
        {
            match String::from_utf8(name_vec.to_vec()) {
                Ok(_) => {
                    return Some(Ok(Device::new(
                        address,
                        name_vec.iter().copied().collect(),
                        DeviceType::Unknown,
                    )));
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
