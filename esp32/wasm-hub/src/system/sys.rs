extern "C" {
    pub fn sys_send_message(msg: i64);
    pub fn sys_get_websocket_data(data: *mut u8, size: usize) -> u8;
    pub fn sys_sleep(micros: u64);
    pub fn sys_print(address: *const u8, size: usize);
    pub fn sys_get(address: *mut u8, size: usize);
    pub fn sys_test_call();
    pub fn sys_get_time() -> u64;
    pub fn sys_set_led(state: bool, which: u8);
}
