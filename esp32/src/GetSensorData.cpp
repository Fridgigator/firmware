#include <mutex>
#include <unordered_map>

#include "GetSensorData.h"
#include "BLEUtils.h"
#include "HTTPSend.h"
#include "Constants.h"
#include "SensorDataStore.h"
#include "generated/packet.pb.h"
#include "pb_encode.h"
#include "exceptions/DecodeException.h"
#include "getTime.h"

void nordicCallbackProcess(BLERemoteCharacteristic *pBLERemoteCharacteristic,
                           const uint8_t *pData,
                           size_t length,
                           MeasureType type
);
static std::vector<BLEUUID> serviceNordicUUIDs({BLEUUID("ef680200-9b35-4933-9b10-52ffa9740042"),
                                                BLEUUID("ef680300-9b35-4933-9b10-52ffa9740042"),
                                                BLEUUID("ef680400-9b35-4933-9b10-52ffa9740042"),
                                                BLEUUID("ef680500-9b35-4933-9b10-52ffa9740042")});
static std::vector<BLEUUID> serviceTIUUIDs({BLEUUID("f000aa00-0451-4000-b000-000000000000")});
static std::vector<BLEUUID> serviceCustomUUIDs({BLEUUID("00000000-0000-4000-0000-000000000000")});
static std::vector<BLEUUID> serviceHubUUIDs({BLEUUID(SERVICE_UUID)});

static BLEUUID charNordicTempUUID("ef680201-9b35-4933-9b10-52ffa9740042");
static BLEUUID charNordicHumidityUUID("ef680203-9b35-4933-9b10-52ffa9740042");
static BLEUUID charCustomUUID("00000000-0000-4000-0000-000000000000");
static BLEUUID charTISendUUID("f000aa02-0451-4000-b000-000000000000");
static BLEUUID charTIRecUUID("f000aa01-0451-4000-b000-000000000000");

static BLEUUID charHubUUID(CHARACTERISTIC_SERVER_UUID);

static BLERemoteCharacteristic *pRemoteTIWriteCharacteristic = nullptr;
static unordered_map<MeasureType, BLERemoteCharacteristic *> pRemoteReadCharacteristics;
static BLERemoteCharacteristic *pRemoteHubCharacteristic = nullptr;

safe_std::mutex<std::deque<std::tuple<std::string, DeviceType>>> addresses;

safe_std::mutex<std::map<std::string, SensorDataStore>> sensorData;

safe_std::mutex<unsigned long> lastGotData;

template<typename T>
bool contains(std::vector<T> &v, T key) {
  for (const auto &e : v) {
    if (e == key) {
      return true;
    }
  }
  return false;
}

void notifyNordicCallbackTemp(
    BLERemoteCharacteristic *pBLERemoteCharacteristic,
    const uint8_t *pData,
    size_t length,
    bool isNotify) {
  nordicCallbackProcess(pBLERemoteCharacteristic,
                        pData,
                        length,
                        MeasureType::TEMP
  );
}
void notifyNordicCallbackHumidity(
    BLERemoteCharacteristic *pBLERemoteCharacteristic,
    const uint8_t *pData,
    size_t length,
    bool isNotify) {
  nordicCallbackProcess(pBLERemoteCharacteristic,
                        pData,
                        length,
                        MeasureType::HUMIDITY
  );
}
void nordicCallbackProcess(BLERemoteCharacteristic *pBLERemoteCharacteristic,
                           const uint8_t *pData,
                           size_t length,
                           MeasureType type
) {
  Serial.printf("Notify callback for characteristic %s of data length %d. data: %d.%d\n",
                pBLERemoteCharacteristic->getUUID().toString().c_str(),
                length,
                pData[1],
                pData[0]);
  int low = pData[0];
  int high = pData[1];
  std::string

      remoteAddress = pBLERemoteCharacteristic->getRemoteService()->getClient()->getConnInfo().getAddress().toString();
  auto *sendTuple = new std::tuple<int, int, std::string>(low, high, remoteAddress);
  TaskHandle_t Task1;

  lastGotData.lockAndSwap(getTime());

  {
    auto _sensorData = sensorData.lock();
    auto time = getTime();

    float temperature = std::stof(std::to_string(low) + "." + std::to_string(high));
    _sensorData->insert_or_assign(remoteAddress + to_string(type), SensorDataStore{
        .timestamp = time,
        .address = remoteAddress,
        .type = DeviceType::Nordic,
        .value = temperature,
        .measure_type = type,

    });
  }
  lastGotData.lockAndSwap(getTime());
}

void notifyTICallback(
    BLERemoteCharacteristic *pBLERemoteCharacteristic,
    uint8_t *pData,
    size_t length,
    bool isNotify) {
  Serial.print("Notify callback for characteristic ");
  Serial.print(pBLERemoteCharacteristic->getUUID().toString().c_str());
  std::string
      remoteAddress =
      pBLERemoteCharacteristic->getRemoteService()->getClient()->getConnInfo().getAddress().toString();
  Serial.print(" of data length ");
  Serial.println(length);
  assert(length == 4);
  static_assert(sizeof(float) == 4, "float size is expected to be 4 bytes");
  float f;
  memcpy(&f, pData, 4);
  {
    auto _sensorData = sensorData.lock();
    _sensorData->insert_or_assign(remoteAddress, SensorDataStore{
        .timestamp = getTime(),
        .address = remoteAddress,
        .type = DeviceType::TI,
        .value = f,
    });

    _sensorData->insert_or_assign(remoteAddress + to_string(MeasureType::TEMP), SensorDataStore{
        .timestamp = getTime(),
        .address = remoteAddress,
        .type = DeviceType::TI,
        .value = f,
        .measure_type = MeasureType::TEMP,
    });
  }
  Serial.printf("addr: %f\n", f);

  lastGotData.lockAndSwap(getTime());
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
    case Hub:Serial.printf("Forming a connection (Hub) to %s \n", device.getAddress().toString().c_str());
      break;
  }

  Serial.println(" - Created client");
  int res = pClient->connect(&device);
  delay(5000);
  Serial.printf(" - Connected to server: %d\n", res);
  Serial.printf(" - is Connected: %d\n", pClient->isConnected());
  auto s = pClient->getServices();
  Serial.printf("possible services length: %d\n", s->size());
  for (auto const p : *s) {
    Serial.printf("possible services: %s\n", p->getUUID().toString().c_str());
  }
  NimBLEDevice::setMTU(517); //set client to request maximum MTU from server (default is 23 otherwise)

  // Obtain a reference to the service we are after in the remote BLE server.
  vector<BLEUUID> *vectorUUID;
  switch (deviceType) {
    case TI:vectorUUID = &serviceTIUUIDs;
      break;
    case Nordic:vectorUUID = &serviceNordicUUIDs;
      break;
    case Custom:vectorUUID = &serviceCustomUUIDs;
      break;
    case Hub:vectorUUID = &serviceHubUUIDs;
      break;
  }
  BLEUUID *UUID;
  bool found = false;
  BLERemoteService *pRemoteService = nullptr;
  for (unsigned int i = 0; i < vectorUUID->size() && !found; i++) {
    pRemoteService = pClient->getService(vectorUUID->at(i));
    if (pRemoteService == nullptr) {
      if (pClient->isConnected()) {
        pClient->disconnect();
      }
    } else {
      Serial.println(" - Found our service");
      found = true;
    }
  }
  if (!found) {
    Serial.printf("Failed to find service UUID\n");
    return false;
  }
  if (pRemoteService == nullptr) {
    Serial.printf("pRemoteService == nullptr\n");
    return false;

  }

  if (deviceType == DeviceType::TI) {
    // Obtain a reference to the characteristic in the service of the remote BLE server.
    pRemoteTIWriteCharacteristic = pRemoteService->getCharacteristic(charTISendUUID);

    if (pRemoteTIWriteCharacteristic == nullptr) {
      Serial.print("Failed to find our characteristic UUID (TI) write: ");
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
  } else if (deviceType == DeviceType::Hub) {
    // Obtain a reference to the characteristic in the service of the remote BLE server.
    pRemoteHubCharacteristic = pRemoteService->getCharacteristic(charHubUUID);

    if (pRemoteHubCharacteristic == nullptr) {
      Serial.print("Failed to find our characteristic UUID (hub): ");
      Serial.println(charHubUUID.toString().c_str());
      pClient->disconnect();
      return false;
    }
    Serial.println(pRemoteHubCharacteristic->canWrite());
    Serial.println(pRemoteHubCharacteristic->canRead());
    Serial.println(pRemoteHubCharacteristic->canWriteNoResponse());
    if (pRemoteHubCharacteristic->canWrite()) {

      BLESendPacket p = BLESendPacket_init_zero;
      p.which_type = BLESendPacket_crossDevicePacket_tag;

      deque<tuple<std::string, DeviceType>> tmpAddresses;
      {
        tmpAddresses = *addresses.lock();
      }
      std::map<std::string, SensorDataStore> tmpSensorData;
      {
        tmpSensorData = *(sensorData.lock());
      }
      SensorsListInterDevice sList = SensorsListInterDevice_init_default;
      ValuesInterDeviceList vList = ValuesInterDeviceList_init_default;

      {
        pb_size_t sizeOfSList = std::min(tmpAddresses.size(), static_cast<size_t>(64));
        sList.sensor_info_count = sizeOfSList;

        for (unsigned int i = 0; i < sizeOfSList; i++) {
          auto name = std::get<0>(tmpAddresses[i]);
          Serial.printf("name: %s\n", name.c_str());
          memcpy(&sList.sensor_info[i].address, name.c_str(), 21);
          auto type = std::get<1>(tmpAddresses[i]);
          switch (type) {
            case TI:sList.sensor_info[i].device_type = SensorInfoInterDevice_DEVICE_TYPE_TI;
              break;
            case Nordic:sList.sensor_info[i].device_type = SensorInfoInterDevice_DEVICE_TYPE_NORDIC;
              break;
            case Hub:sList.sensor_info[i].device_type = SensorInfoInterDevice_DEVICE_TYPE_HUB;
              break;
            case Custom:sList.sensor_info[i].device_type = SensorInfoInterDevice_DEVICE_TYPE_CUSTOM;
              break;
          }
        }
      }
      {
        pb_size_t sizeOfVList = std::min(tmpSensorData.size(), static_cast<size_t>(64));
        vList.values_count = sizeOfVList;
        int i = 0;
        for (auto &iter : tmpSensorData) {
          if (i == sizeOfVList) {
            break;
          }
          auto name = iter.second.address;
          memcpy(&vList.values[i].address, name.c_str(), 21);
          auto value = iter.second;
          Serial.printf("Time To Send=%lld\n", value.timestamp);
          vList.values[i].timestamp = value.timestamp;
          vList.values[i].value = value.value;
          switch (value.measure_type) {
            case TEMP:vList.values[i].measure_type = ValuesInterDevice_MEASURE_TYPE_TEMP;
              break;
            case HUMIDITY:vList.values[i].measure_type = ValuesInterDevice_MEASURE_TYPE_HUMIDITY;
              break;
          }
          switch (value.type) {
            case TI:vList.values[i].device_type = ValuesInterDevice_DEVICE_TYPE_TI;
              break;
            case Nordic:vList.values[i].device_type = ValuesInterDevice_DEVICE_TYPE_NORDIC;
              break;
            case Hub:vList.values[i].device_type = ValuesInterDevice_DEVICE_TYPE_HUB;
              break;
            case Custom:vList.values[i].device_type = ValuesInterDevice_DEVICE_TYPE_CUSTOM;
              break;
          }
          i++;
        }

      }

      p.type.crossDevicePacket = CrossDevicePacket{
          .has_sensorList = true,
          .sensorList = sList,
          .has_values = true,
          .values = vList,
          .timestamp = getTime(),
      };

      auto buf = new pb_byte_t[2024];
      pb_ostream_t output = pb_ostream_from_buffer(buf, 2048);

      int status = pb_encode(&output, BLESendPacket_fields, &p);
      if (!status) {
        Serial.print("Encoding failed:");
        Serial.println(PB_GET_ERROR(&output));
        throw DecodeException();
      }

      pRemoteHubCharacteristic->writeValue(output.bytes_written, true);
      std::vector<uint8_t> buf1;
      Serial.print("About to send to other: ");
      for (int i = 0; i < output.bytes_written; i++) {
        Serial.printf("%d ", (int) buf[i]);
        buf1.push_back(buf[i]);
      }
      Serial.println();
      pRemoteHubCharacteristic->writeValue(buf1);
      delete[] buf;
    } else {
      Serial.print("Can't write to write: ");
      Serial.println(charHubUUID.toString().c_str());
      pClient->disconnect();
      return false;
    }
  }
  pRemoteReadCharacteristics[MeasureType::TEMP] = nullptr;
  pRemoteReadCharacteristics[MeasureType::HUMIDITY] = nullptr;

  switch (deviceType) {
    case TI:pRemoteReadCharacteristics[MeasureType::TEMP] = pRemoteService->getCharacteristic(charTIRecUUID);
      break;
    case Nordic: {
      pRemoteReadCharacteristics[MeasureType::TEMP] = pRemoteService->getCharacteristic(charNordicTempUUID);
      pRemoteReadCharacteristics[MeasureType::HUMIDITY] = pRemoteService->getCharacteristic(charNordicHumidityUUID);
      break;
    }
    case Custom:pRemoteReadCharacteristics[MeasureType::TEMP] = pRemoteService->getCharacteristic(charCustomUUID);
      break;
    case Hub:pRemoteReadCharacteristics[MeasureType::TEMP] = pRemoteService->getCharacteristic(charHubUUID);
      break;
    default:throw "What's it doing here";
  }

  for (const auto [type, pRemoteReadCharacteristic] : pRemoteReadCharacteristics) {
    Serial.printf(" - Found our service %s for %d\n", pRemoteService->getUUID().toString().c_str(), deviceType);
    Serial.printf(" - Type of measurement = %s\n", type == MeasureType::TEMP ? "TEMP" : "HUMIDITY");
    if (pRemoteReadCharacteristic == nullptr) {
      Serial.printf("\033[0;31mNull pointer!!\033[0;30m\n");
      continue;
    }
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

        case Nordic:Serial.printf("Nordic: %s\n", type == MeasureType::HUMIDITY ? "Humidity" : "Temperature");

          pRemoteReadCharacteristic->registerForNotify(
              type == MeasureType::HUMIDITY ? notifyNordicCallbackHumidity : notifyNordicCallbackTemp);
          break;
        case Custom:break;
        case Hub:break;
      }
    }
  }
  return true;
}
struct ParamArgs {
  BLEAdvertisedDevice dev;
  DeviceType deviceType;
  BLEClient *pClient;
};

safe_std::mutex<bool> isConnectingBLE = false;

[[noreturn]] void innerConnectToServer(void *parameters) {

  auto pa = (ParamArgs *) parameters;
  connectToServer(pa->dev, pa->deviceType, pa->pClient);
  Serial.printf("Finished connecting to server\n");
  isConnectingBLE.lockAndSwap(false);
  vTaskDelete(nullptr);
  for (;;) {
  }
}

void GetSensorData::loop() {
  Serial.println("in loop\n\n\n\n\n\n");

  bool addressesEmpty;
  {
    addressesEmpty = addresses.lock()->empty();
  }

  if (addressesEmpty) {

    delay(500);
    return;
  }

  tuple<string, DeviceType> curAddress;
  {
    auto lock = addresses.lock();
    curAddress = lock->front();
    lock->pop_front();
  }
  Serial.printf(" - %s\n", std::get<0>(curAddress).c_str());
  addresses.lock()->emplace_back(curAddress);


  Serial.println("Before get scan");
  delay(100);

  ScanResults scanResultsClass;
  auto scanResults = scanResultsClass.getScanResults();

  for (int i = 0; i < scanResults.getCount(); i++) {
    for (const auto [type, pRemoteReadCharacteristic] : pRemoteReadCharacteristics) {
      if (pRemoteReadCharacteristic != nullptr) {
        pRemoteReadCharacteristic->unsubscribe();
        pRemoteReadCharacteristic->registerForNotify(nullptr);
      }
    }
    auto dev = scanResults.getDevice(i);
    if (dev.getAddress().toString() == std::get<0>(curAddress)) {
      Serial.printf("Found %s - %d\n", dev.getName().c_str(), dev.getAddressType());
      TaskHandle_t Task1;
      ParamArgs pa{
          .dev = dev,
          .deviceType = std::get<1>(curAddress),
          .pClient = pClient,
      };

      isConnectingBLE.lockAndSwap(true);

      xTaskCreate(innerConnectToServer, "Name", 16000, (void *) &pa, 1, &Task1);

      delay(30'000);
      unsigned long _lastGotData;
      {
        _lastGotData = *lastGotData.lock();
      }

      if (*isConnectingBLE.lock() || (_lastGotData != 0 && getTime() - _lastGotData > 120'000)) {
        Serial.printf("_lastGotData = %lu; getTime = %lld; lastGotData = %lu, \n",
                      _lastGotData,
                      getTime(),
                      _lastGotData);
        Serial.println("\033[0;31mRestarting");

        esp_restart();
      }

      //vTaskDelete(Task1);
      //Task1 = nullptr;
      if (pClient != nullptr && pClient->isConnected()) {
        Serial.print("disConnectingBLE\n");
        pClient->disconnect();
        isConnectingBLE.lockAndSwap(false);
      }
    }
  }
  isConnectingBLE.lockAndSwap(false);
}
GetSensorData::GetSensorData() {
  pClient = NimBLEDevice::createClient();
}
void GetSensorData::clearDevices() {
  addresses.lock()->clear();

}
void GetSensorData::setDevices(std::vector<std::tuple<std::string, DeviceType>> &newDevice) {

  std::vector<std::tuple<std::basic_string<char>, DeviceType>> newVec;
  auto lock = addresses.lock();
  for (const auto& e : *lock) {
    if (contains(newDevice, e)) {
      newVec.emplace_back(e);
    }
  }
  for (const auto &e : newDevice) {
    if (!contains(newVec, e)) {
      newVec.emplace_back(e);
    }
  }
  lock->clear();
  for (const auto &e : newVec) {
    Serial.printf("  -  adding new device %s\n", get<0>(e).c_str());
    addresses.lock()->emplace_back(e);
  }
}

