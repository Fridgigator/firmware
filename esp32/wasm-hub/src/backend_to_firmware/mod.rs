use crate::backend_to_firmware::AddDeviceError::UnableToConvert;
use crate::constants::MAX_BLE_SIZE_IN_BYTES;
use crate::protobufs::DeviceInfo;
use crate::system::{FFIMessage, Ffi};
use crate::MAX_DEVICES;
use heapless::Vec;
use crate::alloc::string::ToString;

/// This represents a remote device type
/// 
/// A device can have multiple sensors or just one. Each device has its own protocol and initialization procedure. 
/// The contents may change based on need and whether we add other device types.
#[derive(Ord, PartialOrd, PartialEq, Eq, Clone, Debug)]
pub enum DeviceType {
    /// An unknown device. Should never be used.
    Unknown,
    /// A TI device. We currently use the LPSTK CC1351. This is sent back to the server as type 1.
    TI,
    /// A Nordic Thingy. We currently use the Nordic Thingy: 52. This is sent back to the server as type 2.
    Nordic,
    /// A Pico. This is a custom built module that has DHT-11, DHT-22 and other sensors attached, and connects via a Bluetooth module. This is sent back to the server as type 3.
    Pico,
    /// Another hub. This is currently only esp32's, but we may add other hub device. This is sent back to the server as type 4.
    Hub,
}

impl TryFrom<i32> for DeviceType {
    type Error = i32;
    
    fn try_from(value: i32) -> Result<Self, Self::Error> {
        match value {
            0 => Ok(DeviceType::Unknown),
            1 => Ok(DeviceType::TI),
            2 => Ok(DeviceType::Nordic),
            3 => Ok(DeviceType::Pico),
            4 => Ok(DeviceType::Hub),
            val => Err(val),
        }
    }
}

impl From<DeviceType> for i32 {

    #[must_use]
    fn from(value: DeviceType) -> Self {
        match value {
            DeviceType::Unknown => 0,
            DeviceType::TI => 1,
            DeviceType::Nordic => 2,
            DeviceType::Pico => 3,
            DeviceType::Hub => 4,
        }
    }
}

/// This is a device. Each device has an address
#[derive(Ord, PartialOrd, Eq, Clone, Debug)]
pub struct Device {
    /// Address is always 6 chars long, but for convenience it's 8 bytes long here with the largest values set to 0.
    pub address: [u8; 8],

    /// name represents the device name. It can be of variable length, but will have a maximum value of [MAX_BLE_SIZE_IN_BYTES]
    pub name: Vec<u8, MAX_BLE_SIZE_IN_BYTES>,

    /// device type represents a type.
    pub device_type: DeviceType,
}

impl Device {
    #[must_use]
    /// This generates a new device from an address, a name and a type.
    pub fn new(address: [u8; 8], name: Vec<u8, MAX_BLE_SIZE_IN_BYTES>, t: DeviceType) -> Self {
        Self {
            address,
            name,
            device_type: t,
        }
    }
}

impl PartialEq for Device {
    fn eq(&self, other: &Self) -> bool {
        self.address.eq(&other.address)
    }
}

impl From<&DeviceInfo> for Option<Device> {
    fn from(value: &DeviceInfo) -> Option<Device> {
        let address_byte_array: [u8; 8] = value.clone().address.to_le_bytes();
        // Little endien means that the smallest bytes are on the left, so since the ble address can only be 6 bytes long, the last two bytes should be 0.
        if address_byte_array[6] != 0 || address_byte_array[7] != 0 {
            None
        } else {
            let mut bytes: Vec<u8, MAX_BLE_SIZE_IN_BYTES> = Vec::new();
            for c in value.name.as_str().as_bytes().iter() {
                // If it doesn't fit, just return none.
                if bytes.push(*c).is_err() {
                    return None;
                }
            }
            Some(Device {
                address: address_byte_array,
                name: bytes,
                device_type: DeviceType::try_from(value.device_type).ok()?,
            })
        }
    }
}

impl From<Device> for Option<DeviceInfo> {
    fn from(value: Device) -> Option<DeviceInfo> {
        if let Ok(s) = core::str::from_utf8(value.name.as_ref()) {
            Some(DeviceInfo {
                address: i64::from_le_bytes(value.address),
                name: s.to_string(),
                device_type: value.device_type.into(),
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
    /// The device that we received was not legal.
    UnableToConvert,
}

/// Given a list of BLE reachable devices (using protobuf encodable structs) from the server, convert them to our representation and add them to the list.
pub fn add_devices<F: Ffi>(
    devices_list: &mut Vec<Device, MAX_DEVICES>,
    esp32: &F,
    device_infos: &[DeviceInfo],
) -> Result<(), AddDeviceError> {
    // Empty devices list before adding anything
    devices_list.clear();
    for s in device_infos {
        // convert protobuf device into our own type
        let s: Option<Device> = s.into();
        match s {
            // If it worked
            Some(s) => {
                // If this device is not in the list, no need to add it
                if !devices_list.contains(&s) {
                    // Try to add the device, if it doesn't fit, send a message and exit
                    if devices_list.push(s).is_err() {
                        esp32.send_message(FFIMessage::TooManyDevices);
                        return Err(AddDeviceError::TooManyDevices);
                    }
                }
            }
            None => {
                // If a device was unable to be converted
                return Err(UnableToConvert);
            }
        }
    }
    Ok(())
}
