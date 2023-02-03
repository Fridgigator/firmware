#include <unordered_map>
#include <esp_task_wdt.h>
#include "BLEUtils.h"
#include "GetSensorData.h"
#include "Constants.h"
#include "SensorDataStore.h"
#include "generated/packet.pb.h"
#include "pb_encode.h"
#include "exceptions/DecodeException.h"
#include "getTime.h"
#include "lib/websocket/websocket.h"
#include "generated/firmware_backend.pb.h"

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
static std::vector<BLEUUID> serviceHubUUIDs({BLEUUID(SERVICE_UUID)});

static std::vector<BLEUUID> servicePicoUUIDs({BLEUUID("0000ffe0-0000-1000-8000-00805f9b34fb")});
static BLEUUID charPicoUUID("0000ffe1-0000-1000-8000-00805f9b34fb");

static BLEUUID charNordicTempUUID("ef680201-9b35-4933-9b10-52ffa9740042");
static BLEUUID charNordicHumidityUUID("ef680203-9b35-4933-9b10-52ffa9740042");
static BLEUUID charTISendUUID("f000aa02-0451-4000-b000-000000000000");
static BLEUUID charTIRecUUID("f000aa01-0451-4000-b000-000000000000");

static BLEUUID charHubUUID(CHARACTERISTIC_SERVER_UUID);

static BLERemoteCharacteristic *pRemoteTIWriteCharacteristic = nullptr;
static unordered_map<MeasureType, BLERemoteCharacteristic *> pRemoteReadCharacteristics;
static BLERemoteCharacteristic *pRemoteHubCharacteristic = nullptr;

safe_std::mutex<std::deque<std::tuple<std::string, TypeOfDevice>>> addresses;

safe_std::mutex<std::map<std::string, SensorDataStore>> sensorData;

safe_std::mutex<unsigned long> lastGotData;

template<typename T>
bool contains(std::vector<T> &v, T key) {
    return std::any_of(v.begin(), v.end(), [key](T a) { return a == key; });
}

safe_std::mutex<deque<uint8_t>> pico_data;
safe_std::mutex<bool> hit_null(false);

void notifyCustomCallback(
        BLERemoteCharacteristic *pBLERemoteCharacteristic,
        const uint8_t *pData,
        size_t length,
        bool isNotify) {
    ESP_ERROR_CHECK(gpio_set_level(GPIO_NUM_19, 1));
    for (int i = 0; i < length; i++) {
        if (pData[i] == 0) {
            bool t = true;
            hit_null.lockAndSwap(t);
        }
        if (*hit_null.lock()) {
            pico_data.lock()->emplace_front(pData[i]);
        }
    }
    auto locked_pico_data = pico_data.lock();
    bool found_null = false;
    vector<string> fullData;
    string tmpString;
    while (any_of(locked_pico_data->begin(), locked_pico_data->end(), [](auto a) { return a == 0; })) {
        unsigned char it = locked_pico_data->back();
        locked_pico_data->pop_back();
        if (it == 0) {
            string data_to_copy = tmpString;
            fullData.emplace_back(data_to_copy);
            tmpString.clear();
        } else {
            tmpString += it;
        }
    }

    if (fullData.empty()) {
        LOG("Not enough data\n");
        return;
    }

    for (const auto &data: fullData) {
        string parsedData = data;
        if (parsedData.size() >= 3) {

            MeasureType type;
            float val;
            switch (parsedData.at(0)) {
                case 'H': {
                    type = MeasureType::DHT22_HUMIDITY;
                    break;
                }
                case 'T': {
                    type = MeasureType::DHT22_TEMP;
                    break;
                }
                case 'h': {
                    type = MeasureType::DHT11_HUMIDITY;
                    break;
                }
                case 't': {
                    type = MeasureType::DHT11_TEMP;
                    break;
                }
                case 'p': {
                    type = MeasureType::PICO_TEMP;
                    break;
                }
                default: {
                    throw std::runtime_error("Assertion error: invalid first letter");
                }
            }
            parsedData.erase(0, 1);
            val = stof(parsedData);
            std::string

                    remoteAddress =
                    pBLERemoteCharacteristic->getRemoteService()->getClient()->getConnInfo().getAddress().toString();

            lastGotData.lockAndSwap(getTime());
            auto time = getTime();
            SensorDataStore sensorDataStore = SensorDataStore{
                    .timestamp = time,
                    .address = remoteAddress,
                    .type = TypeOfDevice::Custom,
                    .value = val,
                    .measure_type = type,
            };
            {
                auto _sensorData = sensorData.lock();
                _sensorData->insert_or_assign(remoteAddress + to_string(type), sensorDataStore);
            }
            auto packet = make_unique<FirmwareToBackendPacket>();
            *packet = FirmwareToBackendPacket_init_default;

            packet->which_type = FirmwareToBackendPacket_sensor_data_tag;
            DataType data_type;
            switch (sensorDataStore.measure_type) {
                case MeasureType::DHT22_HUMIDITY:
                    data_type = DataType_DATA_TYPE_DHT22_HUMIDITY;
                    break;
                case MeasureType::DHT11_HUMIDITY:
                    data_type = DataType_DATA_TYPE_DHT11_HUMIDITY;
                    break;
                case TEMP:
                    data_type = DataType_DATA_TYPE_TEMP;
                    break;
                case HUMIDITY:
                    data_type = DataType_DATA_TYPE_HUMIDITY;
                    break;
                case DHT11_TEMP:
                    data_type = DataType_DATA_TYPE_DHT11_TEMP;
                    break;
                case DHT22_TEMP:
                    data_type = DataType_DATA_TYPE_DHT22_TEMP;
                    break;
                case PICO_TEMP:
                    data_type = DataType_DATA_TYPE_PICO_TEMP;
                    break;
            }

            FirmwareToBackendPacket_type_sensor_data_MSGTYPE p = {0};
            for (int i = 0; i < std::min(static_cast<unsigned int>(21),
                                         static_cast<unsigned int>(sensorDataStore.address.size())); i++) {
                p.address[i] = sensorDataStore.address.at(i);
            }
            p.data_type = data_type;
            p.value = sensorDataStore.value;
            p.timestamp = sensorDataStore.timestamp;
            packet.get()->type.sensor_data = p;

            vector<uint8_t>
                    buf(2048);
            pb_ostream_t output = pb_ostream_from_buffer(buf.data(), buf.size());
            int status = pb_encode(&output, FirmwareToBackendPacket_fields, packet.get());
            if (!status) {
                throw std::runtime_error(string("Decoding failed: ") + PB_GET_ERROR(&output));
            }
            buf.resize(output.bytes_written);
            int amountCallsCustomLock;
            LOG("Got custom data: %d\n", amountCallsCustomLock);

            LOG("Sending custom data\n");

            WriteSocketError error = websocket::getInstance()->writeBytes(buf, 5'000);
            if (error == WriteSocketError::WriteError && websocket::getInstance()->isConnected()) {
                throw std::runtime_error("Cannot send data to websocket");
            }

        }
        LOG("About to lock and swap\n");
        lastGotData.lockAndSwap(getTime());
        LOG("Finished lock and swap\n");

    }
    LOG("About to return\n");
    ESP_ERROR_CHECK(gpio_set_level(GPIO_NUM_19, 0));
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
    ESP_ERROR_CHECK(gpio_set_level(GPIO_NUM_19, 1));
    int low = pData[0];
    int high = pData[1];
    std::string remoteAddress = pBLERemoteCharacteristic->getRemoteService()->getClient()->getConnInfo().getAddress().toString();

    lastGotData.lockAndSwap(getTime());
    auto time = getTime();

    float temperature = std::stof(std::to_string(low) + "." + std::to_string(high));
    SensorDataStore sensorDataStore = SensorDataStore{
            .timestamp = time,
            .address = remoteAddress,
            .type = TypeOfDevice::Nordic,
            .value = temperature,
            .measure_type = type,

    };
    {
        auto _sensorData = sensorData.lock();
        _sensorData->insert_or_assign(remoteAddress + to_string(type), sensorDataStore);
    }
    auto packet = make_unique<FirmwareToBackendPacket>();
    *packet = FirmwareToBackendPacket_init_default;
    packet->which_type = FirmwareToBackendPacket_sensor_data_tag;
    DataType data_type;
    switch (sensorDataStore.measure_type) {
        case MeasureType::DHT22_HUMIDITY:
            data_type = DataType_DATA_TYPE_DHT22_HUMIDITY;
            break;
        case MeasureType::DHT11_HUMIDITY:
            data_type = DataType_DATA_TYPE_DHT11_HUMIDITY;
            break;
        case TEMP:
            data_type = DataType_DATA_TYPE_TEMP;
            break;
        case HUMIDITY:
            data_type = DataType_DATA_TYPE_HUMIDITY;
            break;
        case DHT11_TEMP:
            data_type = DataType_DATA_TYPE_DHT11_TEMP;
            break;
        case DHT22_TEMP:
            data_type = DataType_DATA_TYPE_DHT22_TEMP;
            break;
        case PICO_TEMP:
            data_type = DataType_DATA_TYPE_PICO_TEMP;
            break;
    }

    FirmwareToBackendPacket_type_sensor_data_MSGTYPE p = {0};
    for (int i = 0;
         i < std::min(static_cast<unsigned int>(21), static_cast<unsigned int>(sensorDataStore.address.size())); i++) {
        p.address[i] = sensorDataStore.address.at(i);
    }
    p.data_type = data_type;
    p.value = sensorDataStore.value;
    p.timestamp = sensorDataStore.timestamp;
    packet.get()->type.sensor_data = p;
    vector<uint8_t>
            buf(2048);
    pb_ostream_t output = pb_ostream_from_buffer(buf.data(), buf.size());
    int status = pb_encode(&output, FirmwareToBackendPacket_fields, packet.get());
    if (!status) {
        throw std::runtime_error(string("Encoding failed: ") + PB_GET_ERROR(&output));
    }
    buf.resize(output.bytes_written);

    int amountCallsNordicHumidityLock;
    int amountCallsNordicTempLock;

    WriteSocketError error = websocket::getInstance()->writeBytes(buf, 5'000);
    if (error == WriteSocketError::WriteError && websocket::getInstance()->isConnected()) {
        throw std::runtime_error("Cannot send data to websocket");
    }


    LOG("Got nordic data: %d, %d\n", amountCallsNordicHumidityLock, amountCallsNordicTempLock);

    lastGotData.lockAndSwap(getTime());
    ESP_ERROR_CHECK(gpio_set_level(GPIO_NUM_19, 0));
}

void notifyTICallback(
        BLERemoteCharacteristic *pBLERemoteCharacteristic,
        uint8_t *pData,
        size_t length,
        bool isNotify) {
    ESP_ERROR_CHECK(gpio_set_level(GPIO_NUM_19, 1));
    std::string
            remoteAddress =
            pBLERemoteCharacteristic->getRemoteService()->getClient()->getConnInfo().getAddress().toString();
    assert(length == 4);
    static_assert(sizeof(float) == 4, "float size is expected to be 4 bytes");
    float f;
    memcpy(&f, pData, 4);
    SensorDataStore sensorDataStore = SensorDataStore{
            .timestamp = getTime(),
            .address = remoteAddress,
            .type = TypeOfDevice::TI,
            .value = f,
            .measure_type = MeasureType::TEMP,
    };
    {
        auto _sensorData = sensorData.lock();

        _sensorData->insert_or_assign(remoteAddress + to_string(MeasureType::TEMP), sensorDataStore);
    }
    auto packet = make_unique<FirmwareToBackendPacket>();
    *packet = FirmwareToBackendPacket_init_default;
    packet->which_type = FirmwareToBackendPacket_sensor_data_tag;
    DataType data_type;
    switch (sensorDataStore.measure_type) {
        case MeasureType::DHT22_HUMIDITY:
            data_type = DataType_DATA_TYPE_DHT22_HUMIDITY;
            break;
        case MeasureType::DHT11_HUMIDITY:
            data_type = DataType_DATA_TYPE_DHT11_HUMIDITY;
            break;
        case TEMP:
            data_type = DataType_DATA_TYPE_TEMP;
            break;
        case HUMIDITY:
            data_type = DataType_DATA_TYPE_HUMIDITY;
            break;
        case DHT11_TEMP:
            data_type = DataType_DATA_TYPE_DHT11_TEMP;
            break;
        case DHT22_TEMP:
            data_type = DataType_DATA_TYPE_DHT22_TEMP;
            break;
        case PICO_TEMP:
            data_type = DataType_DATA_TYPE_PICO_TEMP;
            break;
    }

    FirmwareToBackendPacket_type_sensor_data_MSGTYPE p = {0};
    for (int i = 0;
         i < std::min(static_cast<unsigned int>(21), static_cast<unsigned int>(sensorDataStore.address.size())); i++) {
        p.address[i] = sensorDataStore.address.at(i);
    }
    p.data_type = data_type;
    p.value = sensorDataStore.value;
    p.timestamp = sensorDataStore.timestamp;

    vector<uint8_t>
            buf(2048);
    pb_ostream_t output = pb_ostream_from_buffer(buf.data(), buf.size());
    packet.get()->type.sensor_data = p;
    int status = pb_encode(&output, FirmwareToBackendPacket_fields, packet.get());
    if (!status) {
        throw std::runtime_error(string("Encoding failed: ") + PB_GET_ERROR(&output));
    }
    buf.resize(output.bytes_written);
    WriteSocketError error = websocket::getInstance()->writeBytes(buf, 5'000);
    if (error == WriteSocketError::WriteError && websocket::getInstance()->isConnected()) {
        throw std::runtime_error("Cannot send data to websocket");

    }
    LOG("About to lockandswap\n");
    lastGotData.lockAndSwap(getTime());
    LOG("About to return\n");
    ESP_ERROR_CHECK(gpio_set_level(GPIO_NUM_19, 0));
}

bool connectToServer(BLEAdvertisedDevice &device,
                     TypeOfDevice deviceType,
                     BLEClient *pClient) {
    switch (deviceType) {
        case TI:
            LOG("Forming a connection (TI) to %s \n", device.getAddress().toString().c_str());
            break;
        case Nordic:
            LOG("Forming a connection (Nordic) to %s \n", device.getAddress().toString().c_str());
            break;
        case Custom:
            LOG("Forming a connection (Custom) to %s \n", device.getAddress().toString().c_str());
            break;
        case Hub:
            LOG("Forming a connection (Hub) to %s \n", device.getAddress().toString().c_str());
            break;
    }

    int res = pClient->connect(&device);
    delay(5000);
    auto s = pClient->getServices();
    NimBLEDevice::setMTU(517); //set client to request maximum MTU from server (default is 23 otherwise)

    // Obtain a reference to the service we are after in the remote BLE server.
    vector<BLEUUID> *vectorUUID;
    switch (deviceType) {
        case TI:
            vectorUUID = &serviceTIUUIDs;
            break;
        case Nordic:
            vectorUUID = &serviceNordicUUIDs;
            break;
        case Custom:
            vectorUUID = &servicePicoUUIDs;
            break;
        case Hub:
            vectorUUID = &serviceHubUUIDs;
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
            found = true;
        }
    }
    if (!found) {
        LOG("Failed to find service UUID %s\n", pClient->getConnInfo().getAddress().toString().c_str());
        return false;
    }
    if (pRemoteService == nullptr) {
        LOG("pRemoteService == nullptr\n");
        return false;

    }

    if (deviceType == TypeOfDevice::TI) {
        // Obtain a reference to the characteristic in the service of the remote BLE server.
        pRemoteTIWriteCharacteristic = pRemoteService->getCharacteristic(charTISendUUID);

        if (pRemoteTIWriteCharacteristic == nullptr) {
            LOG("Failed to find our characteristic UUID (TI) write: %s\n", charTISendUUID.toString().c_str());
            pClient->disconnect();
            return false;
        }
        if (pRemoteTIWriteCharacteristic->canWrite()) {
            pRemoteTIWriteCharacteristic->writeValue((char) 1, true);
            auto val = pRemoteTIWriteCharacteristic->readValue();
            int returnLen = val.length();
        } else {
            LOG("Can't write to write: \n");
            LOG("%s\n", charTISendUUID.toString().c_str());
            pClient->disconnect();
            return false;
        }
    } else if (deviceType == TypeOfDevice::Hub) {
        // Obtain a reference to the characteristic in the service of the remote BLE server.
        pRemoteHubCharacteristic = pRemoteService->getCharacteristic(charHubUUID);

        if (pRemoteHubCharacteristic == nullptr) {
            LOG("Failed to find our characteristic UUID (hub): %s\n", charHubUUID.toString().c_str());
            pClient->disconnect();
            return false;
        }

        if (pRemoteHubCharacteristic->canWrite()) {

            BLESendPacket p = BLESendPacket_init_zero;
            p.which_type = BLESendPacket_crossDevicePacket_tag;

            deque<tuple<std::string, TypeOfDevice>> tmpAddresses;
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
                    memcpy(&sList.sensor_info[i].address, name.c_str(), 21);
                    auto type = std::get<1>(tmpAddresses[i]);
                    switch (type) {
                        case TI:
                            sList.sensor_info[i].device_type = SensorInfoInterDevice_DEVICE_TYPE_TI;
                            break;
                        case Nordic:
                            sList.sensor_info[i].device_type = SensorInfoInterDevice_DEVICE_TYPE_NORDIC;
                            break;
                        case Hub:
                            sList.sensor_info[i].device_type = SensorInfoInterDevice_DEVICE_TYPE_HUB;
                            break;
                        case Custom:
                            sList.sensor_info[i].device_type = SensorInfoInterDevice_DEVICE_TYPE_CUSTOM;
                            break;
                    }
                }
            }
            {
                pb_size_t sizeOfVList = std::min(tmpSensorData.size(), static_cast<size_t>(64));
                vList.values_count = sizeOfVList;
                int i = 0;
                for (auto &iter: tmpSensorData) {
                    if (i == sizeOfVList) {
                        break;
                    }
                    auto name = iter.second.address;
                    memcpy(&vList.values[i].address, name.c_str(), 21);
                    auto value = iter.second;
                    vList.values[i].timestamp = value.timestamp;
                    vList.values[i].value = value.value;
                    switch (value.measure_type) {
                        case TEMP:
                            vList.values[i].measure_type = ValuesInterDevice_MEASURE_TYPE_TEMP;
                            break;
                        case HUMIDITY:
                            vList.values[i].measure_type = ValuesInterDevice_MEASURE_TYPE_HUMIDITY;
                            break;
                        case DHT11_TEMP:
                            vList.values[i].measure_type = ValuesInterDevice_MEASURE_TYPE_DHT11_TEMP;
                            break;
                        case DHT22_TEMP:
                            vList.values[i].measure_type = ValuesInterDevice_MEASURE_TYPE_DHT22_TEMP;
                            break;
                        case DHT11_HUMIDITY:
                            vList.values[i].measure_type = ValuesInterDevice_MEASURE_TYPE_DHT11_HUMIDITY;
                            break;
                        case DHT22_HUMIDITY:
                            vList.values[i].measure_type = ValuesInterDevice_MEASURE_TYPE_DHT22_HUMIDITY;
                            break;
                        case PICO_TEMP:
                            vList.values[i].measure_type = ValuesInterDevice_MEASURE_TYPE_PICO_TEMP;
                            break;
                    }
                    switch (value.type) {
                        case TI:
                            vList.values[i].device_type = ValuesInterDevice_DEVICE_TYPE_TI;
                            break;
                        case Nordic:
                            vList.values[i].device_type = ValuesInterDevice_DEVICE_TYPE_NORDIC;
                            break;
                        case Hub:
                            vList.values[i].device_type = ValuesInterDevice_DEVICE_TYPE_HUB;
                            break;
                        case Custom:
                            vList.values[i].device_type = ValuesInterDevice_DEVICE_TYPE_CUSTOM;
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

            vector<pb_byte_t> buf(2024);
            pb_ostream_t output = pb_ostream_from_buffer(buf.data(), buf.size());

            int status = pb_encode(&output, BLESendPacket_fields, &p);
            if (!status) {
                throw std::runtime_error(string("Encoding failed: ") + PB_GET_ERROR(&output));
            }

            pRemoteHubCharacteristic->writeValue(output.bytes_written, true);
            std::vector<uint8_t> buf1;
            for (int i = 0; i < output.bytes_written; i++) {
                buf1.push_back(buf.at(i));
            }
            pRemoteHubCharacteristic->writeValue(buf1);
        } else {
            LOG("Can't write to write: %s\n", charHubUUID.toString().c_str());
            pClient->disconnect();
            return false;
        }
    }
    pRemoteReadCharacteristics[MeasureType::TEMP] = nullptr;
    pRemoteReadCharacteristics[MeasureType::HUMIDITY] = nullptr;

    switch (deviceType) {
        case TI:
            pRemoteReadCharacteristics[MeasureType::TEMP] = pRemoteService->getCharacteristic(charTIRecUUID);
            break;
        case Nordic: {
            pRemoteReadCharacteristics[MeasureType::TEMP] = pRemoteService->getCharacteristic(charNordicTempUUID);
            pRemoteReadCharacteristics[MeasureType::HUMIDITY] = pRemoteService->getCharacteristic(
                    charNordicHumidityUUID);
            break;
        }
        case Custom:
            pRemoteReadCharacteristics[MeasureType::TEMP] = pRemoteService->getCharacteristic(charPicoUUID);
            break;
        case Hub:
            pRemoteReadCharacteristics[MeasureType::TEMP] = pRemoteService->getCharacteristic(charHubUUID);
            break;
        default:
            throw std::runtime_error("What's it doing here");
    }

    for (const auto [type, pRemoteReadCharacteristic]: pRemoteReadCharacteristics) {
        if (pRemoteReadCharacteristic == nullptr) {
            continue;
        }
        if (pRemoteReadCharacteristic->canNotify()) {
            switch (deviceType) {

                case TI:
                    LOG("TI\n");
                    pRemoteReadCharacteristic->subscribe(true, notifyTICallback, true);
                    break;

                case Nordic:
                    LOG("Nordic: %s\n", type == MeasureType::HUMIDITY ? "Humidity" : "Temperature");

                    pRemoteReadCharacteristic->subscribe(true,
                                                         type == MeasureType::HUMIDITY ? notifyNordicCallbackHumidity
                                                                                       : notifyNordicCallbackTemp,
                                                         true);
                    break;
                case Custom: {
                    hit_null.lockAndSwap(false);
                    pRemoteReadCharacteristic->subscribe(true, notifyCustomCallback, true);

                }
                case Hub:
                    break;
            }
            LOG("After switch\n");
        }
        LOG("After if\n");
    }
    LOG("After for\n");
    return true;
}

struct ParamArgs {
    BLEAdvertisedDevice dev;
    TypeOfDevice deviceType = TypeOfDevice::TI;
    BLEClient *pClient{};
};


[[noreturn]] void innerConnectToServer(void *parameters) {
    {
        ESP_ERROR_CHECK(esp_task_wdt_add(nullptr));
        auto pa = (ParamArgs *) parameters;
        connectToServer(pa->dev, pa->deviceType, pa->pClient);
    }
    LOG("About to close innerConnectToServer\n");
    ESP_ERROR_CHECK(esp_task_wdt_delete(nullptr));

    vTaskDelete(nullptr);
    LOG("Assertion error: After vTaskDelete\n");
    for (;;) {}
}

void GetSensorData::loop() {
    LOG("At %d\n", __LINE__);
    bool addressesEmpty;
    {
        LOG("At %d\n", __LINE__);
        addressesEmpty = addresses.lock()->empty();
        LOG("At %d\n", __LINE__);
    }
    if (addressesEmpty) {
        LOG("Addresses are empty\n");
        delay(500);
        LOG("At %d\n", __LINE__);
        return;
    }
    LOG("At %d\n", __LINE__);
    tuple<string, TypeOfDevice> curAddress;
    LOG("At %d\n", __LINE__);
    {
        LOG("At %d\n", __LINE__);
        auto lock = addresses.lock();
        LOG("At %d\n", __LINE__);
        curAddress = lock->front();
        LOG("At %d\n", __LINE__);
        lock->pop_front();
        LOG("At %d\n", __LINE__);
    }
    LOG("At %d\n", __LINE__);
    {
        LOG("At %d\n", __LINE__);
        auto l = addresses.lock();
        LOG("At %d\n", __LINE__);
        l->emplace_back(curAddress);
        LOG("At %d\n", __LINE__);
        for (int i = 0; i < l->size(); i++) {
            LOG("l[%d]=%s; %d\n", i, std::get<0>(l->at(i)).c_str(), std::get<1>(l->at(i)));
        }
        LOG("At %d\n", __LINE__);
    }
    LOG("At %d\n", __LINE__);
    delay(100);

    ScanResults scanResultsClass;
    LOG("At %d\n", __LINE__);
    auto scanResults = scanResultsClass.getScanResults();
    LOG("At %d\n", __LINE__);
    for (int i = 0; i < scanResults.getCount(); i++) {
        LOG("At %d\n", __LINE__);
        for (const auto [type, pRemoteReadCharacteristic]: pRemoteReadCharacteristics) {
            LOG("At %d\n", __LINE__);
            if (pRemoteReadCharacteristic != nullptr) {
                LOG("At %d\n", __LINE__);
                bool success = pRemoteReadCharacteristic->unsubscribe();
                LOG("At %d, %d\n", __LINE__, success);

                LOG("At %d\n", __LINE__);
            }
            LOG("At %d\n", __LINE__);
        }
        auto dev = scanResults.getDevice(i);
        LOG("At %d\n", __LINE__);
        if (dev.getAddress().toString() == std::get<0>(curAddress)) {
            LOG("At %d\n", __LINE__);
            TaskHandle_t Task1;
            ParamArgs pa{
                    .dev = dev,
                    .deviceType = std::get<1>(curAddress),
                    .pClient = pClient,
            };


            LOG("At %d\n", __LINE__);
            auto ret = xTaskCreate(innerConnectToServer, "Connect to Server", 16'000, (void *) &pa, 1, &Task1);
            if (ret != pdPASS) {
                LOG("At %d\n", __LINE__);
                LOG("Ret = %d\n", ret);
                throw runtime_error("Couldn't create thread");
            }
            LOG("At %d\n", __LINE__);
            delay(10'000);
            LOG("At %d\n", __LINE__);
            if (pClient != nullptr && pClient->isConnected()) {
                LOG("At %d\n", __LINE__);
                pClient->disconnect();
                LOG("At %d\n", __LINE__);
            }
            LOG("At %d\n", __LINE__);
        }
        LOG("At %d\n", __LINE__);
    }

    LOG("At %d\n", __LINE__);
}

GetSensorData::GetSensorData() {
    pClient = NimBLEDevice::createClient();
}

void GetSensorData::clearDevices() {
    addresses.lock()->clear();

}

void GetSensorData::setDevices(std::vector<std::tuple<std::string, TypeOfDevice>> &newDevice) {

    std::vector<std::tuple<std::basic_string<char>, TypeOfDevice>> newVec;
    auto lock = addresses.lock();
    for (const auto &e: *lock) {
        if (contains(newDevice, e)) {
            newVec.emplace_back(e);
        }
    }
    for (const auto &e: newDevice) {
        if (!contains(newVec, e)) {
            newVec.emplace_back(e);
        }
    }
    lock->clear();
    for (const auto &e: newVec) {
        lock->emplace_back(e);
    }
}

