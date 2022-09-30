#include "SendData.h"

void SendData(const std::string &remoteAddress, const SensorDataStore &store) {
  TaskHandle_t Task1;

  auto *sendTuple = new std::tuple<SensorDataStore, std::string>(store, remoteAddress);
  xTaskCreate([](void *arg) {
    auto [fInner, innerRemoteAddress] = *(std::tuple<SensorDataStore, std::string> *) (arg);
    Serial.printf("temperature Rec: (%d): %f\n", fInner.type, fInner.value);
    std::string url =
        ("/api/v1/send-data?data-type=temp&address=") + innerRemoteAddress + "&value=" + std::to_string(fInner.value)
            + "&timestamp=" + std::to_string(fInner.timestamp);
    Serial.printf("url: %s\n", url.c_str());

    std::map<std::string, std::string> headers;
    PostData(url, headers, {}, 0);
    delete (std::tuple<float, std::string> *) (arg);
    vTaskDelete(nullptr);
  }, "Sending Sensor Data", 16000, (void *) sendTuple, 1, &Task1);
}