use crate::backend_to_firmware::AddSensorsEnum::UnableToConvert;

use crate::protobufs::AddSensorInfo;
use crate::system::{FFIMessage, Ffi};

use alloc::collections::VecDeque;
use alloc::string::String;

#[derive(Ord, PartialOrd, PartialEq, Eq, Clone)]
pub enum DeviceType {
    TI,
    Nordic,
    Pico,
    Hub,
}

impl TryFrom<i32> for DeviceType {
    type Error = i32;

    fn try_from(value: i32) -> Result<Self, Self::Error> {
        match value {
            0 => Ok(DeviceType::TI),
            1 => Ok(DeviceType::Nordic),
            2 => Ok(DeviceType::Pico),
            3 => Ok(DeviceType::Hub),
            val => Err(val),
        }
    }
}

#[derive(Ord, PartialOrd, Eq, Clone)]
pub struct Sensor {
    address: String,
    name: String,
    t: DeviceType,
}

impl PartialEq for Sensor {
    fn eq(&self, other: &Self) -> bool {
        self.address.eq(&other.address)
    }
}

impl From<&AddSensorInfo> for Option<Sensor> {
    fn from(value: &AddSensorInfo) -> Option<Sensor> {
        Some(Sensor {
            address: value.sensor_info.clone()?.address,
            name: value.sensor_info.clone()?.name,
            t: DeviceType::try_from(value.device_type).ok()?,
        })
    }
}

#[derive(Debug, Ord, PartialOrd, Eq, PartialEq)]
pub enum AddSensorsEnum {
    TooManySensors,
    UnableToConvert,
}

pub fn add_sensors<F: Ffi>(
    sensors_list: &mut VecDeque<Sensor>,
    esp32: &F,
    add_sensor_infos: &[AddSensorInfo],
) -> Result<(), AddSensorsEnum> {
    for s in add_sensor_infos {
        let s: Option<Sensor> = s.into();
        if let Some(s) = s {
            if !sensors_list.contains(&s) {
                if sensors_list.len() + 1 > sensors_list.capacity() {
                    esp32.send_message(FFIMessage::TooManySensors);
                    return Err(AddSensorsEnum::TooManySensors);
                }
                sensors_list.push_back(s);
            }
        } else {
            return Err(UnableToConvert);
        }
    }
    Ok(())
}
