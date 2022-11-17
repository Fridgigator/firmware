#include <gtest/gtest.h>
#include <thread>
#include <vector>

#include "state/State.h"
#include "state/State.cpp"


TEST(StateTest, RunStateTest) {
  State t;
  deque<uint8_t> d;
  d.push_back(0);
  d.push_back(0);
  d.push_back(0);
  d.push_back(128);
  
  Reader r(d);
  t.push(r);
  vector<uint8_t> v = {1, 2, 4};
  EXPECT_EQ(t.getPacket(), v);
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
