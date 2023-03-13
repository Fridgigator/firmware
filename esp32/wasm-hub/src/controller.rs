use core::time::Duration;

use crate::backend_to_firmware::AddDeviceError;
use crate::constants::MAX_FOUND_DEVICES;
use crate::firmware_to_backend_packet;
use crate::libs::sleep::Sleep;
use crate::protobufs::firmware_to_backend_packet::Type;
use crate::system::{FFIMessage, Ffi, ReadError};
use crate::Device;
use crate::FirmwareToBackendPacket;
use crate::{add_devices, MAX_DEVICES};
use crate::{get_packet_from_bytes, protobufs, Ping, SHOULD_GET_DEVICE_LIST};
use heapless::Vec;

pub(crate) fn ack_server(esp32: &impl Ffi) {
    if esp32
        .send_data(FirmwareToBackendPacket {
            r#type: Some(firmware_to_backend_packet::Type::Ping(Ping::default())),
        })
        .is_err()
    {
        esp32.send_message(FFIMessage::ProtobufEncodeError);
    }
}

/// finds devices. It waits 15 seconds to scan for devices unless it's signalled outherwise.

pub(crate) async fn find_devices(esp32: &'static impl Ffi) -> Result<(), FFIMessage> {
    // SeqCst is the safest and guarantees order
    if SHOULD_GET_DEVICE_LIST.load(core::sync::atomic::Ordering::SeqCst) {
        esp32.start_remote_device_scan();
        // When we search for devices, we hold it here. We need to keep it in RAM to prevent double searching.
        // We can index by id.
        let mut found_devices: Vec<[u8; 8], MAX_FOUND_DEVICES> = Vec::new();
        let start_time = esp32.get_time();
        // Look for data if it's less than 15 seconds since start_time, if SHOULD_GET_DEVICE_LIST is true, and if we have room.
        while (esp32.get_time() - start_time) <= Duration::from_secs(15)
            && SHOULD_GET_DEVICE_LIST.load(core::sync::atomic::Ordering::SeqCst)
            && found_devices.len() < MAX_FOUND_DEVICES
        {
            if let Some(scan_result) = esp32.get_remote_device_scan_result() {
                // The name was a utf8 string
                if let Ok(scan_result) = scan_result {
                    // if this device isn't in the remote_devices list yet
                    if !found_devices.contains(&scan_result.address) {
                        // Add it to the list so we don't double find. If doesn't fit,
                        if found_devices.push(scan_result.address).is_err() {
                            // stop the device scan
                            esp32.stop_remote_device_scan();
                            // Make sure that we won't automatically start searching again
                            SHOULD_GET_DEVICE_LIST
                                .store(false, core::sync::atomic::Ordering::SeqCst);
                            // and exit
                            return Err(FFIMessage::TooManyDevices);
                        }
                        // Convert what we found into a protobuf encodable struct. If it can be turned into a utf-8 string, go ahead. Otherwise, send a
                        // log message to the server
                        if let Some(val) = scan_result.into() {
                            // We need to send the data that we found
                            let found_device_packet = FirmwareToBackendPacket {
                                r#type: Some(Type::DeviceInfo(val)),
                            };
                            if esp32.send_data(found_device_packet).is_err() {
                                esp32.send_message(FFIMessage::ProtobufEncodeError);
                            };
                        } else {
                            esp32.send_message(FFIMessage::BLENameUTF8Error);
                        };
                    }
                } else {
                    esp32.send_message(FFIMessage::BLENameUTF8Error);
                }
            }
            // Yield the scheduler for a bit
            // There's no need to use the result since it's always going to be [Ready::None]
            let _ = Sleep::new(esp32, Duration::from_millis(100), || (false, ()) ).await;
        }
        // We're done. Now we have to clean up
        esp32.stop_remote_device_scan();
        // Make sure that we won't automatically start searching again
        SHOULD_GET_DEVICE_LIST.store(false, core::sync::atomic::Ordering::SeqCst);
    }
    Ok(())
}

pub(crate) fn get_websocket_data(esp32: &impl Ffi, devices_list: &mut Vec<Device, MAX_DEVICES>) {
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
                                if let Err(err) = add_devices(devices_list, esp32, &devices.devices)
                                {
                                    match err {
                                        AddDeviceError::TooManyDevices => {
                                            esp32.send_message(FFIMessage::TooManyDevices);
                                        }
                                        AddDeviceError::UnableToConvert => {
                                            esp32.send_message(FFIMessage::UnableToConvert);
                                        }
                                    }
                                }
                            }
                            protobufs::backend_to_firmware_packet::Type::StopGetDevicesList(_) => {
                                SHOULD_GET_DEVICE_LIST
                                    .store(false, core::sync::atomic::Ordering::SeqCst);
                            }
                            protobufs::backend_to_firmware_packet::Type::GetDevicesList(_) => {
                                SHOULD_GET_DEVICE_LIST
                                    .store(true, core::sync::atomic::Ordering::SeqCst);
                            }
                        };
                    };
                }
                Err(_) => esp32.send_message(FFIMessage::ProtobufDecodeError),
            }
            // If there's more data waiting, don't sleep. If there is and Duration::from_micros(100)
            // is too large to fit into a 128 bit integer, "panic"
            if more && esp32.sleep(Duration::from_micros(100)).is_err() {
                esp32.send_message(FFIMessage::TryFromIntError);
            }
        }
        Err(t) => match t {
            ReadError::OutOfMemory => esp32.send_message(FFIMessage::TooMuchData),
        },
    }
}

#[cfg(test)]
mod test {
    use core::time::Duration;
    use std::sync::Mutex;

    use cassette::pin_mut;
    use cassette::Cassette;

    use crate::backend_to_firmware::Device;
    use crate::backend_to_firmware::DeviceType;
    use crate::libs::time::Time;
    use crate::protobufs::DeviceInfo;
    use crate::test::Mock;
    use crate::FirmwareToBackendPacket;
    use crate::SHOULD_GET_DEVICE_LIST;

    use super::ack_server;
    use super::find_devices;
    use crate::Ping;

    use crate::firmware_to_backend_packet;

    #[test]
    fn test_ack_server() {
        let m = Mock {
            print: None,
            get_websocket_data: None,
            send_message: None,
            sleep: None,
            get_time: None,
            set_led: None,
            send_data: Some(|packet: FirmwareToBackendPacket| {
                if packet
                    != (FirmwareToBackendPacket {
                        r#type: Some(firmware_to_backend_packet::Type::Ping(Ping::default())),
                    })
                {
                    panic!("Wrong type: {:?}", packet);
                }
                Ok(())
            }),
            start_remote_device_scan: None,
            stop_remote_device_scan: None,
            get_remote_device_scan_result: None,
        };
        ack_server(&m);
    }

    static FIND_DEVICES_TIMESTAMP: Mutex<u64> = Mutex::new(0);
    static FIND_DEVICES_START_CALLED: Mutex<u64> = Mutex::new(0);
    static FIND_DEVICES_STOP_CALLED: Mutex<u64> = Mutex::new(0);
    #[test]
    fn test_find_devices() {
        let m = Box::new(Mock {
            print: None,
            get_websocket_data: None,
            send_message: None,
            sleep: None,
            get_time: Some(|| {
                let mut time = FIND_DEVICES_TIMESTAMP.lock().unwrap();
                let old_time = *time;
                *time += 1;
                Time::new(Duration::from_secs(old_time))
            }),
            set_led: None,
            send_data: Some(|packet: FirmwareToBackendPacket| {
                let mut info = DeviceInfo::default();
                info.address = 6618611909121;
                info.name = String::from("abcdefgh");
                info.device_type = DeviceType::Nordic.into();
                if packet
                    != (FirmwareToBackendPacket {
                        r#type: Some(firmware_to_backend_packet::Type::DeviceInfo(info)),
                    })
                {
                    panic!("Wrong type: {:?}", packet);
                }
                Ok(())
            }),
            start_remote_device_scan: Some(|| {
                *FIND_DEVICES_START_CALLED.lock().unwrap() += 1;
            }),
            stop_remote_device_scan: Some(|| {
                *FIND_DEVICES_STOP_CALLED.lock().unwrap() += 1;
            }),
            get_remote_device_scan_result: Some(|| {
                Some(Ok(Device {
                    address: [1, 2, 3, 4, 5, 6, 0, 0],
                    name: {
                        let mut v: heapless::Vec<u8, 29> = heapless::Vec::new();
                        "abcdefgh".bytes().for_each(|x| {
                            v.push(x).unwrap();
                        });
                        v
                    },
                    device_type: crate::backend_to_firmware::DeviceType::Nordic,
                }))
            }),
        });
        SHOULD_GET_DEVICE_LIST.store(true, core::sync::atomic::Ordering::SeqCst);
        let p = find_devices(Box::<Mock>::leak(m));
        pin_mut!(p);
        let mut cm = Cassette::new(p);
        loop {
            if let Some(_) = cm.poll_on() {
                assert!(*FIND_DEVICES_TIMESTAMP.lock().unwrap() > 15);
                assert_eq!(*FIND_DEVICES_START_CALLED.lock().unwrap(), 1);
                assert_eq!(*FIND_DEVICES_STOP_CALLED.lock().unwrap(), 1);
                return;
            }
        }
    }
}
