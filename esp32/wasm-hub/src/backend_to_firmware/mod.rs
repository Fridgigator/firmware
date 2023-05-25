use crate::alloc::string::ToString;
use crate::constants::MAX_BLE_SIZE_IN_BYTES;
use crate::libs::time::Time;
use crate::protobufs::DeviceInfo as protobuf_deviceinfo;
use crate::system::{FFIMessage, Ffi};
use crate::MAX_DEVICES;
use heapless::Vec;

/// This represents a remote device type
///
/// A device can have multiple sensors or just one. Each device has its own protocol and initialization procedure.
/// The contents may change based on need and whether we add other device types.
#[derive(PartialOrd, PartialEq, Clone, Debug)]
pub enum Device {
    /// An unknown device. Should never be used.
    Unknown,
    /// A TI device. We currently use the LPSTK CC1351. This is sent back to the server as type 1.
    TI {
        device_info: DeviceInfo,
        humidity_sensor_data: Option<SensorData>,
        temperature_sensor_data: Option<SensorData>,
    },
    /// A Nordic Thingy. We currently use the Nordic Thingy: 52. This is sent back to the server as type 2.
    Nordic {
        device_info: DeviceInfo,
        humidity_sensor_data: Option<SensorData>,
        temperature_sensor_data: Option<SensorData>,
    },
    /// A Pico. This is a custom built module that has DHT-11, DHT-22 and other sensors attached, and connects via a Bluetooth module. This is sent back to the server as type 3.
    Pico {
        device_info: DeviceInfo,
        dht11_humidity_sensor_data: Option<SensorData>,
        dht22_humidity_sensor_data: Option<SensorData>,
        dht11_temperature_sensor_data: Option<SensorData>,
        dht22_temperature_sensor_data: Option<SensorData>,
        pico_temperature_sensor_data: Option<SensorData>,
    },
    /// Another hub. This is currently only esp32's, but we may add other hub device. This is sent back to the server as type 4.
    Hub { device_info: DeviceInfo },
}

/// This is a device. Each device has an address
#[derive(Ord, PartialOrd, Eq, Clone, Debug)]
pub struct DeviceInfo {
    /// Address is always 6 chars long, but for convenience it's 8 bytes long here with the largest values set to 0.
    pub address: [u8; 8],

    /// name represents the device name. It can be of variable length, but will have a maximum value of [MAX_BLE_SIZE_IN_BYTES]
    pub name: Vec<u8, MAX_BLE_SIZE_IN_BYTES>,
}

#[derive(PartialOrd, Clone, Debug, PartialEq)]
pub struct SensorData {
    time_obtained: Time,
    value: f32,
}

/// Represents a type of
#[derive(Ord, PartialOrd, Eq, Clone, Debug, PartialEq)]
enum SensorType {}

impl DeviceInfo {
    #[must_use]
    /// This generates a new device from an address, a name and a type.
    pub fn new(address: [u8; 8], name: Vec<u8, MAX_BLE_SIZE_IN_BYTES>) -> Self {
        Self { address, name }
    }
}

impl PartialEq for DeviceInfo {
    fn eq(&self, other: &Self) -> bool {
        self.address.eq(&other.address)
    }
}
pub enum ProtobufDeviceInfoToDeviceError {
    LastAddressBytesAreNotNull,
    NameTooLong,
    WrongDeviceInfo(i32),
}
impl TryFrom<&protobuf_deviceinfo> for Device {
    type Error = ProtobufDeviceInfoToDeviceError;
    fn try_from(value: &protobuf_deviceinfo) -> Result<Device, ProtobufDeviceInfoToDeviceError> {
        let address_byte_array: [u8; 8] = value.clone().address.to_le_bytes();
        // Little endien means that the smallest bytes are on the left, so since the ble address can only be 6 bytes long, the last two bytes should be 0.
        if address_byte_array[6] != 0 || address_byte_array[7] != 0 {
            Err(ProtobufDeviceInfoToDeviceError::LastAddressBytesAreNotNull)
        } else {
            let mut bytes: Vec<u8, MAX_BLE_SIZE_IN_BYTES> = Vec::new();
            for c in value.name.as_str().as_bytes().iter() {
                // If it doesn't fit, just return none.
                if bytes.push(*c).is_err() {
                    return Err(ProtobufDeviceInfoToDeviceError::NameTooLong);
                }
            }
            match value.device_type {
                0 => Ok(Device::Unknown),
                1 => Ok(Device::TI {
                    device_info: DeviceInfo::new(address_byte_array, bytes),
                    humidity_sensor_data: None,
                    temperature_sensor_data: None,
                }),
                2 => Ok(Device::Nordic {
                    device_info: DeviceInfo::new(address_byte_array, bytes),
                    humidity_sensor_data: None,
                    temperature_sensor_data: None,
                }),
                3 => Ok(Device::Pico {
                    device_info: DeviceInfo::new(address_byte_array, bytes),
                    dht11_humidity_sensor_data: None,
                    dht22_humidity_sensor_data: None,
                    dht11_temperature_sensor_data: None,
                    dht22_temperature_sensor_data: None,
                    pico_temperature_sensor_data: None,
                }),
                4 => Ok(Device::Hub {
                    device_info: DeviceInfo::new(address_byte_array, bytes),
                }),
                val => Err(ProtobufDeviceInfoToDeviceError::WrongDeviceInfo(val)),
            }
        }
    }
}

impl From<DeviceInfo> for Option<protobuf_deviceinfo> {
    fn from(value: DeviceInfo) -> Option<protobuf_deviceinfo> {
        if let Ok(s) = core::str::from_utf8(value.name.as_ref()) {
            Some(protobuf_deviceinfo {
                address: i64::from_le_bytes(value.address),
                name: s.to_string(),
                device_type: 0,
            })
        } else {
            None
        }
    }
}

/// An error value that can come from being unable to add a device
#[derive(Debug, Ord, PartialOrd, Eq, PartialEq)]
pub enum AddDeviceError {
    /// We try to add more devices than can fit in our array
    TooManyDevices,
    /// Name is too long
    NameTooLong,
    /// Wrong Device Type
    WrongDeviceType,
}

impl From<AddDeviceError> for FFIMessage {
    fn from(value: AddDeviceError) -> Self {
        match value {
            AddDeviceError::TooManyDevices => FFIMessage::TooManyDevices,
            AddDeviceError::NameTooLong => FFIMessage::NameTooLong,
            AddDeviceError::WrongDeviceType => FFIMessage::WrongDeviceType,
        }
    }
}
fn name_to_vec(name: &str) -> Result<heapless::Vec<u8, MAX_BLE_SIZE_IN_BYTES>, AddDeviceError> {
    let mut ret_val: Vec<u8, MAX_BLE_SIZE_IN_BYTES> = heapless::Vec::new();
    for ch in name.bytes() {
        ret_val.push(ch).map_err(|_| AddDeviceError::NameTooLong)?;
    }
    Ok(ret_val)
}

/// Given a list of BLE reachable devices (using protobuf encodable structs) from the server, convert them to our representation and add them to the list.
pub fn add_devices<F: Ffi>(
    devices: &mut Vec<Device, MAX_DEVICES>,
    esp32: &F,
    protobuf_devices: &[protobuf_deviceinfo],
) -> Result<(), AddDeviceError> {
    // Empty devices list before adding anything
    devices.clear();
    for s in protobuf_devices {
        // convert protobuf device into our own type
        let s: Device = {
            match s.device_type {
                0 => Ok(Device::Unknown),
                1 => Ok(Device::TI {
                    device_info: DeviceInfo::new(s.address.to_le_bytes(), name_to_vec(&s.name)?),
                    humidity_sensor_data: None,
                    temperature_sensor_data: None,
                }),
                2 => Ok(Device::Nordic {
                    device_info: DeviceInfo::new(s.address.to_le_bytes(), name_to_vec(&s.name)?),
                    humidity_sensor_data: None,
                    temperature_sensor_data: None,
                }),
                3 => Ok(Device::Pico {
                    device_info: DeviceInfo::new(s.address.to_le_bytes(), name_to_vec(&s.name)?),
                    dht11_humidity_sensor_data: None,
                    dht22_humidity_sensor_data: None,
                    dht11_temperature_sensor_data: None,
                    dht22_temperature_sensor_data: None,
                    pico_temperature_sensor_data: None,
                }),
                4 => Ok(Device::Hub {
                    device_info: DeviceInfo::new(s.address.to_le_bytes(), name_to_vec(&s.name)?),
                }),
                _ => Err(AddDeviceError::WrongDeviceType),
            }
        }?;

        // If this device is not in the list, no need to add it
        if !devices.contains(&s) {
            // Try to add the device, if it doesn't fit, send a message and exit
            if devices.push(s).is_err() {
                esp32.send_message(FFIMessage::TooManyDevices);
                return Err(AddDeviceError::TooManyDevices);
            }
        }
    }
    Ok(())
}
