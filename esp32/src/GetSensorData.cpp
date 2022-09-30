#include "GetSensorData.h"
#include "BLEUtils.h"
#include "HTTPSend.h"
#include "Constants.h"

#include <mutex>
#include <thread>

static BLEUUID serviceNordicUUID("ef680200-9b35-4933-9b10-52ffa9740042");
static BLEUUID serviceTIUUID("f000aa00-0451-4000-b000-000000000000");
static BLEUUID serviceCustomUUID("00000000-0000-4000-0000-000000000000");

static BLEUUID charNordicUUID("ef680201-9b35-4933-9b10-52ffa9740042");
static BLEUUID charCustomUUID("00000000-0000-4000-0000-000000000000");
static BLEUUID charTISendUUID("f000aa02-0451-4000-b000-000000000000");
static BLEUUID charTIRecUUID("f000aa01-0451-4000-b000-000000000000");

static BLERemoteCharacteristic *pRemoteNordicCharacteristic = nullptr;
static BLERemoteCharacteristic *pRemoteTIWriteCharacteristic = nullptr;
static BLERemoteCharacteristic *pRemoteReadCharacteristic = nullptr;

unsigned long getTime() {
  time_t now;
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    //Serial.println("Failed to obtain time");
    return(0);
  }
  time(&now);
  return now;
}

unsigned long lastGotData = 0;
std::mutex lastGotDataMtx;

template<typename T>
bool contains(std::vector<T> &v, T key) {
  for (const auto &e : v) {
    if (e == key) {
      return true;
    }
  }
  return false;
}

void notifyNordicCallback(
    BLERemoteCharacteristic *pBLERemoteCharacteristic,
    const uint8_t *pData,
    size_t length,
    bool isNotify) {
  Serial.print("Notify callback for characteristic ");
  Serial.print(pBLERemoteCharacteristic->getUUID().toString().c_str());
  Serial.print(" of data length ");
  Serial.println(length);
  Serial.print("data: ");
  int low = pData[0];
  int high = pData[1];
  std::string
      remoteAddress = pBLERemoteCharacteristic->getRemoteService()->getClient()->getConnInfo().getAddress().toString();
  auto *sendTuple = new std::tuple<int, int, std::string>(low, high, remoteAddress);
  TaskHandle_t Task1;
  lastGotDataMtx.lock();
  lastGotData = getTime();
  lastGotDataMtx.unlock();

  xTaskCreate([](void *arg) {
    auto [innerLow, innerHigh, innerRemoteAddress] = *(std::tuple<int, int, std::string> *) (arg);
    Serial.printf("addr: %p\n", &innerLow);
    Serial.printf("temperature (Nordic): %d.%d\n", innerLow, innerHigh);

    PostData((std::string) ("/api/send-data?data-type=temp&address=") + innerRemoteAddress + "&value="
                 + std::to_string(innerLow) + "." + std::to_string(innerHigh),
             std::map<std::string, std::string>(),
             {},
             0);
    delete (std::tuple<int, int, std::string> *) (arg);
    vTaskDelete(nullptr);
  }, "Sending Sensor Data", 16000, (void *) sendTuple, 1, &Task1);

}

void notifyTICallback(
    BLERemoteCharacteristic *pBLERemoteCharacteristic,
    uint8_t *pData,
    size_t length,
    bool isNotify) {
  Serial.print("Notify callback for characteristic ");
  Serial.print(pBLERemoteCharacteristic->getUUID().toString().c_str());
  std::string
      remoteAddress = pBLERemoteCharacteristic->getRemoteService()->getClient()->getConnInfo().getAddress().toString();
  Serial.print(" of data length ");
  Serial.println(length);
  assert(length == 4);
  static_assert(sizeof(float) == 4, "float size is expected to be 4 bytes");
  float f;
  memcpy(&f, pData, 4);
  Serial.printf("addr: %p\n", f);
  TaskHandle_t Task1;

  lastGotDataMtx.lock();
  lastGotData = getTime();
  lastGotDataMtx.unlock();

  auto *sendTuple = new std::tuple<float, std::string>(f, remoteAddress);
  xTaskCreate([](void *arg) {
    auto [fInner, innerRemoteAddress] = *(std::tuple<float, std::string> *) (arg);
    Serial.printf("temperature (TI): %f\n", fInner);
    std::string url =
        ("/api/send-data?data-type=temp&address=") + innerRemoteAddress + "&value=" + std::to_string(fInner);
    Serial.printf("url: %s\n", url.c_str());

    std::map<std::string, std::string> headers;
    PostData(url, headers, {}, 0);
    delete (std::tuple<float, std::string> *) (arg);
    vTaskDelete(nullptr);
  }, "Sending Sensor Data", 16000, (void *) sendTuple, 1, &Task1);
}

bool connectToServer(BLEAdvertisedDevice &device,
                     DeviceType deviceType,
                     BLEClient *pClient) {
  switch (deviceType) {
    case TI:Serial.printf("Forming a connection (TI) to %s \n", device.getAddress().toString().c_str());
      break;
    case Nordic:Serial.printf("Forming a connection (Nordic) to %s \n", device.getAddress().toString().c_str());
      break;
    case Custom:Serial.printf("Forming a connection (Custom) to %s \n", device.getAddress().toString().c_str());
      break;
  }

  Serial.println(" - Created client");
  int res = pClient->connect(&device);
  delay(5000);
  Serial.printf(" - Connected to server: %d\n", res);
  Serial.printf(" - is Connected: %d\n", pClient->isConnected());
  NimBLEDevice::setMTU(517); //set client to request maximum MTU from server (default is 23 otherwise)

  // Obtain a reference to the service we are after in the remote BLE server.
  BLEUUID *UUID;
  switch (deviceType) {
    case TI:UUID = &serviceTIUUID;
      break;
    case Nordic:UUID = &serviceNordicUUID;
      break;
    case Custom:UUID = &serviceCustomUUID;
      break;
  }
  BLERemoteService *pRemoteService = pClient->getService(*UUID);

  if (pRemoteService == nullptr) {
    Serial.print("Failed to find service UUID: ");
    Serial.println(UUID->toString().c_str());
    if (pClient->isConnected()) {
      pClient->disconnect();
    }
    return false;
  }
  Serial.println(" - Found our service");

  if (deviceType == DeviceType::TI) {
    // Obtain a reference to the characteristic in the service of the remote BLE server.
    pRemoteTIWriteCharacteristic = pRemoteService->getCharacteristic(charTISendUUID);

    if (pRemoteTIWriteCharacteristic == nullptr) {
      Serial.print("Failed to find our characteristic UUID: ");
      Serial.println(charTISendUUID.toString().c_str());
      pClient->disconnect();
      return false;
    }
    Serial.println(pRemoteTIWriteCharacteristic->canWrite());
    Serial.println(pRemoteTIWriteCharacteristic->canRead());
    Serial.println(pRemoteTIWriteCharacteristic->canWriteNoResponse());
    if (pRemoteTIWriteCharacteristic->canWrite()) {
      pRemoteTIWriteCharacteristic->writeValue((char) 1, true);
      auto val = pRemoteTIWriteCharacteristic->readValue();
      int returnLen = val.length();
      Serial.printf("Len return %d\n", returnLen);
      for (int i = 0; i < returnLen; i++) {
        Serial.printf("val: %d\n", val.data()[i]);
      }

      Serial.println("Finished writing");
    } else {
      Serial.print("Can't write to write: ");
      Serial.println(charTISendUUID.toString().c_str());
      pClient->disconnect();
      return false;
    }
  }
  switch (deviceType) {
    case TI:pRemoteReadCharacteristic = pRemoteService->getCharacteristic(charTIRecUUID);
      break;
    case Nordic:pRemoteReadCharacteristic = pRemoteService->getCharacteristic(charNordicUUID);
      break;
    case Custom:pRemoteReadCharacteristic = pRemoteService->getCharacteristic(charCustomUUID);
      break;
    default:throw "What's it doing here";
  }

  if (pRemoteReadCharacteristic == nullptr) {
    Serial.print("Failed to find our characteristic UUID: ");
    Serial.println(charTIRecUUID.toString().c_str());
    pClient->disconnect();
    return false;
  }
  Serial.printf(" - Found our service %s\n", pRemoteService->getUUID().toString().c_str());
  Serial.printf(" - Found our characteristic %s\n", pRemoteReadCharacteristic->getUUID().toString().c_str());

  Serial.printf("Can Write %d\n", pRemoteReadCharacteristic->canWrite());
  Serial.printf("Can read %d\n", pRemoteReadCharacteristic->canRead());
  Serial.printf("Can notify %d\n", pRemoteReadCharacteristic->canNotify());
  Serial.printf("Can Indicate %d\n", pRemoteReadCharacteristic->canIndicate());

  if (pRemoteReadCharacteristic->canNotify()) {
    switch (deviceType) {

      case TI:Serial.println("TI");
        pRemoteReadCharacteristic->registerForNotify(notifyTICallback);
        break;

      case Nordic:Serial.println("Nordic");
        pRemoteReadCharacteristic->registerForNotify(notifyNordicCallback);
        break;
      case Custom:break;
    }
  }
  return true;
}
struct ParamArgs {
  BLEAdvertisedDevice dev;
  DeviceType deviceType;
  BLEClient *pClient;
};
std::mutex isConnectingBLEMtx;
bool isConnectingBLE = false;

[[noreturn]] void innerConnectToServer(void *parameters) {

  auto pa = (ParamArgs *) parameters;
  connectToServer(pa->dev, pa->deviceType, pa->pClient);
  isConnectingBLEMtx.lock();
  isConnectingBLE = false;
  isConnectingBLEMtx.unlock();
  vTaskDelete(nullptr);
  for (;;) {
  }
}

void GetSensorData::loop() {
  Serial.println("in loop\n\n\n\n\n\n");

  mtxGetBLEAddress.lock();
  bool addressesEmpty = addresses.empty();
  mtxGetBLEAddress.unlock();

  if (addressesEmpty) {

    delay(500);
    return;
  }

  mtxGetBLEAddress.lock();
  auto curAddress = addresses.front();
  addresses.pop_front();
  Serial.printf(" - %s\n", std::get<0>(curAddress).c_str());
  addresses.emplace_back(curAddress);
  mtxGetBLEAddress.unlock();

  Serial.println("Before get scan");
  delay(100);

  ScanResults scanResultsClass;
  auto scanResults = scanResultsClass.getScanResults();

  for (int i = 0; i < scanResults.getCount(); i++) {
    auto dev = scanResults.getDevice(i);
    if (dev.getAddress().toString() == std::get<0>(curAddress)) {
      Serial.printf("Found %s - %d\n", dev.getName().c_str(), dev.getAddressType());
      TaskHandle_t Task1;
      ParamArgs pa{
          .dev = dev,
          .deviceType = std::get<1>(curAddress),
          .pClient = pClient,
      };
      isConnectingBLEMtx.lock();
      isConnectingBLE = true;
      isConnectingBLEMtx.unlock();
      xTaskCreate(innerConnectToServer, "Name", 16000, (void *) &pa, 1, &Task1);
      delay(30'000);
      lastGotDataMtx.lock();
      unsigned long _lastGotData = lastGotData;
      lastGotDataMtx.unlock();

      isConnectingBLEMtx.lock();
      if (isConnectingBLE || (_lastGotData != 0 && getTime() - lastGotData > 120'000)) {
        Serial.println("Restarting");
        isConnectingBLEMtx.unlock();

        esp_restart();
      }
      isConnectingBLEMtx.unlock();
      if (pRemoteReadCharacteristic != nullptr) {
        pRemoteReadCharacteristic->registerForNotify(nullptr);
      }
      //vTaskDelete(Task1);
      //Task1 = nullptr;
      if (pClient != nullptr && pClient->isConnected()) {
        Serial.print("disConnectingBLE\n");
        pClient->disconnect();
      }
    }
  }

}
GetSensorData::GetSensorData() {
  pClient = NimBLEDevice::createClient();
}
void GetSensorData::clearDevices() {
  mtxGetBLEAddress.lock();
  addresses.clear();
  mtxGetBLEAddress.unlock();
}
void GetSensorData::setDevices(std::vector<std::tuple<std::string, DeviceType>> &newDevice) {
  mtxGetBLEAddress.lock();
  std::vector<std::tuple<std::basic_string<char>, DeviceType>> newVec;
  for (const auto &e : addresses) {
    if (contains(newDevice, e)) {
      newVec.emplace_back(e);
    }
  }
  for (const auto &e : newDevice) {
    if (!contains(newVec, e)) {
      newVec.emplace_back(e);
    }
  }
  addresses.clear();
  for (const auto &e : newVec) {
    addresses.emplace_back(e);
  }
  mtxGetBLEAddress.unlock();
}

