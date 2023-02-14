#include <unordered_map>
#include <hal/gpio_types.h>
#include <driver/gpio.h>
#include "ScanResults.h"
#include "GetSensorData.h"
#include "Constants.h"
#include "SensorDataStore.h"
#include "generated/packet.pb.h"
#include "../components/nanopb/pb_encode.h"
#include "exceptions/DecodeException.h"
#include "getTime.h"
#include "lib/websocket/websocket.h"
#include "generated/firmware_backend.pb.h"
#include "lib/ArduinoSupport/ArduinoSupport.h"


GetSensorData *getGetSensorData() {
    static GetSensorData _sensorData;
    return &_sensorData;
}

void nordicCallbackProcess(BLERemoteCharacteristic *pBLERemoteCharacteristic, const uint8_t *pData, size_t length,
                           MeasureType type);

static std::vector<BLEUUID> serviceNordicUUIDs(
        {BLEUUID("ef680200-9b35-4933-9b10-52ffa9740042"), BLEUUID("ef680300-9b35-4933-9b10-52ffa9740042"),
         BLEUUID("ef680400-9b35-4933-9b10-52ffa9740042"), BLEUUID("ef680500-9b35-4933-9b10-52ffa9740042")});
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

void notifyCustomCallback(BLERemoteCharacteristic *pBLERemoteCharacteristic, const uint8_t *pData, size_t length,
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

                    remoteAddress = pBLERemoteCharacteristic->getRemoteService()->getClient()->getConnInfo().getAddress().toString();

            lastGotData.lockAndSwap(getTime());
            auto time = getTime();
            SensorDataStore sensorDataStore = SensorDataStore{.timestamp = time, .address = remoteAddress, .type = TypeOfDevice::Custom, .value = val, .measure_type = type,};
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
                default:
                    throw std::runtime_error("Wrong measure type");

            }

            FirmwareToBackendPacket_type_sensor_data_MSGTYPE p = {0};
            for (int i = 0; i < std::min(static_cast<unsigned int>(21),
                                         static_cast<unsigned int>(sensorDataStore.address.size())); i++) {
                p.address[i] = sensorDataStore.address.at(i);
            }
            p.data_type = data_type;
            p.value = sensorDataStore.value;
            p.timestamp = sensorDataStore.timestamp;
            packet.get()->type = {0};
            packet.get()->type.sensor_data = p;

            vector<uint8_t> buf(2048);
            pb_ostream_t output = pb_ostream_from_buffer(buf.data(), buf.size());
            int status = pb_encode(&output, FirmwareToBackendPacket_fields, packet.get());
            if (!status) {
                throw std::runtime_error(string("Decoding failed: ") + PB_GET_ERROR(&output));
            }
            buf.resize(output.bytes_written);
            LOG("Got custom data: %f\n", sensorDataStore.value);

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

void notifyNordicCallbackTemp(BLERemoteCharacteristic *pBLERemoteCharacteristic, const uint8_t *pData, size_t length,
                              bool isNotify) {
    nordicCallbackProcess(pBLERemoteCharacteristic, pData, length, MeasureType::TEMP);
}

void
notifyNordicCallbackHumidity(BLERemoteCharacteristic *pBLERemoteCharacteristic, const uint8_t *pData, size_t length,
                             bool isNotify) {
    nordicCallbackProcess(pBLERemoteCharacteristic, pData, length, MeasureType::HUMIDITY);
}

void nordicCallbackProcess(BLERemoteCharacteristic *pBLERemoteCharacteristic, const uint8_t *pData, size_t length,
                           MeasureType type) {
    ESP_ERROR_CHECK(gpio_set_level(GPIO_NUM_19, 1));
    int low = pData[0];
    int high = pData[1];
    std::string remoteAddress = pBLERemoteCharacteristic->getRemoteService()->getClient()->getConnInfo().getAddress().toString();

    lastGotData.lockAndSwap(getTime());
    auto time = getTime();

    float temperature = std::stof(std::to_string(low) + "." + std::to_string(high));
    SensorDataStore sensorDataStore = SensorDataStore{.timestamp = time, .address = remoteAddress, .type = TypeOfDevice::Nordic, .value = temperature, .measure_type = type,

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
        default:
            throw std::runtime_error("Wrong measure type");
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
    vector<uint8_t> buf(2048);
    pb_ostream_t output = pb_ostream_from_buffer(buf.data(), buf.size());
    int status = pb_encode(&output, FirmwareToBackendPacket_fields, packet.get());
    if (!status) {
        throw std::runtime_error(string("Encoding failed: ") + PB_GET_ERROR(&output));
    }
    buf.resize(output.bytes_written);

    WriteSocketError error = websocket::getInstance()->writeBytes(buf, 5'000);
    if (error == WriteSocketError::WriteError && websocket::getInstance()->isConnected()) {
        throw std::runtime_error("Cannot send data to websocket");
    }


    LOG("Got nordic data: %f\n", sensorDataStore.value);

    lastGotData.lockAndSwap(getTime());
    ESP_ERROR_CHECK(gpio_set_level(GPIO_NUM_19, 0));
}

void notifyTICallback(BLERemoteCharacteristic *pBLERemoteCharacteristic, uint8_t *pData, size_t length, bool isNotify) {
    ESP_ERROR_CHECK(gpio_set_level(GPIO_NUM_19, 1));
    std::string remoteAddress = pBLERemoteCharacteristic->getRemoteService()->getClient()->getConnInfo().getAddress().toString();
    assert(length == 4);
    static_assert(sizeof(float) == 4, "float size is expected to be 4 bytes");
    float f;
    memcpy(&f, pData, 4);
    SensorDataStore sensorDataStore = SensorDataStore{.timestamp = getTime(), .address = remoteAddress, .type = TypeOfDevice::TI, .value = f, .measure_type = MeasureType::TEMP,};
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
        default: {
            throw std::runtime_error("Assertion error: invalid type");
        }
    }

    FirmwareToBackendPacket_type_sensor_data_MSGTYPE p = {0};
    for (int i = 0;
         i < std::min(static_cast<unsigned int>(21), static_cast<unsigned int>(sensorDataStore.address.size())); i++) {
        p.address[i] = sensorDataStore.address.at(i);
    }
    p.data_type = data_type;
    p.value = sensorDataStore.value;
    p.timestamp = sensorDataStore.timestamp;

    vector<uint8_t> buf(2048);
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

bool connectToServer(BLEAdvertisedDevice &device, TypeOfDevice deviceType) {
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

    NimBLEClient *c;
    {
        auto l = getGetSensorData()->pClient.lock();
        if (l->find(device.getAddress()) == l->end()) {
            c = NimBLEDevice::createClient(device.getAddress());
            l->insert_or_assign(device.getAddress(), c);
        } else {
            c = l->find(device.getAddress())->second;
        }
    }
    c->connect(&device);
    delay(5000);
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
        pRemoteService = c->getService(vectorUUID->at(i));
        if (pRemoteService == nullptr) {
            if (c->isConnected()) {
                c->disconnect();
            }
        } else {
            found = true;
        }
    }
    if (!found) {
        LOG("Failed to find service UUID %s\n", c->getConnInfo().getAddress().toString().c_str());
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
            c->disconnect();
            return false;
        }
        if (pRemoteTIWriteCharacteristic->canWrite()) {
            pRemoteTIWriteCharacteristic->writeValue((char) 1, true);
            auto val = pRemoteTIWriteCharacteristic->readValue();
            int returnLen = val.length();
        } else {
            LOG("Can't write to write: \n");
            LOG("%s\n", charTISendUUID.toString().c_str());
            c->disconnect();
            return false;
        }
    } else if (deviceType == TypeOfDevice::Hub) {
        // Obtain a reference to the characteristic in the service of the remote BLE server.
        pRemoteHubCharacteristic = pRemoteService->getCharacteristic(charHubUUID);

        if (pRemoteHubCharacteristic == nullptr) {
            LOG("Failed to find our characteristic UUID (hub): %s\n", charHubUUID.toString().c_str());
            c->disconnect();
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

            p.type.crossDevicePacket = CrossDevicePacket{.has_sensorList = true, .sensorList = sList, .has_values = true, .values = vList, .timestamp = getTime(),};

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
            c->disconnect();
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
                    pRemoteReadCharacteristic->subscribe(true, notifyTICallback);
                    break;

                case Nordic:
                    LOG("Nordic: %s\n", type == MeasureType::HUMIDITY ? "Humidity" : "Temperature");

                    pRemoteReadCharacteristic->subscribe(true,
                                                         type == MeasureType::HUMIDITY ? notifyNordicCallbackHumidity
                                                                                       : notifyNordicCallbackTemp);
                    break;
                case Custom: {
                    hit_null.lockAndSwap(false);
                    pRemoteReadCharacteristic->subscribe(true, notifyCustomCallback);

                }
                case Hub:
                    break;
            }
            LOG("After switch\n");
        }
        LOG("After if\n");
    }
    LOG("After for\n");
    delay(MS_TO_STAY_CONNECTED);
    c->disconnect();
    return true;
}

struct ParamArgs {
    BLEAdvertisedDevice dev;
    TypeOfDevice deviceType = TypeOfDevice::TI;
};


void vTaskGetRunTimeStats() {

    TaskStatus_t *pxTaskStatusArray;
    volatile UBaseType_t uxArraySize, x;
    unsigned long ulTotalRunTime, ulStatsAsPercentage;


    /* Take a snapshot of the number of tasks in case it changes while this
    function is executing. */
    uxArraySize = uxTaskGetNumberOfTasks();

    /* Allocate a TaskStatus_t structure for each task.  An array could be
    allocated statically at compile time. */
    pxTaskStatusArray = static_cast<TaskStatus_t *>(pvPortMalloc(uxArraySize * sizeof(TaskStatus_t)));
    printf("Heap size: %lu\n\n", xPortGetFreeHeapSize());
    if (pxTaskStatusArray != NULL) {
        printf("Not null\n");
        /* Generate raw status information about each task. */
        uxArraySize = uxTaskGetSystemState(pxTaskStatusArray, uxArraySize, &ulTotalRunTime);

        /* For percentage calculations. */
        ulTotalRunTime /= 100UL;

        /* Avoid divide by zero errors. */
        if (ulTotalRunTime > 0) {
            printf(">0\n");
            /* For each populated position in the pxTaskStatusArray array,
            format the raw data as human readable ASCII data. */
            printf("arr size:%d\n", uxArraySize);
            for (x = 0; x < uxArraySize; x++) {
                /* What percentage of the total run time has the task used?
                This will always be rounded down to the nearest integer.
                ulTotalRunTimeDiv100 has already been divided by 100. */
                ulStatsAsPercentage = pxTaskStatusArray[x].ulRunTimeCounter / ulTotalRunTime;

                if (ulStatsAsPercentage > 0UL) {
                    printf("%stt%lutt %lu%%rn", pxTaskStatusArray[x].pcTaskName, pxTaskStatusArray[x].ulRunTimeCounter,
                           ulStatsAsPercentage);
                } else {
                    /* If the percentage is zero here then the task has
                    consumed less than 1% of the total run time. */
                    printf("%stt%lutt <1%%rn", pxTaskStatusArray[x].pcTaskName, pxTaskStatusArray[x].ulRunTimeCounter);
                }
                printf(" uptime: %lld %lld\n", esp_timer_get_time(), time(nullptr));
            }
        }

        /* The array is no longer needed, free the memory it consumes. */
        vPortFree(pxTaskStatusArray);
    }
}

[[noreturn]] void innerConnectToServer(void *parameters) {
    LOG("In inner connect to server\n");
    {
        auto pa = (ParamArgs *) parameters;
        connectToServer(pa->dev, pa->deviceType);
    }
    LOG("About to close innerConnectToServer\n");
    for (;;) {}
}

#define STACK_SIZE 32'000
StackType_t xStack[STACK_SIZE];
TaskHandle_t xHandle = NULL;

StaticTask_t xTaskBuffer;
std::mutex m;

void GetSensorData::loop() {
    bool addressesEmpty;
    {
        addressesEmpty = addresses.lock()->empty();
    }
    if (addressesEmpty) {
        delay(500);
        return;
    }
    tuple<string, TypeOfDevice> curAddress;
    {
        auto lock = addresses.lock();
        curAddress = lock->front();
        lock->pop_front();
    }
    {
        auto l = addresses.lock();
        l->emplace_back(curAddress);
        for (int i = 0; i < l->size(); i++) {
            LOG("l[%d]=%s; %d\n", i, std::get<0>(l->at(i)).c_str(), std::get<1>(l->at(i)));
        }
    }
    delay(100);

    ScanResults scanResultsClass;
    auto scanResults = scanResultsClass.getScanResults();
    LOG("Scan Result get count: %d\n", scanResults.getCount())
    for (int i = 0; i < scanResults.getCount(); i++) {
        for (const auto [type, pRemoteReadCharacteristic]: pRemoteReadCharacteristics) {
            if (pRemoteReadCharacteristic != nullptr) {
                bool success = pRemoteReadCharacteristic->unsubscribe();

            }
        }
        auto dev = scanResults.getDevice(i);
        LOG("Device obtained: %s, curAddres: %s\n", dev.getAddress().toString().c_str(),
            std::get<0>(curAddress).c_str());
        if (dev.getAddress().toString() == std::get<0>(curAddress)) {
            printf("Found\n\n");
            ParamArgs pa{.dev = dev, .deviceType = std::get<1>(curAddress),};
            vTaskGetRunTimeStats();
            m.lock();


            xHandle = xTaskCreateStatic(innerConnectToServer, "Connect to Server", STACK_SIZE, (void *) &pa, 1, xStack,
                                        &xTaskBuffer);
            delay(MS_TO_STAY_CONNECTED + 10'000);
            vTaskDelete(xHandle);
            m.unlock();
        }
        LOG("At %d\n", __LINE__);
    }
}

GetSensorData::GetSensorData() {
    pClient.lockAndSwap(std::unordered_map<std::string, NimBLEClient *>());
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

