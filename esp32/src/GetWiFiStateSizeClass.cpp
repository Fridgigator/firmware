#include <memory>
#include <iostream>
#include <cstring>
#include "GetWiFiStateSizeClass.h"
#include "generated/packet.pb.h"
#include "pb_encode.h"
#include "DecodeException.h"
using namespace std;
GetWiFiStateSizeClass::GetWiFiStateSizeClass(WiFiState wifi) {
  auto wifiResponsePacket = make_unique<WiFiConnectResponseInfo>();
  *wifiResponsePacket = WiFiConnectResponseInfo_init_zero;
  auto *buf = new pb_byte_t[2048];
  switch (wifi) {
    case CONNECTING: {
      wifiResponsePacket->which_type = WiFiConnectResponseInfo_connectingToWiFi_tag;
      wifiResponsePacket->type = {};
      break;
    }
    case TIMEOUT: {
      wifiResponsePacket->which_type = WiFiConnectResponseInfo_timeout_tag;
      wifiResponsePacket->type = {};
      break;
    }
    case CONNECTED: {
      wifiResponsePacket->which_type = WiFiConnectResponseInfo_connectedToWiFi_tag;
      wifiResponsePacket->type = {};
      break;
    }
  }

  pb_ostream_t output = pb_ostream_from_buffer(buf, 2048);

  int status = pb_encode(&output, WiFiConnectResponseInfo_fields, wifiResponsePacket.get());
  if (!status) {
    cerr << "Encoding failed:" << PB_GET_ERROR(&output) << endl;
    throw DecodeException();
  }

  for (int i = 0; i < output.bytes_written; i++) {
    returnData.push_back(buf[i]);
  }
  delete[] buf;
}

vector<uint8_t> GetWiFiStateSizeClass::getSize() const {
  unsigned int size = returnData.size();
  uint8_t sizeArray[4];
  std::memcpy(sizeArray, &size, sizeof(sizeArray));
  return {sizeArray, sizeArray + 4};
}

optional<vector<uint8_t>> GetWiFiStateSizeClass::getData(int amnt) {
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
bool GetWiFiStateSizeClass::isEmpty() const {
  return returnData.empty();
}

