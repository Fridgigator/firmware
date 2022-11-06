#include "SendData.h"

void SendData(const std::string &remoteAddress, const SensorDataStore &store) {
  Serial.printf("temperature Rec: (%d): %f\n", store.type, store.value);
  std::string url =
      ("/api/v1/send-data?data-type=temp&address=") + remoteAddress + "&value=" + std::to_string(store.value)
          + "&timestamp=" + std::to_string(store.timestamp);
  Serial.printf("url: %s\n", url.c_str());

  std::map<std::string, std::string> headers;
  PostData(url, headers, {}, 0);
}