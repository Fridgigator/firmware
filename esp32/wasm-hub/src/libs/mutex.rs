/// Built with oreilly.com/library/view/rust-atomics-and/978109119430/ch04.html
use core::{
    cell::UnsafeCell,
    ops::{Deref, DerefMut},
    sync::atomic::{AtomicBool, Ordering},
    time::Duration,
};

use crate::system::Ffi;

use super::sleep::Sleep;

/// Mutex is a spinlock based mutex structure. It's designed to work in async land and uses no_std.
/// I don't care about efficiency since it should preferably never be contended.

pub struct Mutex<T> {
    data: UnsafeCell<T>,
    lock: AtomicBool,
}

pub struct MutexGuard<'a, T> {
    mutex: &'a Mutex<T>,
}
unsafe impl<T> Sync for Mutex<T> {}

impl<T> Mutex<T> {
    /// new creates a new Mutex
    pub fn new(data: T) -> Self {
        Self {
            data: UnsafeCell::new(data),
            lock: AtomicBool::new(false),
        }
    }
    /// lock locks the mutex, blocking until it's available
    pub async fn lock<F: Ffi>(&self, ffi: &'static F) -> MutexGuard<'_, T> {
        loop {
            // if we can lock the mutex, return a guard
            if !self.lock.swap(true, Ordering::Acquire) {
                return MutexGuard { mutex: self };
            } else {
                // if we can't lock the mutex, yield and try again
                let _ = Sleep::new(ffi, Duration::from_millis(1), || (false, ())).await;
            }
        }
    }
}

impl<'a, T> Deref for MutexGuard<'a, T> {
    type Target = T;

    fn deref(&self) -> &Self::Target {
        //Safety: We own this mutex guard, we have exclusive control over the data
        unsafe { &*self.mutex.data.get() }
    }
}

impl<'a, T> DerefMut for MutexGuard<'a, T> {
    fn deref_mut(&mut self) -> &mut T {
        //Safety: We own this mutex guard, we have exclusive control over the data
        unsafe { &mut *self.mutex.data.get() }
    }
}
impl<'a, T> Drop for MutexGuard<'a, T> {
    fn drop(&mut self) {
        self.mutex.lock.store(false, Ordering::Release);
    }
}
