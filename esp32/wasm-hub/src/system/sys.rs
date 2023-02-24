//! This module contains the unsafe function prototypes for the FFI.
//!
//! These functions should have a safe implemetations in [crate::system::safe_sys].

extern "C" {
    /// Sends a quick and cheap message to the wasm runtime.
    ///
    /// This is cheap as it doesn't require copying large strings across the wire, and this saves memory
    /// as the wasm doesn't have to hold large uncompressed strings.
    ///
    /// # Arguments
    ///
    /// * `msg`: One of:
    ///   * 0: [crate::system::FFIMessage::GenericError]
    ///   * 1: [crate::system::FFIMessage::TooMuchData]
    ///   * 2: [crate::system::FFIMessage::TryFromIntError]
    ///   * 3: [crate::system::FFIMessage::PanicErr]
    ///   * 4: [crate::system::FFIMessage::TooManySensors]
    ///   * 5: [crate::system::FFIMessage::AssertWrongModelType]
    ///
    pub fn sys_send_message(msg: i64);

    /// Gets raw data from the websocket.
    ///
    /// The wasm runtime should hold a cache of commands coming from the server. We poll the cache one at a time,
    /// pulling in data.
    ///
    /// # Arguments
    ///
    /// * `data`: `*mut u8`. A pointer to where the runtime should write the data
    /// * `size`: How much space was allocated for the data. Overflowing this can cause undefined behavior!
    ///
    /// # Returns:
    /// * `u8`: a bitfield
    ///
    /// | Bit (Least significant bit is 0 | Description |
    /// |---------------------------------|------------|
    /// | 0 | There's more data to poll. This program will poll it right away without waiting|
    /// | 1 | The data packet being polled is too large to fit into `size`. This data will be thrown away|
    ///
    pub fn sys_get_websocket_data(data: *mut u8, size: usize) -> u8;

    /// Puts the thread to sleep for micros microseconds. It will also allow the RTOS to execute other
    /// important threads (like BLE and WiFi)
    ///
    /// # Arguments
    ///
    /// * `micros`: Time to sleep in microseconds
    ///
    ///
    pub fn sys_sleep(micros: u64);

    /// Print text to the console
    ///
    ///
    /// # Arguments
    ///
    /// * `address`: The starting address of a byte array. It's not null terminating so you can't just call printf
    /// * `size`: The size of the byte array
    ///
    ///
    /// # Examples
    /// ```
    /// let text = "abc😀";
    /// unsafe {
    ///   sys_print(text.as_ptr(), text.bytes().len());
    /// }
    /// ```
    pub fn sys_print(address: *const u8, size: usize);

    /// Gets the current system time (in UNIX timestamp). Will return -1 if it's not set.
    ///
    ///
    /// returns: u64, a UNIX timestamp
    ///
    pub fn sys_get_time() -> u64;

    /// Turns on and off a LED attached to a GPIO.
    ///
    /// # Arguments
    ///
    /// * `state`: true (1) for on, false (0) for off
    /// * `which`: Specifies the GPIO pin number
    ///
    pub fn sys_set_led(state: bool, which: u8);

    /// Starts a remote device scan
    ///
    /// Once the scan is started, we should poll the runtime to get scan details
    pub fn sys_start_remote_device_scan();

    /// Once inside a remote device scan, poll a device from the runtime
    ///
    /// # Arguments
    ///
    /// * `address`: The BLE address of the remote device. Must be exactly 6 bytes long. Overwriting or underwriting this will lead to undefined behavior.
    /// * `name`: The human readable name of the remote device. Overwriting this will lead to undefined behavior.
    /// * `name_size`: The length allocated to name
    ///
    /// Returns 0 if successfully read, 1 if no data available
    ///
    pub fn sys_get_device_from_scan(address: *const u8, name: *const u8, name_size: usize) -> u8;

    /// Stops a remote device scan
    ///
    /// This ends the remote device scan
    pub fn sys_stop_remote_device_scan();
}
