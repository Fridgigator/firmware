#ifndef ESP32_SRC_WEBDATAPOLLINGCLIENT_H_
#define ESP32_SRC_WEBDATAPOLLINGCLIENT_H_

#include <map>
#include <thread>
#include <variant>
#include "generated/FirmwareBackend.pb.h"

namespace WebData {

class WebDataPollingClient {
 private:
  std::optional<std::function<void(BackendToFirmwarePacket)>> callback;
  std::map<std::string, std::string> headers;
  std::string url;
 public:
  WebDataPollingClient(std::string& url, const std::map<std::string, std::string> &headers);
  void onRecData(const std::function<void(BackendToFirmwarePacket)>& callback);
  void poll();
};
}

#endif //ESP32_SRC_WEBDATAPOLLINGCLIENT_H_
