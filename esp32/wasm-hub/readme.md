# wasm-hub

## About

`wasm-hub` contains the business logic for the esp32 hub.
It has an interface that the esp32 framework can call, and
it can send data back to the esp32 via FFI.

**Safety**: This code assumes its running in single-threaded mode. All function
calls must be run from behind a mutex.


## Interface functions:
* `wasm_main()`: This is the entry point into wasm-hub. This function never returns.
* `sys_get_ble_data(data: *mut u8, size: usize)`: This function gets ble data from the environment. It passes a pointer to a free are in RAM 
 and the size of this free area and returns if there's more data waiting. Overwriting past the limit will result in undefined behavior!
* `sys_sleep(micros: u32)`: This function pauses the thread running the wasm interpreter in `ms` microseconds. 
* `sys_print(address: *const u8, size: usize)`: This function prints data in utf8, starting from `address` for size bytes.
* `fn sys_test_call()`: This does nothing. It can be used to benchmark the amount of time it takes just to cross the FFI barrier.
* `fn sys_get_time() -> u64`: This returns the current UNIX timestamp. 

## Building

To build a release build without debugging info, run `cargo build --target=wasm32-unknown-unknown --release` followed by 
`wasm-opt -Os -o target/wasm32-unknown-unknown/release/wasm_hub.wasm target/wasm32-unknown-unknown/release/wasm_hub.wasm --strip-debug` to 
further optimize the build.

To run it in the go environment, run:

1. `cd go_test`
2. `go build`
3. `./rt ../target/wasm32-unknown-unknown/release/wasm_hub.wasm`