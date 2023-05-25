use core::time::Duration;

use futures::Future;

use crate::{libs::time::Time, system::Ffi};

/// Sleeps for a given amount of time
pub struct Sleep<F: 'static + Ffi, G> {
    wait_for_time: Time,
    /// If sleep needs to be cancelled by another thread, have the function return true
    cancel: fn() -> (bool, G),
    ffi: &'static F,
}

impl<F: Ffi, G> Sleep<F, G> {
    /// Sleeps for a period of time. It yields control back to the RTOS so that the WDT doesn't complain and so that the OS can
    /// do other tasks.
    ///
    /// # Arguments
    ///
    /// * esp32: An [Ffi]. This allows for dependency injection and better testing
    /// * dur: How long to sleep
    /// * cancel: If sleep needs to be cancelled by another thread, have the function return true. Otherwise, it should return false.
    pub fn new(esp32: &'static F, dur: Duration, cancel: fn() -> (bool, G)) -> Self {
        Self {
            wait_for_time: esp32.get_time() + dur,
            cancel,
            ffi: esp32,
        }
    }
}

/// [Sleep] can return one of three values
#[must_use]
pub enum Return<F> {
    /// Sleep finished normally. It's just time to wake up
    None,
    /// Sleep was cancelled. There may be a reason for this.
    Cancelled(F),
}

impl<F: Ffi, G> Future for Sleep<F, G> {
    type Output = Return<G>;

    fn poll(
        self: core::pin::Pin<&mut Self>,
        _cx: &mut core::task::Context<'_>,
    ) -> core::task::Poll<Self::Output> {
        let (c, reason) = (self.cancel)();
        let cur_time = self.ffi.get_time();
        if c {
            core::task::Poll::Ready(Return::Cancelled(reason))
        } else if cur_time > self.wait_for_time {
            core::task::Poll::Ready(Return::None)
        } else {
            core::task::Poll::Pending
        }
    }
}
