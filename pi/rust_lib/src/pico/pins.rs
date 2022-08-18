use core::ffi::c_void;
macro_rules! make_pin {
    ($name:ident, $num:literal) => {
        /// This represents a pin
        pub struct $name {
            _secret: Option<*mut c_void>, // This is to ensure that it won't be Send or Sync
        }
        impl Pin for $name {
            fn get_pin(&self) -> u8 {
                return $num;
            }
        }
        impl $name {
            pub(super) fn new() -> Self {
                return Self { _secret: None };
            }
        }
    };
}

/// All pins have to implement this interface so that other functions would be able to easily get the gpio pin it represents
pub trait Pin {
    /// This function returns the gpio number of a given pin
    fn get_pin(&self) -> u8;
}

make_pin!(Pin0, 0);
make_pin!(Pin1, 1);
make_pin!(Pin2, 2);
make_pin!(Pin3, 3);
make_pin!(Pin4, 4);
make_pin!(Pin5, 5);
make_pin!(Pin6, 6);
make_pin!(Pin7, 7);
make_pin!(Pin8, 8);
make_pin!(Pin9, 9);
make_pin!(Pin10, 10);
make_pin!(Pin11, 11);
make_pin!(Pin12, 12);
make_pin!(Pin13, 13);
make_pin!(Pin14, 14);
make_pin!(Pin15, 15);
make_pin!(Pin16, 16);
make_pin!(Pin17, 17);
make_pin!(Pin18, 18);
make_pin!(Pin19, 19);
make_pin!(Pin20, 20);
make_pin!(Pin21, 21);
make_pin!(Pin22, 22);
make_pin!(Pin25, 25);
make_pin!(Pin26, 26);
make_pin!(Pin27, 27);
make_pin!(Pin28, 28);
