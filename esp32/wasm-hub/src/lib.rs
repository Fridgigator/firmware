#![feature(never_type)]
#![cfg_attr(not(test), no_std)]
#![feature(panic_info_message)]
// #![warn(missing_docs)]

//! The Fridgigator hub business logic.
//!
//! This code should be generic and work on any device that can support WebAssembly. The native code should initialize the wasm runtime and call main.
//! The rest of the logic sits in main's loop which should periodically yield the CPU.

extern crate alloc;
extern crate core;

/// contains the logical types representing the backend <-> firmware interaction
pub mod backend_to_firmware;

/// contains the logical types representing the firmware <-> firmware interaction
pub mod firmware_to_firmware;

/// contains constants that are used by the program
pub mod constants;

/// contains the business logic
pub mod controller;

/// contains internal libraries
pub mod libs;

/// contains the generated protobuf files
pub mod protobufs;

#[cfg(not(test))]
/// This module contains functions that are only applicable in wasm mode and cannot be tested using rust's
/// testing framework.
pub mod setup;

/// contains the functions to interact with the runtime and native code
pub mod system;

use crate::constants::MAX_DEVICES;
use crate::controller::{ack_server, connect_devices, find_devices, get_websocket_data};
use crate::libs::mutex::Mutex;
use crate::protobufs::{DevicesList, FirmwareToFirmwarePacket};
use crate::system::{FFIMessage, Ffi};
use cassette::pin_mut;
use cassette::Cassette;
use core::sync::atomic::AtomicBool;
use core::time::Duration;
use futures::future::join;
use heapless::{Deque, Vec};
use libs::sleep::Sleep;
use protobufs::firmware_to_backend_packet;
use protobufs::FirmwareToBackendPacket;
use protobufs::Ping;

use crate::alloc::string::ToString;
use crate::backend_to_firmware::{add_devices, Device};
use prost::DecodeError;
use prost::Message;

#[cfg(not(test))]
const ESP32_INIT: crate::system::ESP32 = crate::system::ESP32::new();
/// This is the entry point to the module. It will not return.
#[cfg(not(test))]
#[no_mangle]
pub extern "C" fn wasm_main() {
    // Initialize the allocator BEFORE you use it

    unsafe {
        setup::init_allocator();
    }

    // wasm_main is only called when run in wasm mode (as in, not in testing mode). Thus use the "real"
    // extern functions
    main(&ESP32_INIT);
}

/// The GPIO controlling the red LED
const RED_LED: u8 = 23;
/// The GPIO controlling the white LED
const WHITE_LED: u8 = 17;
/// The GPIO controlling the green LED
const GREEN_LED: u8 = 19;
/// The GPIO controlling the yellow LED
const YELLOW_LED: u8 = 18;

/// The real function. You must pass an [Ffi] (either ESP32 or a mock FFI)
fn main<F: Ffi>(esp32: &'static F) {
    /// This should be true when we should be getting the device list and false otherwise.
    static SHOULD_GET_DEVICE_LIST: AtomicBool = AtomicBool::new(false);
    const MAX_CONNECTED_DEVICES: usize = 4;
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
    // last time the registered_devices_list was updated
    let last_registered_devices_list_update = Mutex::new(esp32.get_time());
    // List of devices with their sensors
    let registered_devices_list: Mutex<heapless::Vec<Device, MAX_DEVICES>> =
        Mutex::new(heapless::Vec::new());

    // Every 60 seconds, send an ack to the server
    let ack_server_async = async {
        loop {
            ack_server(esp32);
            // sleep for 60 seconds
            // There's no need to use the result since it's always going to be [Ready::None]
            let _ = Sleep::new(esp32, Duration::from_secs(60), || (false, ())).await;
        }
    };

    // Get websocket data
    let get_websocket_data_async = async {
        loop {
            let mut temp_registered_devices_list = Vec::new();
            get_websocket_data(
                esp32,
                &mut temp_registered_devices_list,
                &SHOULD_GET_DEVICE_LIST,
            );
            // get current time and set it to the last updated time
            let got_time = esp32.get_time();
            {
                *last_registered_devices_list_update.lock(esp32).await = got_time;
            }
            // update the real device list. Lock and send out of scope ASAP
            {
                let mut registered_devices_list = registered_devices_list.lock(esp32).await;
                registered_devices_list.clear();
                for device in temp_registered_devices_list {
                    let _ = registered_devices_list.push(device);
                }
            }
            // Yield the scheduler for a bit (100ms)
            // There's no need to use the result since it's always going to be [Ready::None]
            let _ = Sleep::new(esp32, Duration::from_millis(100), || (false, ())).await;
        }
    };

    let find_devices_async = async {
        loop {
            if let Err(msg) = find_devices(esp32, &SHOULD_GET_DEVICE_LIST).await {
                esp32.send_message(msg);
                loop {}
                //panic!("Assertion error");
            }
            // We'll wait 5 seconds
            // There's no need to use the result since it's always going to be [Ready::None]
            let _ = Sleep::new(esp32, Duration::from_secs(5), || (false, ())).await;
        }
    };

    let connect_devices_async = async {
        let mut old_registered_devices_list: Deque<_, 64> = Deque::new();
        let registered_devices_list_tmp = { registered_devices_list.lock(esp32).await.clone() };
        // load data into old_registered_devices_list
        for device in registered_devices_list_tmp {
            if let Err(_) = old_registered_devices_list.push_back(device.clone()) {
                esp32.send_message(FFIMessage::RegDeviceOutOfRoomError);
                loop {}
            }
        }

        loop {
            let registered_devices_list = { registered_devices_list.lock(esp32).await.clone() };
            // if nothing changed
            if contains_all(&registered_devices_list, &old_registered_devices_list) {
                if let Err(msg) = connect_devices::<MAX_CONNECTED_DEVICES>(
                    esp32,
                    &mut old_registered_devices_list,
                ) {
                    esp32.send_message(msg);
                    panic!("Assertion error");
                }
                // push devices that were connected to back to the list
                for device in registered_devices_list {
                    let err = old_registered_devices_list.push_back(device);
                    if let Err(_) = err {
                        esp32.send_message(FFIMessage::RegDeviceOutOfRoomError);
                        panic!("Assertion error");
                    }
                }
                // get last update time
                // get current time and set it to the last updated time
                let got_time = { *last_registered_devices_list_update.lock(esp32).await };
                // Send a list of devices to all hubs that we're connected to
                for device in old_registered_devices_list.iter() {
                    if let Device::Hub { device_info } = device {
                        if let Err(_) = esp32.ble_send_packet(
                            device_info,
                            FirmwareToFirmwarePacket {
                                // (2^64)/2 = 2^63. 2^63 seconds is 292*10^11 years in the future. I think we'll be fine
                                timestamp: got_time.time_since_epoch.as_secs() as i64,
                                r#type: Some(
                                    protobufs::firmware_to_firmware_packet::Type::DevicesList(
                                        DevicesList {
                                            devices: old_registered_devices_list
                                                .iter()
                                                .map(|device| match device {
                                                    Device::Hub { device_info, .. } => {
                                                        protobufs::DeviceInfo {
                                                            address: i64::from_le_bytes(device_info.address),
                                                            name: alloc::string::String::from_utf8_lossy(&device_info.name).to_string(),
                                                            device_type: protobufs::DeviceType::Hub as i32 }
                                                    }
                                                    Device::Nordic { device_info, .. } => {
                                                        protobufs::DeviceInfo {
                                                            address: i64::from_le_bytes(device_info.address),
                                                            name: alloc::string::String::from_utf8_lossy(&device_info.name).to_string(),
                                                            device_type: protobufs::DeviceType::Nordic as i32 }
                                                    }
                                                    Device::Pico { device_info, .. } => {
                                                        protobufs::DeviceInfo {
                                                            address: i64::from_le_bytes(device_info.address),
                                                            name: alloc::string::String::from_utf8_lossy(&device_info.name).to_string(),
                                                            device_type: protobufs::DeviceType::Pico as i32 }
                                                    }
                                                    Device::TI { device_info, .. } => {
                                                        protobufs::DeviceInfo {
                                                            address: i64::from_le_bytes(device_info.address),
                                                            name: alloc::string::String::from_utf8_lossy(&device_info.name).to_string(),
                                                            device_type: protobufs::DeviceType::Ti as i32 }
                                                    }
                                                    Device::Unknown => todo!(),
                                                })
                                                .collect(),
                                        },
                                    ),
                                ),
                            },
                        ) {
                            esp32.send_message(FFIMessage::ProtobufEncodeError);
                            loop{}
                          //  panic!("Assertion error");
                        }
                    }
                }
                // We'll stay connected for 30 seconds, getting incoming data every second
                // There's no need to use the result since it's always going to be [Ready::None]
                let _ = Sleep::new(esp32, Duration::from_secs(30), || (false, ())).await;
            } else {
                // registered devices changed. We need to update the deque
                old_registered_devices_list.clear();
                for device in registered_devices_list.iter() {
                    if let Err(_) = old_registered_devices_list.push_back(device.clone()) {
                        esp32.send_message(FFIMessage::RegDeviceOutOfRoomError);
                        loop {}
                    }
                }
            }
        }
    };

    let websocket_data_ack_server = join(
        join(ack_server_async, get_websocket_data_async),
        join(find_devices_async, connect_devices_async),
    );
    pin_mut!(websocket_data_ack_server);
    let mut cm = Cassette::new(websocket_data_ack_server);

    loop {
        // Because all of our futures are should never return, this should never return a value.
        if cm.poll_on().is_some() {
            esp32.send_message(FFIMessage::AssertExitedFuture);
            return;
        }
    }
}

/// contains_all checks if a vec and a deque contain the same elements

fn contains_all<T: PartialEq>(
    vec: &heapless::Vec<T, MAX_DEVICES>,
    deque: &Deque<T, MAX_DEVICES>,
) -> bool {
    if vec.len() != deque.len() {
        return false;
    }
    for item in vec.iter() {
        let mut contains = false;
        for item2 in deque.iter() {
            if item == item2 {
                contains = true;
                break;
            }
        }
        if !contains {
            return false;
        }
    }
    true
}
/// This converts a buffer into a packet, if possible
fn get_packet_from_bytes(buf: &[u8]) -> Result<protobufs::BackendToFirmwarePacket, DecodeError> {
    Message::decode(buf)
}

#[cfg(test)]
mod test {
    use crate::backend_to_firmware::{add_devices, AddDeviceError, Device, DeviceInfo};
    use crate::constants::MAX_DEVICES;
    use crate::controller::connect_devices;
    use crate::get_packet_from_bytes;
    use crate::libs::time::Time;
    use crate::protobufs::backend_to_firmware_packet::Type;
    use crate::protobufs::DeviceInfo as protobuf_deviceinfo;
    use crate::protobufs::FirmwareToBackendPacket;
    use crate::protobufs::StopGetDevicesList;
    use crate::system::{FFIMessage, Ffi, ReadError};
    use alloc::vec::Vec;
    use core::num::TryFromIntError;
    use core::time::Duration;
    use prost::{EncodeError, Message};

    type WebsocketDataFunc = fn(&mut [u8]) -> Result<bool, ReadError>;

    #[derive(Debug, Clone)]
    pub(crate) struct Mock {
        pub(crate) print: Option<fn(&str)>,
        pub(crate) get_websocket_data: Option<WebsocketDataFunc>,
        pub(crate) send_message: Option<fn(FFIMessage)>,
        pub(crate) sleep: Option<fn(Duration) -> Result<(), TryFromIntError>>,
        pub(crate) get_time: Option<fn() -> Time>,
        pub(crate) set_led: Option<fn(u8, bool)>,
        pub(crate) start_remote_device_scan: Option<fn()>,
        pub(crate) stop_remote_device_scan: Option<fn()>,
        pub(crate) send_data:
            Option<fn(packet: FirmwareToBackendPacket) -> Result<(), EncodeError>>,
        pub(crate) get_remote_device_scan_result:
            Option<fn() -> Option<Result<DeviceInfo, alloc::string::FromUtf8Error>>>,
        pub(crate) connect_devices: Option<fn(device: &[u64])>,
        pub(crate) ble_send_devices_list: Option<
            fn(
                target: &DeviceInfo,
                list: crate::protobufs::FirmwareToFirmwarePacket,
            ) -> Result<(), EncodeError>,
        >,
    }

    impl Ffi for Mock {
        fn connect_devices(&self, devices: &[u64]) {
            self.connect_devices.unwrap()(devices)
        }
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
        fn get_time(&self) -> Time {
            self.get_time.unwrap()()
        }
        fn set_led(&self, which: u8, state: bool) {
            self.set_led.unwrap()(which, state);
        }

        fn start_remote_device_scan(&self) {
            self.start_remote_device_scan.unwrap()()
        }

        fn stop_remote_device_scan(&self) {
            self.stop_remote_device_scan.unwrap()()
        }

        fn get_remote_device_scan_result(
            &self,
        ) -> Option<Result<DeviceInfo, alloc::string::FromUtf8Error>> {
            self.get_remote_device_scan_result.unwrap()()
        }

        fn send_data(
            &self,
            packet: crate::protobufs::FirmwareToBackendPacket,
        ) -> Result<(), prost::EncodeError> {
            (self.send_data.unwrap())(packet)
        }

        fn ble_send_packet(
            &self,
            target: &DeviceInfo,
            list: crate::protobufs::FirmwareToFirmwarePacket,
        ) -> Result<(), EncodeError> {
            (self.ble_send_devices_list.unwrap())(target, list)
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
            timestamp: 0,
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
        let packet = crate::protobufs::BackendToFirmwarePacket {
            timestamp: 0,
            r#type: None,
        };
        Message::encode(&packet, &mut v).unwrap();
        let result = get_packet_from_bytes(&v);
        assert!(result.is_ok());
        assert_eq!(result.unwrap().r#type, None);
    }
    static MESSAGE_TO_SEND_GET_DATA: Mutex<Option<FFIMessage>> = Mutex::new(None);
    #[test]
    fn test_websocket_get_data() {
        let m = Mock {
            print: None,
            get_websocket_data: Some(|mut v| {
                let packet = crate::protobufs::BackendToFirmwarePacket {
                    timestamp: 0,
                    r#type: None,
                };
                Message::encode(&packet, &mut v).unwrap();
                Ok(false)
            }),
            send_message: Some(|message| {
                *MESSAGE_TO_SEND_GET_DATA.lock().unwrap() = Some(message);
            }),
            sleep: None,
            get_time: None,
            set_led: None,
            send_data: None,
            start_remote_device_scan: None,
            stop_remote_device_scan: None,
            get_remote_device_scan_result: None,
            connect_devices: None,
            ble_send_devices_list: None,
        };
        let mut v = Vec::new();
        assert!(!m.get_websocket_data(&mut v).unwrap());
        let result = get_packet_from_bytes(&v);
        assert!(result.is_ok());
        assert_eq!(result.unwrap().r#type, None);
        assert_eq!(*MESSAGE_TO_SEND_GET_DATA.lock().unwrap(), None);
    }
    static MESSAGE_TO_SEND_ADD_SENSOR_HAS_ROOM: Mutex<Option<FFIMessage>> = Mutex::new(None);

    #[test]
    fn test_add_sensor_has_room() {
        let m = Mock {
            print: None,
            get_websocket_data: None,
            send_message: Some(|message| {
                *MESSAGE_TO_SEND_ADD_SENSOR_HAS_ROOM.lock().unwrap() = Some(message);
            }),
            sleep: None,
            get_time: None,
            set_led: None,
            send_data: None,
            start_remote_device_scan: None,
            stop_remote_device_scan: None,
            get_remote_device_scan_result: None,
            connect_devices: None,
            ble_send_devices_list: None,
        };
        let mut sensors_list: heapless::Vec<Device, MAX_DEVICES> = heapless::Vec::new();
        add_devices(
            &mut sensors_list,
            &m,
            &[
                protobuf_deviceinfo {
                    name: "Name".into(),
                    address: 1,

                    device_type: 1,
                },
                protobuf_deviceinfo {
                    name: "Name".into(),
                    address: 2,

                    device_type: 1,
                },
                protobuf_deviceinfo {
                    name: "Name".into(),
                    address: 3,
                    device_type: 1,
                },
            ],
        )
        .unwrap();
        assert_eq!(sensors_list.len(), 3);
        assert_eq!(*MESSAGE_TO_SEND_ADD_SENSOR_HAS_ROOM.lock().unwrap(), None);
    }

    use std::sync::Mutex;

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
            start_remote_device_scan: None,
            stop_remote_device_scan: None,
            get_remote_device_scan_result: None,
            connect_devices: None,
            ble_send_devices_list: None,
        };
        let mut sensors_list: heapless::Vec<Device, MAX_DEVICES> = heapless::Vec::new();
        assert_eq!(
            add_devices(
                &mut sensors_list,
                &m,
                &[protobuf_deviceinfo {
                    name: "Name".into(),
                    address: 6.into(),

                    device_type: 1000,
                },],
            ),
            Err(AddDeviceError::WrongDeviceType)
        );

        assert_eq!(sensors_list.len(), 0)
    }

    #[test]
    fn connect_devices_test() {
        static MUTEX: Mutex<Option<FFIMessage>> = Mutex::new(None);
        static GETTING_DEVICE_LIST: Mutex<Vec<u64>> = Mutex::new(vec![]);

        let m = Box::new(Mock {
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
            start_remote_device_scan: None,
            stop_remote_device_scan: None,
            get_remote_device_scan_result: None,
            connect_devices: Some(|msg| {
                assert!(msg.len() <= 2);
                let mut device_list = GETTING_DEVICE_LIST.lock().unwrap();
                device_list.clear();
                for val in msg {
                    device_list.push(*val);
                }
            }),
            ble_send_devices_list: None,
        });
        let mut p: heapless::Deque<Device, MAX_DEVICES> = heapless::Deque::new();
        let mut name = heapless::Vec::<u8, 29>::new();
        name.push(b'N').unwrap();
        name.push(b'a').unwrap();
        name.push(b'm').unwrap();
        name.push(b'e').unwrap();
        p.push_back(Device::Hub {
            device_info: DeviceInfo {
                address: 1000_u64.to_le_bytes(),
                name,
            },
        })
        .unwrap();
        connect_devices::<2>(std::boxed::Box::<Mock>::leak(m), &mut p).unwrap();
        assert_eq!(GETTING_DEVICE_LIST.lock().unwrap()[0], 1000_u64);
    }

    #[test]
    fn connect_devices_test_four_at_a_time() {
        static MUTEX: Mutex<Option<FFIMessage>> = Mutex::new(None);
        static GETTING_DEVICE_LIST: Mutex<Vec<u64>> = Mutex::new(vec![]);

        let m = Box::new(Mock {
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
            start_remote_device_scan: None,
            stop_remote_device_scan: None,
            get_remote_device_scan_result: None,
            connect_devices: Some(|msg| {
                assert!(msg.len() <= 2);
                let mut device_list = GETTING_DEVICE_LIST.lock().unwrap();
                device_list.clear();
                for val in msg {
                    device_list.push(*val);
                }
            }),
            ble_send_devices_list: None,
        });
        let mut p: heapless::Deque<Device, MAX_DEVICES> = heapless::Deque::new();
        let mut name = heapless::Vec::<u8, 29>::new();
        name.push(b'N').unwrap();
        name.push(b'a').unwrap();
        name.push(b'm').unwrap();
        name.push(b'e').unwrap();
        p.push_back(Device::Hub {
            device_info: DeviceInfo {
                address: 1000_u64.to_le_bytes(),
                name: name.clone(),
            },
        })
        .unwrap();
        p.push_back(Device::Hub {
            device_info: DeviceInfo {
                address: 1001_u64.to_le_bytes(),
                name: name.clone(),
            },
        })
        .unwrap();
        p.push_back(Device::Hub {
            device_info: DeviceInfo {
                address: 1002_u64.to_le_bytes(),
                name: name.clone(),
            },
        })
        .unwrap();
        p.push_back(Device::Hub {
            device_info: DeviceInfo {
                address: 1003_u64.to_le_bytes(),
                name: name.clone(),
            },
        })
        .unwrap();
        eprintln!("p_orig: {p:?}");
        connect_devices::<2>(std::boxed::Box::<Mock>::leak(m.clone()), &mut p).unwrap();
        assert_eq!(GETTING_DEVICE_LIST.lock().unwrap()[0], 1000_u64);
        assert_eq!(GETTING_DEVICE_LIST.lock().unwrap()[1], 1001_u64);
        eprintln!("p_after_1: {p:?}");
        connect_devices::<2>(std::boxed::Box::<Mock>::leak(m), &mut p).unwrap();
        eprintln!("p after 2: {p:?}");
        assert_eq!(GETTING_DEVICE_LIST.lock().unwrap()[0], 1002_u64);
        assert_eq!(GETTING_DEVICE_LIST.lock().unwrap()[1], 1003_u64);
    }

    #[test]
    fn connect_unknown_devices_test() {
        static MUTEX: Mutex<Option<FFIMessage>> = Mutex::new(None);

        let m = Box::new(Mock {
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
            start_remote_device_scan: None,
            stop_remote_device_scan: None,
            get_remote_device_scan_result: None,
            connect_devices: Some(|msg| {
                assert!(msg.len() < 4);
            }),
            ble_send_devices_list: None,
        });
        let mut p = heapless::Deque::new();
        p.push_back(Device::Unknown).unwrap();
        assert_eq!(
            connect_devices::<2>(std::boxed::Box::<Mock>::leak(m), &mut p),
            Err(FFIMessage::UsingUnknownDevice)
        );
    }
}
