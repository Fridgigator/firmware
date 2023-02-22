# wasm-hub

## About

`wasm-hub` contains the business logic for the esp32 hub.
It has an interface that the esp32 framework can call, and
it can send data back to the esp32 via FFI.

**Safety**: This code assumes it's running in single-threaded mode. All function
calls must be run from behind a mutex.


## Interface functions:

Look up the documentation on the `system/sys.rs` file.
## Building

To build a release build without debugging info, run `cargo build --target=wasm32-unknown-unknown --release` followed by 
`wasm-opt -Os -o target/wasm32-unknown-unknown/release/wasm_hub.wasm target/wasm32-unknown-unknown/release/wasm_hub.wasm --strip-debug` to 
further optimize the build.

To run it in the go environment, run:

1. `cd go_test`
2. `go build`
3. `./rt ../target/wasm32-unknown-unknown/release/wasm_hub.wasm`