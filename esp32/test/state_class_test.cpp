#include <gtest/gtest.h>
#include "generated/packet.pb.h"
#include "State.h"

TEST(MutexTest, EnsureLock) {
  BLESendPacket packet = BLESendPacket::default_instance();
  packet.set_allocated_getwifi(new GetWiFi());
  std::string out = packet.SerializeAsString();
  State s;
  s.push();
  EXPECT_EQ( 110000, 110000);
}
