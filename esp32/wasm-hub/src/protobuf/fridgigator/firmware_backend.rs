// Automatically generated rust module for 'firmware_backend.proto' file

#![allow(non_snake_case)]
#![allow(non_upper_case_globals)]
#![allow(non_camel_case_types)]
#![allow(unused_imports)]
#![allow(unknown_lints)]
#![allow(clippy::all)]
#![cfg_attr(rustfmt, rustfmt_skip)]


use alloc::vec::Vec;
use alloc::borrow::Cow;
use quick_protobuf::{MessageInfo, MessageRead, MessageWrite, BytesReader, Writer, WriterBackend, Result};
use quick_protobuf::sizeofs::*;
use super::super::*;

#[derive(Debug, PartialEq, Eq, Clone, Copy)]
pub enum DataType {
    DATA_TYPE_UNSPECIFIED = 0,
    DATA_TYPE_TEMP = 1,
    DATA_TYPE_HUMIDITY = 2,
    DATA_TYPE_DHT22_TEMP = 3,
    DATA_TYPE_DHT22_HUMIDITY = 4,
    DATA_TYPE_DHT11_TEMP = 5,
    DATA_TYPE_DHT11_HUMIDITY = 6,
    DATA_TYPE_PICO_TEMP = 7,
}

impl Default for DataType {
    fn default() -> Self {
        DataType::DATA_TYPE_UNSPECIFIED
    }
}

impl From<i32> for DataType {
    fn from(i: i32) -> Self {
        match i {
            0 => DataType::DATA_TYPE_UNSPECIFIED,
            1 => DataType::DATA_TYPE_TEMP,
            2 => DataType::DATA_TYPE_HUMIDITY,
            3 => DataType::DATA_TYPE_DHT22_TEMP,
            4 => DataType::DATA_TYPE_DHT22_HUMIDITY,
            5 => DataType::DATA_TYPE_DHT11_TEMP,
            6 => DataType::DATA_TYPE_DHT11_HUMIDITY,
            7 => DataType::DATA_TYPE_PICO_TEMP,
            _ => Self::default(),
        }
    }
}

impl<'a> From<&'a str> for DataType {
    fn from(s: &'a str) -> Self {
        match s {
            "DATA_TYPE_UNSPECIFIED" => DataType::DATA_TYPE_UNSPECIFIED,
            "DATA_TYPE_TEMP" => DataType::DATA_TYPE_TEMP,
            "DATA_TYPE_HUMIDITY" => DataType::DATA_TYPE_HUMIDITY,
            "DATA_TYPE_DHT22_TEMP" => DataType::DATA_TYPE_DHT22_TEMP,
            "DATA_TYPE_DHT22_HUMIDITY" => DataType::DATA_TYPE_DHT22_HUMIDITY,
            "DATA_TYPE_DHT11_TEMP" => DataType::DATA_TYPE_DHT11_TEMP,
            "DATA_TYPE_DHT11_HUMIDITY" => DataType::DATA_TYPE_DHT11_HUMIDITY,
            "DATA_TYPE_PICO_TEMP" => DataType::DATA_TYPE_PICO_TEMP,
            _ => Self::default(),
        }
    }
}

#[derive(Debug, PartialEq, Eq, Clone, Copy)]
pub enum DeviceType {
    DEVICE_TYPE_UNSPECIFIED = 0,
    DEVICE_TYPE_TI = 1,
    DEVICE_TYPE_NORDIC = 2,
    DEVICE_TYPE_CUSTOM = 3,
    DEVICE_TYPE_HUB = 4,
}

impl Default for DeviceType {
    fn default() -> Self {
        DeviceType::DEVICE_TYPE_UNSPECIFIED
    }
}

impl From<i32> for DeviceType {
    fn from(i: i32) -> Self {
        match i {
            0 => DeviceType::DEVICE_TYPE_UNSPECIFIED,
            1 => DeviceType::DEVICE_TYPE_TI,
            2 => DeviceType::DEVICE_TYPE_NORDIC,
            3 => DeviceType::DEVICE_TYPE_CUSTOM,
            4 => DeviceType::DEVICE_TYPE_HUB,
            _ => Self::default(),
        }
    }
}

impl<'a> From<&'a str> for DeviceType {
    fn from(s: &'a str) -> Self {
        match s {
            "DEVICE_TYPE_UNSPECIFIED" => DeviceType::DEVICE_TYPE_UNSPECIFIED,
            "DEVICE_TYPE_TI" => DeviceType::DEVICE_TYPE_TI,
            "DEVICE_TYPE_NORDIC" => DeviceType::DEVICE_TYPE_NORDIC,
            "DEVICE_TYPE_CUSTOM" => DeviceType::DEVICE_TYPE_CUSTOM,
            "DEVICE_TYPE_HUB" => DeviceType::DEVICE_TYPE_HUB,
            _ => Self::default(),
        }
    }
}

#[allow(clippy::derive_partial_eq_without_eq)]
#[derive(Debug, Default, PartialEq, Clone)]
pub struct FirmwareToBackendPacket<'a> {
    pub type_pb: fridgigator::firmware_backend::mod_FirmwareToBackendPacket::OneOftype_pb<'a>,
}

impl<'a> MessageRead<'a> for FirmwareToBackendPacket<'a> {
    fn from_reader(r: &mut BytesReader, bytes: &'a [u8]) -> Result<Self> {
        let mut msg = Self::default();
        while !r.is_eof() {
            match r.next_tag(bytes) {
                Ok(10) => msg.type_pb = fridgigator::firmware_backend::mod_FirmwareToBackendPacket::OneOftype_pb::ping(r.read_message::<fridgigator::firmware_backend::Ping>(bytes)?),
                Ok(18) => msg.type_pb = fridgigator::firmware_backend::mod_FirmwareToBackendPacket::OneOftype_pb::sensor_data(r.read_message::<fridgigator::firmware_backend::SensorData>(bytes)?),
                Ok(138) => msg.type_pb = fridgigator::firmware_backend::mod_FirmwareToBackendPacket::OneOftype_pb::devices_list(r.read_message::<fridgigator::firmware_backend::DevicesList>(bytes)?),
                Ok(146) => msg.type_pb = fridgigator::firmware_backend::mod_FirmwareToBackendPacket::OneOftype_pb::registration(r.read_message::<fridgigator::firmware_backend::Registration>(bytes)?),
                Ok(154) => msg.type_pb = fridgigator::firmware_backend::mod_FirmwareToBackendPacket::OneOftype_pb::stop_get_devices_list(r.read_message::<fridgigator::firmware_backend::StopGetDevicesList>(bytes)?),
                Ok(t) => { r.read_unknown(bytes, t)?; }
                Err(e) => return Err(e),
            }
        }
        Ok(msg)
    }
}

impl<'a> MessageWrite for FirmwareToBackendPacket<'a> {
    fn get_size(&self) -> usize {
        0
        + match self.type_pb {
            fridgigator::firmware_backend::mod_FirmwareToBackendPacket::OneOftype_pb::ping(ref m) => 1 + sizeof_len((m).get_size()),
            fridgigator::firmware_backend::mod_FirmwareToBackendPacket::OneOftype_pb::sensor_data(ref m) => 1 + sizeof_len((m).get_size()),
            fridgigator::firmware_backend::mod_FirmwareToBackendPacket::OneOftype_pb::devices_list(ref m) => 2 + sizeof_len((m).get_size()),
            fridgigator::firmware_backend::mod_FirmwareToBackendPacket::OneOftype_pb::registration(ref m) => 2 + sizeof_len((m).get_size()),
            fridgigator::firmware_backend::mod_FirmwareToBackendPacket::OneOftype_pb::stop_get_devices_list(ref m) => 2 + sizeof_len((m).get_size()),
            fridgigator::firmware_backend::mod_FirmwareToBackendPacket::OneOftype_pb::None => 0,
    }    }

    fn write_message<W: WriterBackend>(&self, w: &mut Writer<W>) -> Result<()> {
        match self.type_pb {            fridgigator::firmware_backend::mod_FirmwareToBackendPacket::OneOftype_pb::ping(ref m) => { w.write_with_tag(10, |w| w.write_message(m))? },
            fridgigator::firmware_backend::mod_FirmwareToBackendPacket::OneOftype_pb::sensor_data(ref m) => { w.write_with_tag(18, |w| w.write_message(m))? },
            fridgigator::firmware_backend::mod_FirmwareToBackendPacket::OneOftype_pb::devices_list(ref m) => { w.write_with_tag(138, |w| w.write_message(m))? },
            fridgigator::firmware_backend::mod_FirmwareToBackendPacket::OneOftype_pb::registration(ref m) => { w.write_with_tag(146, |w| w.write_message(m))? },
            fridgigator::firmware_backend::mod_FirmwareToBackendPacket::OneOftype_pb::stop_get_devices_list(ref m) => { w.write_with_tag(154, |w| w.write_message(m))? },
            fridgigator::firmware_backend::mod_FirmwareToBackendPacket::OneOftype_pb::None => {},
    }        Ok(())
    }
}

pub mod mod_FirmwareToBackendPacket {

use alloc::vec::Vec;
use super::*;

#[derive(Debug, PartialEq, Clone)]
pub enum OneOftype_pb<'a> {
    ping(fridgigator::firmware_backend::Ping),
    sensor_data(fridgigator::firmware_backend::SensorData),
    devices_list(fridgigator::firmware_backend::DevicesList<'a>),
    registration(fridgigator::firmware_backend::Registration),
    stop_get_devices_list(fridgigator::firmware_backend::StopGetDevicesList),
    None,
}

impl<'a> Default for OneOftype_pb<'a> {
    fn default() -> Self {
        OneOftype_pb::None
    }
}

}

#[allow(clippy::derive_partial_eq_without_eq)]
#[derive(Debug, Default, PartialEq, Clone)]
pub struct Ping { }

impl<'a> MessageRead<'a> for Ping {
    fn from_reader(r: &mut BytesReader, _: &[u8]) -> Result<Self> {
        r.read_to_end();
        Ok(Self::default())
    }
}

impl MessageWrite for Ping { }

#[allow(clippy::derive_partial_eq_without_eq)]
#[derive(Debug, Default, PartialEq, Clone)]
pub struct UUID {
    pub uuid_lower: i64,
    pub uuid_higher: i64,
}

impl<'a> MessageRead<'a> for UUID {
    fn from_reader(r: &mut BytesReader, bytes: &'a [u8]) -> Result<Self> {
        let mut msg = Self::default();
        while !r.is_eof() {
            match r.next_tag(bytes) {
                Ok(8) => msg.uuid_lower = r.read_int64(bytes)?,
                Ok(16) => msg.uuid_higher = r.read_int64(bytes)?,
                Ok(t) => { r.read_unknown(bytes, t)?; }
                Err(e) => return Err(e),
            }
        }
        Ok(msg)
    }
}

impl MessageWrite for UUID {
    fn get_size(&self) -> usize {
        0
        + if self.uuid_lower == 0i64 { 0 } else { 1 + sizeof_varint(*(&self.uuid_lower) as u64) }
        + if self.uuid_higher == 0i64 { 0 } else { 1 + sizeof_varint(*(&self.uuid_higher) as u64) }
    }

    fn write_message<W: WriterBackend>(&self, w: &mut Writer<W>) -> Result<()> {
        if self.uuid_lower != 0i64 { w.write_with_tag(8, |w| w.write_int64(*&self.uuid_lower))?; }
        if self.uuid_higher != 0i64 { w.write_with_tag(16, |w| w.write_int64(*&self.uuid_higher))?; }
        Ok(())
    }
}

#[allow(clippy::derive_partial_eq_without_eq)]
#[derive(Debug, Default, PartialEq, Clone)]
pub struct Registration {
    pub uuid_self: Option<fridgigator::firmware_backend::UUID>,
    pub uuid_user: Option<fridgigator::firmware_backend::UUID>,
}

impl<'a> MessageRead<'a> for Registration {
    fn from_reader(r: &mut BytesReader, bytes: &'a [u8]) -> Result<Self> {
        let mut msg = Self::default();
        while !r.is_eof() {
            match r.next_tag(bytes) {
                Ok(10) => msg.uuid_self = Some(r.read_message::<fridgigator::firmware_backend::UUID>(bytes)?),
                Ok(18) => msg.uuid_user = Some(r.read_message::<fridgigator::firmware_backend::UUID>(bytes)?),
                Ok(t) => { r.read_unknown(bytes, t)?; }
                Err(e) => return Err(e),
            }
        }
        Ok(msg)
    }
}

impl MessageWrite for Registration {
    fn get_size(&self) -> usize {
        0
        + self.uuid_self.as_ref().map_or(0, |m| 1 + sizeof_len((m).get_size()))
        + self.uuid_user.as_ref().map_or(0, |m| 1 + sizeof_len((m).get_size()))
    }

    fn write_message<W: WriterBackend>(&self, w: &mut Writer<W>) -> Result<()> {
        if let Some(ref s) = self.uuid_self { w.write_with_tag(10, |w| w.write_message(s))?; }
        if let Some(ref s) = self.uuid_user { w.write_with_tag(18, |w| w.write_message(s))?; }
        Ok(())
    }
}

#[allow(clippy::derive_partial_eq_without_eq)]
#[derive(Debug, Default, PartialEq, Clone)]
pub struct SensorsStart { }

impl<'a> MessageRead<'a> for SensorsStart {
    fn from_reader(r: &mut BytesReader, _: &[u8]) -> Result<Self> {
        r.read_to_end();
        Ok(Self::default())
    }
}

impl MessageWrite for SensorsStart { }

#[allow(clippy::derive_partial_eq_without_eq)]
#[derive(Debug, Default, PartialEq, Clone)]
pub struct DeviceInfo<'a> {
    pub address: i64,
    pub name: Cow<'a, str>,
    pub device_type: fridgigator::firmware_backend::DeviceType,
}

impl<'a> MessageRead<'a> for DeviceInfo<'a> {
    fn from_reader(r: &mut BytesReader, bytes: &'a [u8]) -> Result<Self> {
        let mut msg = Self::default();
        while !r.is_eof() {
            match r.next_tag(bytes) {
                Ok(8) => msg.address = r.read_int64(bytes)?,
                Ok(18) => msg.name = r.read_string(bytes).map(Cow::Borrowed)?,
                Ok(24) => msg.device_type = r.read_enum(bytes)?,
                Ok(t) => { r.read_unknown(bytes, t)?; }
                Err(e) => return Err(e),
            }
        }
        Ok(msg)
    }
}

impl<'a> MessageWrite for DeviceInfo<'a> {
    fn get_size(&self) -> usize {
        0
        + if self.address == 0i64 { 0 } else { 1 + sizeof_varint(*(&self.address) as u64) }
        + if self.name == "" { 0 } else { 1 + sizeof_len((&self.name).len()) }
        + if self.device_type == fridgigator::firmware_backend::DeviceType::DEVICE_TYPE_UNSPECIFIED { 0 } else { 1 + sizeof_varint(*(&self.device_type) as u64) }
    }

    fn write_message<W: WriterBackend>(&self, w: &mut Writer<W>) -> Result<()> {
        if self.address != 0i64 { w.write_with_tag(8, |w| w.write_int64(*&self.address))?; }
        if self.name != "" { w.write_with_tag(18, |w| w.write_string(&**&self.name))?; }
        if self.device_type != fridgigator::firmware_backend::DeviceType::DEVICE_TYPE_UNSPECIFIED { w.write_with_tag(24, |w| w.write_enum(*&self.device_type as i32))?; }
        Ok(())
    }
}

#[allow(clippy::derive_partial_eq_without_eq)]
#[derive(Debug, Default, PartialEq, Clone)]
pub struct SensorData {
    pub address: i64,
    pub data_type: fridgigator::firmware_backend::DataType,
    pub value: f64,
    pub timestamp: i64,
}

impl<'a> MessageRead<'a> for SensorData {
    fn from_reader(r: &mut BytesReader, bytes: &'a [u8]) -> Result<Self> {
        let mut msg = Self::default();
        while !r.is_eof() {
            match r.next_tag(bytes) {
                Ok(8) => msg.address = r.read_int64(bytes)?,
                Ok(16) => msg.data_type = r.read_enum(bytes)?,
                Ok(25) => msg.value = r.read_double(bytes)?,
                Ok(32) => msg.timestamp = r.read_int64(bytes)?,
                Ok(t) => { r.read_unknown(bytes, t)?; }
                Err(e) => return Err(e),
            }
        }
        Ok(msg)
    }
}

impl MessageWrite for SensorData {
    fn get_size(&self) -> usize {
        0
        + if self.address == 0i64 { 0 } else { 1 + sizeof_varint(*(&self.address) as u64) }
        + if self.data_type == fridgigator::firmware_backend::DataType::DATA_TYPE_UNSPECIFIED { 0 } else { 1 + sizeof_varint(*(&self.data_type) as u64) }
        + if self.value == 0f64 { 0 } else { 1 + 8 }
        + if self.timestamp == 0i64 { 0 } else { 1 + sizeof_varint(*(&self.timestamp) as u64) }
    }

    fn write_message<W: WriterBackend>(&self, w: &mut Writer<W>) -> Result<()> {
        if self.address != 0i64 { w.write_with_tag(8, |w| w.write_int64(*&self.address))?; }
        if self.data_type != fridgigator::firmware_backend::DataType::DATA_TYPE_UNSPECIFIED { w.write_with_tag(16, |w| w.write_enum(*&self.data_type as i32))?; }
        if self.value != 0f64 { w.write_with_tag(25, |w| w.write_double(*&self.value))?; }
        if self.timestamp != 0i64 { w.write_with_tag(32, |w| w.write_int64(*&self.timestamp))?; }
        Ok(())
    }
}

#[allow(clippy::derive_partial_eq_without_eq)]
#[derive(Debug, Default, PartialEq, Clone)]
pub struct BackendToFirmwarePacket<'a> {
    pub type_pb: fridgigator::firmware_backend::mod_BackendToFirmwarePacket::OneOftype_pb<'a>,
}

impl<'a> MessageRead<'a> for BackendToFirmwarePacket<'a> {
    fn from_reader(r: &mut BytesReader, bytes: &'a [u8]) -> Result<Self> {
        let mut msg = Self::default();
        while !r.is_eof() {
            match r.next_tag(bytes) {
                Ok(18) => msg.type_pb = fridgigator::firmware_backend::mod_BackendToFirmwarePacket::OneOftype_pb::get_devices_list(r.read_message::<fridgigator::firmware_backend::GetDevicesList>(bytes)?),
                Ok(26) => msg.type_pb = fridgigator::firmware_backend::mod_BackendToFirmwarePacket::OneOftype_pb::stop_get_devices_list(r.read_message::<fridgigator::firmware_backend::StopGetDevicesList>(bytes)?),
                Ok(34) => msg.type_pb = fridgigator::firmware_backend::mod_BackendToFirmwarePacket::OneOftype_pb::devices(r.read_message::<fridgigator::firmware_backend::DevicesList>(bytes)?),
                Ok(t) => { r.read_unknown(bytes, t)?; }
                Err(e) => return Err(e),
            }
        }
        Ok(msg)
    }
}

impl<'a> MessageWrite for BackendToFirmwarePacket<'a> {
    fn get_size(&self) -> usize {
        0
        + match self.type_pb {
            fridgigator::firmware_backend::mod_BackendToFirmwarePacket::OneOftype_pb::get_devices_list(ref m) => 1 + sizeof_len((m).get_size()),
            fridgigator::firmware_backend::mod_BackendToFirmwarePacket::OneOftype_pb::stop_get_devices_list(ref m) => 1 + sizeof_len((m).get_size()),
            fridgigator::firmware_backend::mod_BackendToFirmwarePacket::OneOftype_pb::devices(ref m) => 1 + sizeof_len((m).get_size()),
            fridgigator::firmware_backend::mod_BackendToFirmwarePacket::OneOftype_pb::None => 0,
    }    }

    fn write_message<W: WriterBackend>(&self, w: &mut Writer<W>) -> Result<()> {
        match self.type_pb {            fridgigator::firmware_backend::mod_BackendToFirmwarePacket::OneOftype_pb::get_devices_list(ref m) => { w.write_with_tag(18, |w| w.write_message(m))? },
            fridgigator::firmware_backend::mod_BackendToFirmwarePacket::OneOftype_pb::stop_get_devices_list(ref m) => { w.write_with_tag(26, |w| w.write_message(m))? },
            fridgigator::firmware_backend::mod_BackendToFirmwarePacket::OneOftype_pb::devices(ref m) => { w.write_with_tag(34, |w| w.write_message(m))? },
            fridgigator::firmware_backend::mod_BackendToFirmwarePacket::OneOftype_pb::None => {},
    }        Ok(())
    }
}

pub mod mod_BackendToFirmwarePacket {

use alloc::vec::Vec;
use super::*;

#[derive(Debug, PartialEq, Clone)]
pub enum OneOftype_pb<'a> {
    get_devices_list(fridgigator::firmware_backend::GetDevicesList),
    stop_get_devices_list(fridgigator::firmware_backend::StopGetDevicesList),
    devices(fridgigator::firmware_backend::DevicesList<'a>),
    None,
}

impl<'a> Default for OneOftype_pb<'a> {
    fn default() -> Self {
        OneOftype_pb::None
    }
}

}

#[allow(clippy::derive_partial_eq_without_eq)]
#[derive(Debug, Default, PartialEq, Clone)]
pub struct DevicesList<'a> {
    pub devices: Vec<fridgigator::firmware_backend::DeviceInfo<'a>>,
}

impl<'a> MessageRead<'a> for DevicesList<'a> {
    fn from_reader(r: &mut BytesReader, bytes: &'a [u8]) -> Result<Self> {
        let mut msg = Self::default();
        while !r.is_eof() {
            match r.next_tag(bytes) {
                Ok(10) => msg.devices.push(r.read_message::<fridgigator::firmware_backend::DeviceInfo>(bytes)?),
                Ok(t) => { r.read_unknown(bytes, t)?; }
                Err(e) => return Err(e),
            }
        }
        Ok(msg)
    }
}

impl<'a> MessageWrite for DevicesList<'a> {
    fn get_size(&self) -> usize {
        0
        + self.devices.iter().map(|s| 1 + sizeof_len((s).get_size())).sum::<usize>()
    }

    fn write_message<W: WriterBackend>(&self, w: &mut Writer<W>) -> Result<()> {
        for s in &self.devices { w.write_with_tag(10, |w| w.write_message(s))?; }
        Ok(())
    }
}

#[allow(clippy::derive_partial_eq_without_eq)]
#[derive(Debug, Default, PartialEq, Clone)]
pub struct GetDevicesList { }

impl<'a> MessageRead<'a> for GetDevicesList {
    fn from_reader(r: &mut BytesReader, _: &[u8]) -> Result<Self> {
        r.read_to_end();
        Ok(Self::default())
    }
}

impl MessageWrite for GetDevicesList { }

#[allow(clippy::derive_partial_eq_without_eq)]
#[derive(Debug, Default, PartialEq, Clone)]
pub struct StopGetDevicesList { }

impl<'a> MessageRead<'a> for StopGetDevicesList {
    fn from_reader(r: &mut BytesReader, _: &[u8]) -> Result<Self> {
        r.read_to_end();
        Ok(Self::default())
    }
}

impl MessageWrite for StopGetDevicesList { }

