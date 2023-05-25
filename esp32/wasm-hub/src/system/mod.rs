/// The safe wrappers around the FFI defined in sys.
pub mod safe_sys;
/// The FFI function definitions
pub mod sys;

pub use safe_sys::*;
