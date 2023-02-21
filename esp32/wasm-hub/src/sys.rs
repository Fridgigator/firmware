extern "C" {
    #[allow(dead_code)]
    pub fn sys_get_ble_data(data: *mut u8, size: usize) -> bool;
    pub fn sys_sleep(micros: u32);
    pub fn sys_print(address: *const u8, size: usize);
    pub fn sys_get(address: *mut u8, size: usize);
    pub fn sys_test_call();
    #[allow(dead_code)]
    pub fn sys_get_time() -> u64;
}
