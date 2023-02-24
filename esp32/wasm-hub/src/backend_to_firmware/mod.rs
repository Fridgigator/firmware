use crate::backend_to_firmware::AddSensorsEnum::UnableToConvert;

use crate::protobufs::AddSensorInfo;
use crate::system::{FFIMessage, Ffi};
use alloc::string::String;
use alloc::vec::Vec;

#[derive(Ord, PartialOrd, PartialEq, Eq, Clone)]
pub enum DeviceType {
    Unknown,
    TI,
    Nordic,
    Pico,
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

#[derive(Ord, PartialOrd, Eq, Clone)]
pub struct Device {
    pub address: [u8; 6],
    pub name: String,
    pub device_type: DeviceType,
}

impl Device {
    pub fn new(address: [u8; 6], name: String, t: DeviceType) -> Self {
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

impl From<&AddSensorInfo> for Option<Device> {
    fn from(value: &AddSensorInfo) -> Option<Device> {
        let address_byte_array: [u8; 8] = value.clone().sensor_info?.address.to_le_bytes();
        // Little endien means that the smallest bytes are on the left, so since the ble address can only be 6 bytes long, the last two bytes should be 0
        if address_byte_array[6] != 0 || address_byte_array[7] != 0 {
            None
        } else {
            Some(Device {
                address: [
                    address_byte_array[0],
                    address_byte_array[1],
                    address_byte_array[2],
                    address_byte_array[3],
                    address_byte_array[4],
                    address_byte_array[5],
                ],
                name: value.sensor_info.clone()?.name,
                device_type: DeviceType::try_from(value.device_type).ok()?,
            })
        }
    }
}

#[derive(Debug, Ord, PartialOrd, Eq, PartialEq)]
pub enum AddSensorsEnum {
    TooManySensors,
    UnableToConvert,
}

pub fn add_sensors<F: Ffi>(
    sensors_list: &mut Vec<Device>,
    esp32: &F,
    add_sensor_infos: &[AddSensorInfo],
) -> Result<(), AddSensorsEnum> {
    for s in add_sensor_infos {
        let s: Option<Device> = s.into();
        if let Some(s) = s {
            if !sensors_list.contains(&s) {
                if sensors_list.len() + 1 > sensors_list.capacity() {
                    esp32.send_message(FFIMessage::TooManySensors);
                    return Err(AddSensorsEnum::TooManySensors);
                }
                sensors_list.push(s);
            }
        } else {
            return Err(UnableToConvert);
        }
    }
    Ok(())
}
