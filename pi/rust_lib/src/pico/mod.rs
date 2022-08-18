/// This module contains required libraries to interact with the internal state of the pico such as obtaining a representation of its pins and putting it to sleep.
pub mod device;
/// This module contains required libraries to interact with different peripherals, such as external LEDs, LCD screens, and soon to be other devices.
pub mod perifs;
/// This module contains structures representing the pico's pins. This allows functions to take exact pins (so you won't be able to pass gpio25 (the internal LED) to the LCD)
pub mod pins;
