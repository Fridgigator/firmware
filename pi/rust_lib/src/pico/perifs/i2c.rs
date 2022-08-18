#![allow(clippy::similar_names)]

use crate::pico::pins::{
    Pin, Pin0, Pin1, Pin10, Pin11, Pin12, Pin13, Pin14, Pin15, Pin16, Pin17, Pin18, Pin19, Pin2,
    Pin20, Pin21, Pin26, Pin27, Pin3, Pin4, Pin5, Pin6, Pin7, Pin8, Pin9,
};
use paste::paste;

/// This struct represents an I2C bus. It can be created from pins and then can be used to create other peripherals that are attached to the bus.
///
/// It can be made by calling a `from_pins_#` function, where the first number is that of the sda pin.
/// The other pins must also be passed in so they won't be accidentally used somewhere else.
///
/// Since there are two I2C busses in the pico, it's possible to create only two I2C objects before running out of pins.
pub struct I2C<P1, P2> {
    pub(super) sda: P1,
    pub(super) scl: P2,
}

macro_rules! make_I2C {
    ($num:literal, $sda:ident, $scl:ident, ($other_pin1:ident, $other_pin2:ident, $other_pin3:ident, $other_pin4:ident, $other_pin5:ident, $other_pin6:ident, $other_pin7:ident, $other_pin8:ident, $other_pin9:ident, $other_pin10:ident)) => {
        impl I2C<$sda, $scl> {
            paste! {
                /// This function allows creating an I2C device from given pins.
                /// It consumes the other pins that are on the same I2C channel so they won't be mistakenly used somewhere else.
                #[must_use]
                pub fn [<from_pins_ $num>](
                    sda: $sda,
                    scl: $scl,
                    other_pins: (
                        $other_pin1,
                        $other_pin2,
                        $other_pin3,
                        $other_pin4,
                        $other_pin5,
                        $other_pin6,
                        $other_pin7,
                        $other_pin8,
                        $other_pin9,
                        $other_pin10,
                    ),
                ) -> Self {
                    drop(other_pins);
                    I2C { sda, scl }
                }

            }
        }
    };
}

impl<P1: Pin, P2: Pin> I2C<P1, P2> {
    pub(super) fn get_num(&self) -> u8 {
        match self.sda.get_pin() {
            0 | 4 | 8 | 12 | 16 | 21 => 0,
            2 | 6 | 10 | 14 | 18 | 26 => 1,
            _ => unreachable!(),
        }
    }
}

make_I2C!(
    0,
    Pin0,
    Pin1,
    (Pin4, Pin5, Pin8, Pin9, Pin12, Pin13, Pin16, Pin17, Pin20, Pin21)
);
make_I2C!(
    4,
    Pin4,
    Pin5,
    (Pin0, Pin1, Pin8, Pin9, Pin12, Pin13, Pin16, Pin17, Pin20, Pin21)
);
make_I2C!(
    8,
    Pin8,
    Pin9,
    (Pin0, Pin1, Pin4, Pin5, Pin12, Pin13, Pin16, Pin17, Pin20, Pin21)
);
make_I2C!(
    12,
    Pin12,
    Pin13,
    (Pin0, Pin1, Pin4, Pin5, Pin8, Pin9, Pin16, Pin17, Pin20, Pin21)
);
make_I2C!(
    16,
    Pin16,
    Pin17,
    (Pin0, Pin1, Pin4, Pin5, Pin8, Pin9, Pin12, Pin13, Pin20, Pin21)
);
make_I2C!(
    21,
    Pin21,
    Pin20,
    (Pin0, Pin1, Pin4, Pin5, Pin8, Pin9, Pin12, Pin13, Pin16, Pin17)
);

make_I2C!(
    2,
    Pin2,
    Pin3,
    (Pin6, Pin7, Pin10, Pin11, Pin14, Pin15, Pin18, Pin19, Pin26, Pin27)
);

make_I2C!(
    6,
    Pin6,
    Pin7,
    (Pin2, Pin3, Pin10, Pin11, Pin14, Pin15, Pin18, Pin19, Pin26, Pin27)
);

make_I2C!(
    10,
    Pin10,
    Pin11,
    (Pin2, Pin3, Pin6, Pin7, Pin14, Pin15, Pin18, Pin19, Pin26, Pin27)
);

make_I2C!(
    14,
    Pin14,
    Pin15,
    (Pin2, Pin3, Pin6, Pin7, Pin10, Pin11, Pin18, Pin19, Pin26, Pin27)
);

make_I2C!(
    18,
    Pin18,
    Pin19,
    (Pin2, Pin3, Pin6, Pin7, Pin10, Pin11, Pin14, Pin15, Pin26, Pin27)
);

make_I2C!(
    26,
    Pin26,
    Pin27,
    (Pin2, Pin3, Pin6, Pin7, Pin10, Pin11, Pin14, Pin15, Pin18, Pin19)
);
