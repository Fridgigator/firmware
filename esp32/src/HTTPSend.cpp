#include <HTTPClient.h>
#include <variant>
#include "HTTPSend.h"
#include "Constants.h"
#include "generated/FirmwareBackend.pb.h"
#include "spm_headers/pb_decode.h"
BackendToFirmwarePacket PostData(std::string &url, std::map<std::string, std::string> &headers, uint8_t *sendData, size_t size) {
  HTTPClient http_client;
  std::string fullUrl = std::string("https://fridgigator.herokuapp.com") + url;
  http_client.begin(fullUrl.c_str(), rootCACertificate);
  for(auto [key, val]: headers) {
    http_client.addHeader(key.c_str(), val.c_str());
  }

  int res = http_client.POST(sendData, size);
  if(res != 200) {
    Serial.println("Result is not 200");
    throw "Wrong return value";
  }

  int val;
  auto wifiStream = http_client.getStream();
  std::vector<uint8_t> data;
  while((val = wifiStream.read()) != -1){
    data.emplace_back(val);
  }
  BackendToFirmwarePacket message;
  message = BackendToFirmwarePacket_init_zero;
  pb_istream_t stream = pb_istream_from_buffer(data.data(), data.size());

  bool status = pb_decode(&stream, BackendToFirmwarePacket_fields, &message);
  if(!status){
    Serial.println("Stream decode bug");
    throw "Stream decode bug";
  }
  return message;
}

BackendToFirmwarePacket GetData(std::string &url, std::map<std::string, std::string> &headers) {
  HTTPClient http_client;
  std::string fullUrl = std::string("https://fridgigator.herokuapp.com") + url;
  http_client.begin(fullUrl.c_str(), rootCACertificate);

  for(auto [key, val]: headers) {
    http_client.addHeader(key.c_str(), val.c_str());
  }

  int res = http_client.GET();
  auto wifiStream = http_client.getStream();
  Serial.printf("url = %s, res = %d\n",fullUrl.c_str(), res);
  if(res != 200) {
    Serial.println("Result is not 200");
    throw "Wrong return value";
  }
  std::vector<uint8_t> data;

  for(auto val: http_client.getString()){
    data.emplace_back(val);
  }
  BackendToFirmwarePacket message;
  message = BackendToFirmwarePacket_init_zero;
  pb_istream_t stream = pb_istream_from_buffer(data.data(), data.size());

  bool status = pb_decode(&stream, BackendToFirmwarePacket_fields, &message);
  if(!status){
    Serial.println("Stream decode bug");
    throw "Stream decode bug";
  }
  return message;
}