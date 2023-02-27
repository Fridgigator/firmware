#![feature(never_type)]
#![cfg_attr(not(test), no_std)]
#![feature(panic_info_message)]
extern crate core;

pub mod system;

/// This module contains functions that are only applicable in wasm mode and cannot be tested using rust's
/// testing framework.
#[cfg(not(test))]
pub mod setup;

pub mod backend_to_firmware;
use crate::protobufs::DeviceInfo;
use crate::protobufs::DeviceType::*;
use crate::system::{FFIMessage, Ffi, ReadError};
use alloc::vec::Vec;
use prost::EncodeError;
use protobufs::FirmwareToBackendPacket;
use crate::protobufs::DevicesList;
use protobufs::firmware_to_backend_packet;
use core::time::Duration;

extern crate alloc;

use crate::backend_to_firmware::{add_devices, AddSensorsEnum, Device};
use prost::DecodeError;
use prost::Message;

/// This is the entry point to the module. It will not return.
#[cfg(not(test))]
#[no_mangle]
pub extern "C" fn wasm_main() {
    // Initialize the allocator BEFORE you use it
    use crate::system::ESP32;

    unsafe {
        setup::init_allocator();
    }

    // wasm_main is only called when run in wasm mode (as in, not in testing mode). Thus use the "real"
    // extern functions
    main(&ESP32::new());
}

/// The GPIO controlling the red LED
const RED_LED: u8 = 23;
/// The GPIO controlling the white LED
const WHITE_LED: u8 = 17;
/// The GPIO controlling the green LED
const GREEN_LED: u8 = 19;
/// The GPIO controlling the yellow LED
const YELLOW_LED: u8 = 18;
/// Maximum amount of devices that we can hold
const MAX_DEVICES: usize = 64;

/// Maximum amount of found devices that we can hold
const MAX_FOUND_DEVICES: usize = 64;

/// The real function. You must pass an [Ffi] (either ESP32 or a mock FFI)
fn main<F: Ffi>(esp32: &F) {
    // Flash LEDs on bootup
    for _ in 0..4 {
        esp32.set_led(RED_LED, true);
        esp32.set_led(WHITE_LED, true);
        esp32.set_led(GREEN_LED, true);
        esp32.set_led(YELLOW_LED, true);
        // I'm passing a constant so it should never error
        if esp32.sleep(Duration::from_millis(500)).is_err() {
            esp32.send_message(FFIMessage::TryFromIntError);
            return;
        };
        esp32.set_led(RED_LED, false);
        esp32.set_led(WHITE_LED, false);
        esp32.set_led(GREEN_LED, false);
        esp32.set_led(YELLOW_LED, false);
        // I'm passing a constant so it should never error
        if esp32.sleep(Duration::from_millis(500)).is_err() {
            esp32.send_message(FFIMessage::TryFromIntError);
            return;
        };
        esp32.stop_remote_device_scan();
    }
    // List of sensors
    let mut devices_list: Vec<Device> = Vec::with_capacity(MAX_DEVICES);
    // Timer to ack to the server
    let mut next_timestamp = 0;
    // Timer to stop getting sensors
    let mut stop_get_remote_devices: Option<u64> = None;
    // When we search for devices, we hold it here. We need to keep it in RAM to prevent double searching
    let mut remote_devices: Vec<Device> = Vec::new();
    // Message loop
    loop {
        let cur_time = esp32.get_time();
        // If we should stop getting sensors
        if let Some(stop_get_remote_devices_local) = stop_get_remote_devices {
            if stop_get_remote_devices_local > cur_time {
                // Stop getting sensors
                esp32.stop_remote_device_scan();
                stop_get_remote_devices = None;
                if send_data(esp32,&mut remote_devices).is_err() {
                    esp32.send_message(FFIMessage::EncodeError);
                }
            }
        }
        // If looking for sensors, read them at every cycle (if available) and add them to a set that will be sent to the server
        if stop_get_remote_devices.is_some() {
            if let Some(scan_result) = esp32.get_remote_device_scan_result() {
                // The name was a utf8 string
                if let Ok(scan_result) = scan_result {
                    // if this device isn't in the remote_devices list yet
                    if !remote_devices.contains(&scan_result) {
                        if remote_devices.len() < MAX_FOUND_DEVICES {
                            remote_devices.push(scan_result);
                        } else {
                            // If we found the most devices that we can hold
                            esp32.stop_remote_device_scan();
                            stop_get_remote_devices = None;
                            if send_data(esp32,&mut remote_devices).is_err() {
                                esp32.send_message(FFIMessage::EncodeError);
                            }
                        }
                    }
                } else {
                    esp32.send_message(FFIMessage::FromUtf8Error)
                }
            }
        }
        // Every 60 seconds, send an ack to the server
        if cur_time > next_timestamp {
            if esp32.send_data(FirmwareToBackendPacket { r#type:  None }).is_err() {
                esp32.send_message(FFIMessage::EncodeError);
            }
            // set next time to call
            next_timestamp = cur_time + 60;
        }
        // Try to get data from the websocket
        let mut buf = [0u8; 256];
        match esp32.get_websocket_data(&mut buf) {
            // If all of the data fit into the buffer
            Ok(more) => {
                match get_packet_from_bytes(&buf) {
                    Ok(val) => {
                        if let Some(t) = val.r#type {
                            match t {
                                protobufs::backend_to_firmware_packet::Type::Devices(devices) => {
                                    if let Err(err) =
                                        add_devices(&mut devices_list, esp32, &devices.devices)
                                    {
                                        match err {
                                            AddSensorsEnum::TooManySensors => {
                                                esp32.send_message(FFIMessage::TooManySensors);
                                            }
                                            AddSensorsEnum::UnableToConvert => {
                                                esp32
                                                    .send_message(FFIMessage::AssertWrongModelType);
                                            }
                                        }
                                    }
                                }
                                protobufs::backend_to_firmware_packet::Type::StopGetDevicesList(
                                    _,
                                ) => {
                                    esp32.stop_remote_device_scan();
                                    stop_get_remote_devices = None;
                                    if send_data(esp32,&mut remote_devices).is_err() {
                                        esp32.send_message(FFIMessage::EncodeError);
                                    }
                                }
                                protobufs::backend_to_firmware_packet::Type::GetDevicesList(_) => {
                                    // Stop getting sensors in 15 seconds
                                    esp32.start_remote_device_scan();
                                    stop_get_remote_devices = Some(cur_time + 15);
                                }
                            };
                        };
                    }
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

fn send_data<F: Ffi>(esp32: &F, remote_devices: &mut Vec<Device>) -> Result<(), EncodeError> {
    esp32.send_data(FirmwareToBackendPacket { r#type: Some(firmware_to_backend_packet::Type::DevicesList(
        DevicesList {
            devices: remote_devices.iter().map(|x|{
                DeviceInfo{
                    address: i64::from_le_bytes(x.address),
                    name: x.name.clone(),
                    device_type: match x.device_type {
                        backend_to_firmware::DeviceType::Unknown => Unspecified.into(),
                        backend_to_firmware::DeviceType::TI => Ti.into(),
                        backend_to_firmware::DeviceType::Nordic => Nordic.into(),
                        backend_to_firmware::DeviceType::Pico => Custom.into(),
                        backend_to_firmware::DeviceType::Hub => Hub.into(),
                    }
                }
            }).collect()
        }
    )) })?;
    remote_devices.clear();
    Ok(())
}

fn get_packet_from_bytes(buf: &[u8]) -> Result<protobufs::BackendToFirmwarePacket, DecodeError> {
    Message::decode(buf)
}

pub mod protobufs {
    include!(concat!(env!("OUT_DIR"), "/fridgigator.firmware_backend.rs"));
}

#[cfg(test)]
mod test {
    use crate::backend_to_firmware::{add_devices, AddSensorsEnum, Device};
    use crate::get_packet_from_bytes;
    use crate::protobufs::backend_to_firmware_packet::Type;
    use crate::protobufs::{DeviceInfo, FirmwareToBackendPacket};
    use crate::protobufs::StopGetDevicesList;
    use crate::system::{FFIMessage, Ffi, ReadError};
    use alloc::vec::Vec;
    use core::num::TryFromIntError;
    use core::time::Duration;
    use prost::{Message, EncodeError};

    type WebsocketDataFunc = fn(&mut [u8]) -> Result<bool, ReadError>;

    struct Mock {
        print: Option<fn(&str)>,
        get_websocket_data: Option<WebsocketDataFunc>,
        send_message: Option<fn(FFIMessage)>,
        sleep: Option<fn(Duration) -> Result<(), TryFromIntError>>,
        get_time: Option<fn() -> u64>,
        set_led: Option<fn(u8, bool)>,
        send_data: Option<fn (packet: FirmwareToBackendPacket) -> Result<(), EncodeError>>,
    }

    impl Ffi for Mock {
        fn print(&self, text: &str) {
            self.print.unwrap()(text);
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

        fn start_remote_device_scan(&self) {}

        fn get_remote_device_scan_result(
            &self,
        ) -> Option<Result<Device, alloc::string::FromUtf8Error>> {
            None
        }

        fn stop_remote_device_scan(&self) {}

        fn send_data(&self, packet: crate::protobufs::FirmwareToBackendPacket) -> Result<(), prost::EncodeError> {
        todo!()
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
            r#type: Some(Type::StopGetDevicesList(StopGetDevicesList::default())),
        };
        Message::encode(&packet, &mut v).unwrap();
        let result = get_packet_from_bytes(&v);
        assert!(result.is_ok());
        assert_eq!(
            result.unwrap().r#type,
            Some(Type::StopGetDevicesList(StopGetDevicesList::default()))
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
            print: None,
            get_websocket_data: Some(|mut v| {
                let packet = crate::protobufs::BackendToFirmwarePacket { r#type: None };
                Message::encode(&packet, &mut v).unwrap();
                Ok(false)
            }),
            send_message: None,
            sleep: None,
            get_time: None,
            set_led: None,
            send_data: None,
        };
        let mut v = Vec::new();
        assert!(!m.get_websocket_data(&mut v).unwrap());
        let result = get_packet_from_bytes(&v);
        assert!(result.is_ok());
        assert_eq!(result.unwrap().r#type, None);
    }

    #[test]
    fn test_add_sensor_has_room() {
        let m = Mock {
            print: None,
            get_websocket_data: None,
            send_message: None,
            sleep: None,
            get_time: None,
            set_led: None,
            send_data: None,
        };
        let mut sensors_list: Vec<Device> = Vec::with_capacity(4);
        add_devices(
            &mut sensors_list,
            &m,
            &[
                DeviceInfo {
                    name: "Name".into(),
                    address: 1,

                    device_type: 1,
                },
                DeviceInfo {
                    name: "Name".into(),
                    address: 2,

                    device_type: 1,
                },
                DeviceInfo {
                    name: "Name".into(),
                    address: 3,
                    device_type: 1,
                },
            ],
        )
        .unwrap();
        assert_eq!(sensors_list.len(), 3)
    }

    #[test]
    fn test_add_sensor_has_duplicates_with_same_address_so_has_room() {
        let m = Mock {
            print: None,
            get_websocket_data: None,
            send_message: None,
            sleep: None,
            get_time: None,
            set_led: None,
            send_data: None,
        };
        let mut sensors_list: Vec<Device> = Vec::with_capacity(2);
        add_devices(
            &mut sensors_list,
            &m,
            &[
                DeviceInfo {
                    name: "Name".into(),
                    address: 1,

                    device_type: 1,
                },
                DeviceInfo {
                    name: "Name".into(),
                    address: 2,

                    device_type: 1,
                },
                DeviceInfo {
                    name: "Name".into(),
                    address: 1,

                    device_type: 1,
                },
            ],
        )
        .unwrap();
        assert_eq!(sensors_list.len(), 2)
    }

    use std::sync::Mutex;

    #[test]
    fn test_add_sensor_out_of_room() {
        static MUTEX: Mutex<Option<FFIMessage>> = Mutex::new(None);

        let m = Mock {
            print: None,
            get_websocket_data: None,
            send_message: Some(|msg| {
                let mut tmp = MUTEX.lock().unwrap();
                *tmp = Some(msg);
            }),
            sleep: None,
            get_time: None,
            send_data: None,
            set_led: None,
        };
        let mut sensors_list: Vec<Device> = Vec::with_capacity(1);
        assert_eq!(
            add_devices(
                &mut sensors_list,
                &m,
                &[
                    DeviceInfo {
                        name: "Name".into(),
                        address: 6,

                        device_type: 1,
                    },
                    DeviceInfo {
                        name: "Name".into(),
                        address: 7,

                        device_type: 1,
                    },
                    DeviceInfo {
                        name: "Name".into(),
                        address: 8,

                        device_type: 1,
                    },
                    DeviceInfo {
                        name: "Name".into(),
                        address: 9,

                        device_type: 1,
                    },
                ],
            ),
            Err(AddSensorsEnum::TooManySensors)
        );

        assert_eq!(sensors_list.len(), 1)
    }

    #[test]
    fn test_add_sensor_wrong_device_types() {
        static MUTEX: Mutex<Option<FFIMessage>> = Mutex::new(None);

        let m = Mock {
            print: None,
            get_websocket_data: None,
            send_message: Some(|msg| {
                let mut tmp = MUTEX.lock().unwrap();
                *tmp = Some(msg);
            }),
            sleep: None,
            get_time: None,
            set_led: None,
            send_data: None,
        };
        let mut sensors_list: Vec<Device> = Vec::with_capacity(1);
        assert_eq!(
            add_devices(
                &mut sensors_list,
                &m,
                &[DeviceInfo {
                    name: "Name".into(),
                    address: 6.into(),

                    device_type: 1000,
                },],
            ),
            Err(AddSensorsEnum::UnableToConvert)
        );

        assert_eq!(sensors_list.len(), 0)
    }
}
