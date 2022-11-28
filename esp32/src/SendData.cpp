#include "SendData.h"

void SendData(const std::string &remoteAddress, const SensorDataStore &store) {
  Serial.printf("temperature Rec: (%d): %f\n", store.type, store.value);
  std::string measure_type;
  switch (store.measure_type) {
    case MeasureType::TEMP:measure_type = "temp";
      break;
    case MeasureType::HUMIDITY:measure_type = "humidity";
      break;
    case DHT11_TEMP:measure_type = "dht11_temp";
      break;
    case DHT22_TEMP:
      measure_type = "dht22_temp";
      break;
    case DHT11_HUMIDITY:
      measure_type = "dht11_humidity";
      break;
    case DHT22_HUMIDITY:
      measure_type = "dht22_humidity";
      break;
    case PICO_TEMP:
      measure_type = "pico_temp";
      break;
  }
  std::string url =
      ("/api/v1/send-data?address=") + remoteAddress + "&value=" + std::to_string(store.value)
          + "&timestamp=" + std::to_string(store.timestamp) + "&data-type=" + measure_type;
  Serial.printf("url: %s\n", url.c_str());

  std::map<std::string, std::string> headers;
  PostData(url, headers, {}, 0);
}