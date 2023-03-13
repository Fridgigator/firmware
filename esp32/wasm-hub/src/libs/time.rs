use core::{
    ops::{Add, Sub},
    time::Duration,
};

/// A representation of time
/// 
/// It is simply a wrapper around Duration (for now), allowing it to be small and fast. The difference is that it's meant to represent a time period since 
/// the unix epoch.
#[derive(Debug, PartialEq, Eq, PartialOrd, Ord, Clone, Copy)]
pub struct Time {
    time_since_epoch: Duration,
}

impl Time {
    /// Creates a new time object
    /// 
    /// This takes a duration since the Unix epoch (1-1-1970). 
    #[must_use]
    pub const fn new(time_since_epoch: Duration) -> Self {
        Self { time_since_epoch }
    }
}

impl Sub<Time> for Time {
    type Output = Duration;

    fn sub(self, rhs: Time) -> Self::Output {
        self.time_since_epoch
            .checked_sub(rhs.time_since_epoch)
            .unwrap()
    }
}

impl Add<Duration> for Time {
    type Output = Time;

    fn add(self, rhs: Duration) -> Self::Output {
        Time::new(self.time_since_epoch + rhs)
    }
}
