#include <Arduino.h>
#include "WebDataPollingClient.h"
#include "HTTPSend.h"

namespace WebData {

WebDataPollingClient::WebDataPollingClient(std::string &url,
                                           const std::map<std::string, std::string> &headers) {
  this->headers = headers;
  this->url = url;
}

void WebDataPollingClient::onRecData(const std::function<void(BackendToFirmwarePacket)> &_callback) {
  this->callback = _callback;
}
void WebDataPollingClient::poll() {
  auto d = GetData(url, headers);
  if (d.index() == 1) {
    return;
  } else {
    auto d1 = std::get<0>(d);
    if (d1.which_type == BackendToFirmwarePacket_ack_tag) {
      return;
    }
    Serial.printf(" - message type=%d\n", d1.which_type);
    if (callback.has_value()) {
      callback.value()(d1);
    }
  }
}
}