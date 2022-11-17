#include <gtest/gtest.h>
#include <thread>
#include <vector>

#include "lib/mutex.h"

TEST(MutexTest, EnsureLock) {
  safe_std::mutex<int> mux(0);
  std::vector<std::thread> v;
  for (int i = 0; i < 11; i++) {
    std::thread t([&mux]() {
      for (int i = 0; i < 10000; i++) {
        auto v = mux.lock();
        int a = *v;
        a++;
        *v = a;
      }
    });
    v.emplace_back(std::move(t));
  }
  for (auto &t : v) {
    t.join();
  }

  EXPECT_EQ(*mux.lock(), 110000);
}

TEST(MutexTest, LockAndSwap) {
  safe_std::mutex<int *> mux(new int(5));
  std::vector<std::thread> v;
  for (int i = 0; i < 11; i++) {
    std::thread t([&mux]() {
      for (int i = 0; i < 10000; i++) {
        delete mux.lockAndSwap(new int(6));
      }
    });
    v.emplace_back(std::move(t));
  }
  for (auto &t : v) {
    t.join();
  }

  EXPECT_EQ(**mux.lock(), 6);
  delete mux.lockAndSwap(nullptr);
}

#if defined(ARDUINO)
#include <Arduino.h>

void setup() {
  // should be the same value as for the `test_speed` option in "platformio.ini"
  // default value is test_speed=115200
  Serial.begin(115200);

  ::testing::InitGoogleTest();
  // if you plan to use GMock, replace the line above with
  // ::testing::InitGoogleMock();
}

void loop() {
  // Run tests
  if (RUN_ALL_TESTS());

  // sleep for 1 sec
  delay(1000);
}

#else
int main(int argc, char **argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    // if you plan to use GMock, replace the line above with
    // ::testing::InitGoogleMock(&argc, argv);

    if (RUN_ALL_TESTS())
    ;

    // Always return zero-code and allow PlatformIO to parse results
    return 0;
}
#endif
