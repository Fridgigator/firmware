use core::time::Duration;

use super::pins::{
    Pin0, Pin1, Pin10, Pin11, Pin12, Pin13, Pin14, Pin15, Pin16, Pin17, Pin18, Pin19, Pin2, Pin20,
    Pin21, Pin22, Pin25, Pin26, Pin27, Pin28, Pin3, Pin4, Pin5, Pin6, Pin7, Pin8, Pin9,
};

type Pins = (
    Pin0,
    Pin1,
    Pin2,
    Pin3,
    Pin4,
    Pin5,
    Pin6,
    Pin7,
    Pin8,
    Pin9,
    Pin10,
    Pin11,
    Pin12,
    Pin13,
    Pin14,
    Pin15,
    Pin16,
    Pin17,
    Pin18,
    Pin19,
    Pin20,
    Pin21,
    Pin22,
    Pin25,
    Pin26,
    Pin27,
    Pin28,
);

/// This structure represents the device as a whole. It can return all of the pins available and can be used to sleep for a set interval.
///
/// This itself is an important part of the library. Each pin is represented by a type, and different peripherals can take different pins.
/// For example, the pico doesn't support I2C channels on gpio22 or gpio34. To prevent this from being expressed in code, the I2C class only takes
/// arguments that are physically valid.
///
/// Another important point is to prevent accidentally using the same pin twice. For example, you don't want to flash an LED when it's already being flashed by another function.
/// Since pins don't implement Copy or Clone, they can only be obtained from Pico, and this should be done exactly once.
pub struct Pico {
    pins: Option<Pins>,
    pio: Option<(Pio, Pio)>,
}
unsafe impl Send for Pico {}

impl Pico {
    /// This initializes the device. You can get pins from it, but only once.
    /// # Safety
    /// - If you call this twice, you risk undefined behavior. See the module description for more details, but creating two devices and getting pins twice
    /// may result in a race condition when reading from or writing to peripherals.
    #[must_use]
    pub unsafe fn new() -> Self {
        Pico {
            pins: Option::Some((
                Pin0::new(),
                Pin1::new(),
                Pin2::new(),
                Pin3::new(),
                Pin4::new(),
                Pin5::new(),
                Pin6::new(),
                Pin7::new(),
                Pin8::new(),
                Pin9::new(),
                Pin10::new(),
                Pin11::new(),
                Pin12::new(),
                Pin13::new(),
                Pin14::new(),
                Pin15::new(),
                Pin16::new(),
                Pin17::new(),
                Pin18::new(),
                Pin19::new(),
                Pin20::new(),
                Pin21::new(),
                Pin22::new(),
                Pin25::new(),
                Pin26::new(),
                Pin27::new(),
                Pin28::new(),
            )),
            pio: Some((Pio { pio_num: 0 }, Pio { pio_num: 1 })),
        }
    }

    /// This function gets pins from the pico.
    ///
    /// This can be called once, so you only have one variable representing each pin, and each function that wants to interact with the device has to have a pin passed
    /// to it as an argument.
    pub fn get_pins(&mut self) -> Option<Pins> {
        self.pins.take()
    }

    /// This function returns the pio devices from the pico
    ///
    /// This can only be called once, and each pio device can load only one program
    pub fn get_pio(&mut self) -> Option<(Pio, Pio)> {
        self.pio.take()
    }

    /// This function puts the device to sleep for a specified duration
    ///
    /// Note, the device is fully on from the battery perspective.
    ///
    /// # TODO:
    /// Put it to sleep in a low power state, thus saving battery.
    #[allow(clippy::unused_self)]
    pub fn sleep(&mut self, ms: Duration) {
        unsafe {
            let amnt_ms = ms.as_millis().try_into().unwrap_or(u32::MAX);
            rust_bridge::c_functions::c_device_sleep(amnt_ms);
        }
    }
}

/// This represents one of the two available pio devices in the Raspberry Pi Pico
pub struct Pio {
    pio_num: u8,
}

impl Pio {
    /// This returns the pio number of the device
    #[must_use]
    pub fn get_num(&self) -> u8 {
        self.pio_num
    }
}
