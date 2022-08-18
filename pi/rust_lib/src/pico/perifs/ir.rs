use core::mem;

use crate::pico::device::Pio;
use crate::pico::pins::Pin;

/// This represents an IR receiver
pub struct IR {
    pio: Pio,
    sm: u8,
}
impl IR {
    /// This initialized an IR receiver, from which we can read later.
    /// # Returns
    /// It either returns a valid IR receiver or, on failure, a [`PioInitError`].
    /// # Errors
    /// [`PioInitError`]
    pub fn new<T: Pin>(pio: Pio, pin: T) -> Result<Self, PioInitError> {
        let pin_num = pin.get_pin();
        // We don't need the pin anymore
        mem::drop(pin);
        let return_val = unsafe { rust_bridge::c_functions::c_register_ir(pio.get_num(), pin_num) };
        match return_val {
            rust_bridge::c_functions::WRONG_PIO_DEVICE => Err(PioInitError::WrongPioDevice),
            rust_bridge::c_functions::CANNOT_INIT_PIO => Err(PioInitError::CannotInitPio),
            sm => match sm.try_into() {
                Ok(sm) => Ok(IR { pio, sm }),
                Err(_) => Err(PioInitError::SMTooLarge),
            },
        }
    }

    /// This reads a value from the IR.
    /// # Returns
    /// If there is no value to be read, it returns None, otherwise it returns an [`IRReturn`] struct.
    pub fn read(&mut self) -> Option<IRReturn> {
        let frame = unsafe { rust_bridge::c_functions::c_read_ir(self.pio.get_num(), self.sm) };
        if frame.error == 0 {
            Some(IRReturn {
                address: frame.p_address,
                data: frame.p_data,
            })
        } else {
            None
        }
    }
}

/// This struct represents a value returned from an IR reader.
pub struct IRReturn {
    /// This represents the address of the sending IR device. On my remote control, it's always 0.
    pub address: u8,
    /// This represents the data the IR device sends.
    pub data: u8,
}

/// This is returned when the PIO device is unable to be initiated
pub enum PioInitError {
    /// This is an assertion error. If this is returned, it means that the C code was passed an invalid PIO number (less than 0 or more than 1).
    WrongPioDevice,
    /// This is returned if the PIO cannot be initiated.
    CannotInitPio,
    /// This is an assertion error. SM should be one byte long, so if the original function returned too large of a value, we get an error
    SMTooLarge,
}
