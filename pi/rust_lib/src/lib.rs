//! `rust_lib` is where the main business logic of the project is stored.
//!
//! Like in console applications, the main loop is in the main function. The [`pico`] module contains
//! libraries to interact with the pico and its peripherals.

#![allow(clippy::similar_names)]
#![cfg_attr(not(test), no_std)]
#![warn(missing_docs)]

use core::fmt::Write;
use core::time::Duration;
use heapless::String;

use pico::{
    device::Pico,
    perifs::{
        i2c::I2C,
        internal_led::InternalLED,
        ir::{PioInitError, IR},
        lcd2004a::LCD2004a,
    },
};

/// This module contains the libraries required to interact with the pico in a safe way.
pub mod pico;

/// This function is where the main program loop is located.
/// # Panics
/// - This should avoid panicking as much as possible.
/// - It panics if we try to get pins multiple times, as that would allow reading and writing from them in different places causing errors.
pub fn main() {
    let mut pico = unsafe { Pico::new() };

    // The following is only called once, so it shouldn't panic
    let (pio0, _pio1) = pico.get_pio().unwrap();
    let (
        pin0,
        pin1,
        _pin2,
        _pin3,
        pin4,
        pin5,
        _pin6,
        _pin7,
        pin8,
        pin9,
        _pin10,
        _pin11,
        pin12,
        pin13,
        pin14,
        _pin15,
        pin16,
        pin17,
        _pin18,
        _pin19,
        pin20,
        pin21,
        _pin22,
        pin25,
        _pin26,
        _pin27,
        _pin28,
    ) = pico.get_pins().unwrap();
    let mut led: InternalLED = pin25.into();
    let i2c = I2C::from_pins_4(
        pin4,
        pin5,
        (
            pin0, pin1, pin8, pin9, pin12, pin13, pin16, pin17, pin20, pin21,
        ),
    );
    let mut lcd_screen = match LCD2004a::from_i2c(i2c) {
        Ok(lcd_screen) => lcd_screen,
        Err(e) => match e {
            pico::perifs::lcd2004a::LCDError::InitError => panic!("InitError"),
            pico::perifs::lcd2004a::LCDError::ErrCodeTooLarge => panic!("ErrCodeTooLarge"),
            pico::perifs::lcd2004a::LCDError::WrongI2C => panic!("WrongI2C"),
        },
    };
    lcd_screen.display("This is a test\nline 2".as_bytes().iter().copied());

    let mut pio = match IR::new(pio0, pin14) {
        Ok(pio) => pio,
        Err(err) => match err {
            PioInitError::WrongPioDevice => loop {
                lcd_screen.display("Wrong Pio Device".as_bytes().iter().copied());
            },
            PioInitError::CannotInitPio => loop {
                lcd_screen.display("CannotInitPio".as_bytes().iter().copied());
            },
            PioInitError::SMTooLarge => loop {
                lcd_screen.display("Assert Error, sm too large".as_bytes().iter().copied());
            },
        },
    };

    // To show that it's doing something
    loop {
        let read = pio.read();
        read.map(|val| {
            if val.data != 0 {
                let mut s: String<50> = String::new();
                if write!(s, "{:x} {:x}", val.address, val.data).is_err() {
                    lcd_screen.display("Error writing values".as_bytes().iter().copied());
                }

                lcd_screen.display(s.as_bytes().iter().copied());
            };
            Some(())
        });
        blink(&mut pico, &mut led);
    }
}

/// This function turns the internal LED on and off every two seconds
pub fn blink(pico: &mut Pico, led: &mut InternalLED) {
    led.turn_on();
    pico.sleep(Duration::from_millis(1));
    led.turn_off();
    pico.sleep(Duration::from_millis(1));
}

/// This function runs whenever the program panics.
/// # TODO
/// I should blink an LED to a known pattern and try writing to the LCD if possible
pub fn panic() {
    #[allow(clippy::empty_loop)]
    loop {}
}
