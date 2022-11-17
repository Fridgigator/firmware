#include "SendWifiDataClass.h"
#include "generated/packet.pb.h"
#include "exceptions/DecodeException.h"
#include "pb_encode.h"
#include <memory>
#include <cstring>
#include <optional>
#include "lib/log.h"

using namespace std;

SendWifiDataClass::SendWifiDataClass(vector<WiFiStorage> &wifi) {
  vector<WiFiStorage> _wifi;

  auto p = make_unique<WiFiVector>();
  p->data_count = 0;
  auto *buf = new pb_byte_t[2048];
  pb_ostream_t output = pb_ostream_from_buffer(buf, 2048);
  vector<WiFiData> payload;
  pb_size_t sizeOfPacketVec = sizeof p->data / sizeof p->data[0];
  for (int i = 0; i < sizeOfPacketVec && i < wifi.size(); i++) {
    auto d = wifi.at(i);
    WiFiData wData = WiFiData_init_zero;
    wData = WiFiData{.bssid = (uint64_t) d.BSSID[0] | ((uint64_t) d.BSSID.at(1) << 8) |
        ((uint64_t) d.BSSID.at(2) << 16) | ((uint64_t) d.BSSID.at(3) << 24) |
        ((uint64_t) d.BSSID.at(4) << 32) | ((uint64_t) d.BSSID.at(5)
        << 40), .Channel = static_cast<uint32_t>(d.Channel), .isEncrypted = d.isEncrypted};
    strncpy(wData.ssid, d.SSID.c_str(), std::min(d.SSID.size(), sizeof(wData.ssid) - 1));
    payload.push_back(wData);
  }

  p->data_count = static_cast<pb_size_t>(std::min((unsigned int) sizeOfPacketVec,
                                                  (unsigned int) wifi.size()));
  for (int i = 0; i < sizeOfPacketVec && i < payload.size(); i++) {
    p->data[i] = payload.at(i);
  }
  int status = pb_encode(&output, WiFiVector_fields, p.get());
  if (!status) {
    LOG("Encoding failed: %s\n", PB_GET_ERROR(&output));
    throw DecodeException();
  }
  for (int i = 0; i < output.bytes_written; i++) {
    returnData.push_back(buf[i]);
  }
  delete[] buf;
}

vector<uint8_t> SendWifiDataClass::getSize() const {
  unsigned int size = returnData.size();
  uint8_t sizeArray[4];
  std::memcpy(sizeArray, &size, sizeof(sizeArray));
  return {sizeArray, sizeArray + 4};
}

optional<vector<uint8_t>> SendWifiDataClass::getData(int amnt) {
  if (returnData.empty()) {
    return nullopt;
  }
  vector<uint8_t> returnVal;
  returnVal.reserve(amnt);
  while (returnVal.size() <= amnt && !returnData.empty()) {
    returnVal.emplace_back(returnData.front());
    returnData.pop_front();
  }
  return returnVal;
}
bool SendWifiDataClass::isEmpty() {
  return returnData.empty();
}
