use core::sync::atomic::AtomicBool;
use core::time::Duration;

use crate::backend_to_firmware::Device;
use crate::constants::MAX_FOUND_DEVICES;
use crate::firmware_to_backend_packet;
use crate::libs::sleep::Sleep;
use crate::protobufs::firmware_to_backend_packet::Type;
use crate::system::{FFIMessage, Ffi, ReadError};
use crate::FirmwareToBackendPacket;
use crate::{add_devices, MAX_DEVICES};
use crate::{get_packet_from_bytes, protobufs, Ping};
use heapless::{Deque, Vec};
use static_assertions::const_assert;

/// Responds to the server's ping
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

/// connects to devices. It connects to as many devices as possible.
pub(crate) fn connect_devices<const MAX_CONNECTED_DEVICES: usize>(
    esp32: &impl Ffi,
    devices: &mut Deque<Device, MAX_DEVICES>,
) -> Result<(), FFIMessage> {
    const_assert!(MAX_DEVICES <= (u16::MAX as usize));

    // We need to connect to all of the devices that we have
    for device in devices.iter() {
        if let Device::Unknown = device {
            return Err(FFIMessage::UsingUnknownDevice);
        }
    }
    let mut device_addresses_to_connect: Vec<u64, MAX_CONNECTED_DEVICES> = heapless::Vec::new();

    // Create a temporary vector containing maximum amount of devices
    for _ in 0..MAX_CONNECTED_DEVICES {
        if let Some(device) = devices.pop_front() {
            let addr = match &device {
                Device::Nordic { device_info, .. } => u64::from_le_bytes(device_info.address),
                Device::Hub { device_info, .. } => u64::from_le_bytes(device_info.address),
                Device::Pico { device_info, .. } => u64::from_le_bytes(device_info.address),
                Device::TI { device_info, .. } => u64::from_le_bytes(device_info.address),
                Device::Unknown => {
                    return Err(FFIMessage::UsingUnknownDevice);
                }
            };
            if device_addresses_to_connect
                .iter()
                .filter(|x| **x == addr)
                .count()
                > 0
            {
                // We already have this device in the list
                // return poped device back to the list
                if let Err(_) = devices.push_back(device) {
                    return Err(FFIMessage::GenericAssertionError);
                }
                break;
            } else {
                if let Err(_) = device_addresses_to_connect.push(addr) {
                    return Err(FFIMessage::GenericAssertionError);
                }
                if let Err(_) = devices.push_back(device) {
                    return Err(FFIMessage::GenericAssertionError);
                }
            }
        };
    }
    esp32.connect_devices(&device_addresses_to_connect);

    Ok(())
}

/// finds devices. It waits 15 seconds to scan for devices unless it's signalled outherwise.

pub(crate) async fn find_devices(
    esp32: &'static impl Ffi,
    should_get_device_list: &AtomicBool,
) -> Result<(), FFIMessage> {
    // SeqCst is the safest and guarantees order
    if should_get_device_list.load(core::sync::atomic::Ordering::SeqCst) {
        esp32.start_remote_device_scan();
        // When we search for devices, we hold the name here. We need to keep it in RAM to prevent double searching.
        // We can index by id.
        // The name is limited to 8 chars since we define the address to be 8 bytes long (It's really 6 bytes long).
        let mut found_devices: Vec<[u8; 8], MAX_FOUND_DEVICES> = Vec::new();
        let start_time = esp32.get_time();
        // Look for data if it's less than 15 seconds since start_time, if SHOULD_GET_DEVICE_LIST is true, and if we have room.
        while (esp32.get_time() - start_time) <= Duration::from_secs(15)
            && should_get_device_list.load(core::sync::atomic::Ordering::SeqCst)
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
                            should_get_device_list
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
            let _ = Sleep::new(esp32, Duration::from_millis(100), || (false, ())).await;
        }
        // We're done. Now we have to clean up
        esp32.stop_remote_device_scan();
        // Make sure that we won't automatically start searching again
        should_get_device_list.store(false, core::sync::atomic::Ordering::SeqCst);
    }
    Ok(())
}

pub(crate) fn get_websocket_data(
    esp32: &impl Ffi,
    devices_list: &mut Vec<Device, MAX_DEVICES>,
    should_get_device_list: &AtomicBool,
) {
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
                                    add_devices(devices_list, esp32, &devices.devices).into()
                                {
                                    let err = FFIMessage::from(err);
                                    esp32.send_message(err);
                                }
                            }
                            protobufs::backend_to_firmware_packet::Type::StopGetDevicesList(_) => {
                                should_get_device_list
                                    .store(false, core::sync::atomic::Ordering::SeqCst);
                            }
                            protobufs::backend_to_firmware_packet::Type::GetDevicesList(_) => {
                                should_get_device_list
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
    use core::sync::atomic::AtomicBool;
    use core::time::Duration;
    use std::sync::Mutex;

    use cassette::pin_mut;
    use cassette::Cassette;

    use crate::backend_to_firmware::DeviceInfo;
    use crate::libs::time::Time;
    use crate::protobufs::DeviceInfo as protobuf_deviceinfo;
    use crate::test::Mock;
    use crate::FirmwareToBackendPacket;

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
            connect_devices: None,
            ble_send_devices_list: None,
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
                let mut info = protobuf_deviceinfo::default();
                info.address = 6618611909121;
                info.name = String::from("abcdefgh");
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
                Some(Ok(DeviceInfo {
                    address: [1, 2, 3, 4, 5, 6, 0, 0],
                    name: {
                        let mut v: heapless::Vec<u8, 29> = heapless::Vec::new();
                        "abcdefgh".bytes().for_each(|x| {
                            v.push(x).unwrap();
                        });
                        v
                    },
                }))
            }),
            connect_devices: None,
            ble_send_devices_list: None,
        });
        let mut should_get_device_list: AtomicBool = AtomicBool::new(false);
        should_get_device_list.store(true, core::sync::atomic::Ordering::SeqCst);
        let p = find_devices(Box::<Mock>::leak(m), &mut should_get_device_list);
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
