use super::i2c::I2C;
use crate::pico::pins::Pin;
use core::{ffi::c_void, mem};
use rust_bridge::c_functions;

/// This struct represents an LCD2004 (which simply means that it's 20 blocks wide by 04 lines)
pub struct LCD2004a {
    ptr: *mut c_void,
}

impl LCD2004a {
    /// `from_i2c` creates an LCD object from an i2c.
    /// # Errors
    /// * `LCDError::WrongI2C` if it's given an incorrect i2c channel.
    /// * `LCDError::InitError` if it fails to initialize the device.
    /// * `LCDError::ErrCodeTooLarge` is an assertion error. If the library returns this, there's a bug in the library
    pub fn from_i2c<T: Pin, Q: Pin>(i2c: I2C<T, Q>) -> Result<LCD2004a, LCDError> {
        let mut err_code: i8 = -1;
        let ptr = unsafe {
            c_functions::c_lcd_init(
                i2c.get_num(),
                0x27,
                i2c.sda.get_pin(),
                i2c.scl.get_pin(),
                &mut err_code as *mut i8,
            )
        };
        mem::drop(i2c);
        match err_code {
            0 => Ok(LCD2004a { ptr }),
            c_functions::WRONG_I2C_ASSERT_ERROR => Err(LCDError::WrongI2C),
            c_functions::FAILED_TO_INIT_LCD_DEVICE => Err(LCDError::InitError),
            _ => Err(LCDError::ErrCodeTooLarge),
        }
    }

    /// This function displays the string.
    ///
    /// It takes an iterator as an argument. This allows the string to be displayed to be lazily generated.
    pub fn display<S: Iterator<Item = u8>>(&mut self, string: S) {
        unsafe { c_functions::c_lcd_clear(self.ptr) };
        let mut line = 0;
        let mut pos = 0;
        for c in string {
            unsafe {
                #[allow(clippy::cast_possible_truncation)]
                #[allow(clippy::cast_possible_wrap)]
                if c == b'\n' {
                    line += 1;
                    pos = 0;
                } else {
                    c_functions::c_lcd_putch(
                        self.ptr,
                        line,
                        if pos <= 128 { pos as i32 } else { 129 },
                        c,
                    );
                    pos += 1;
                }
            };
        }
    }
}

/// This represents an error returned from the lcd c library
pub enum LCDError {
    /// This variant is returned when the LCD could not be initialized (such as if the library has an incorrect I2C address).
    InitError,
    /// This is an assertion error.
    ErrCodeTooLarge,
    /// This variant is returned when there is no LCD plugged into the supplied I2C bus.
    WrongI2C,
}
