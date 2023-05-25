/// The max length of a ble device name is 29 chars: https://stackoverflow.com/questions/65568893/how-to-know-the-maximum-length-of-bt-name
pub const MAX_BLE_SIZE_IN_BYTES: usize = 29;

/// Maximum amount of devices that we can hold
pub const MAX_DEVICES: usize = 64;

/// Maximum amount of found devices that we can hold
pub const MAX_FOUND_DEVICES: usize = 64;

pub const MAX_AMNT_SENSORS: usize = 16;
