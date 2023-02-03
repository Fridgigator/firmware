#include <Arduino.h>
#include "NimBLEDevice.h"
#include "WiFi.h"
#include <vector>
#include <cstring>
#include <Preferences.h>
#include <map>
#include <esp_websocket_client.h>
#include <esp_task_wdt.h>

#include "exceptions/StateException.h"
#include "uuid.h"
#include "Constants.h"
#include "GetSensorData.h"
#include "pb_encode.h"
#include "generated/firmware_backend.pb.h"
#include "pb_decode.h"
#include "BLEUtils.h"
#include "setClock.h"
#include "lib/log.h"
#include "lib/websocket/websocket.h"
#include "secrets.h"

using namespace std;
bool timeSet = false;
NimBLECharacteristic *pRead = nullptr;

safe_std::mutex<bool> bleIsInUse;

class CharacteristicCallback : public NimBLECharacteristicCallbacks {
    void onWrite(NimBLECharacteristic *ch) override {

    }

    void onRead(NimBLECharacteristic *ch) override {
    }
};

class ServerCallbacks : public NimBLEServerCallbacks {
    void onConnect(NimBLEServer *pServer) override {
        bleIsInUse.lockAndSwap(true);
    }

    void onDisconnect(NimBLEServer *pServer) override {

        bleIsInUse.lockAndSwap(false);
    }

};

vector<uint8_t> getData(NimBLECharacteristic *pRead, NimBLECharacteristic *pWrite);

CharacteristicCallback *c_callback;
optional<GetSensorData> getSensorData;

void loop1();

void clientConnectLoop();

string uuid;

void getSensorsList() {
    ScanResults scanResultsClass;
    auto res = scanResultsClass.getScanResults();
    vector<SensorInfo> sensorInfo;
    for (int i = 0; i < res.getCount(); i++) {
        auto device = res.getDevice(i);
        string addressString = device.getAddress().toString();
        string nameString = device.getName();
        if (nameString.empty()) {
            nameString = addressString;
        }
        SensorInfo info{};
        int len = 0;
        for (int j = 0; j < addressString.size() && j < sizeof(info.address) / sizeof(info.address[0]); j++) {
            info.address[j] = addressString.at(j);
            len = j;

        }
        info.address[len + 1] = 0;
        len = 0;

        for (int j = 0; j < nameString.size() && j < sizeof(info.name) / sizeof(info.name[0]); j++) {
            info.name[j] = nameString.at(j);
            len = j;
        }
        info.name[len + 1] = 0;
        sensorInfo.push_back(info);
    }
    res.getCount();
    vector<pb_byte_t> buf(1024);
    pb_ostream_t output = pb_ostream_from_buffer(buf.data(), buf.size());
    SensorsList sensorsList{.sensor_infos_count = static_cast<pb_size_t>(sensorInfo.size()),};

    for (int i = 0;
         i < sensorInfo.size() && i < sizeof(sensorsList.sensor_infos) / sizeof(sensorsList.sensor_infos[0]); i++) {

        sensorsList.sensor_infos[i] = sensorInfo.at(i);

    }
    int encodeResult = pb_encode(&output, SensorsList_fields, &sensorsList);
    if (!encodeResult) {
        throw std::runtime_error(string("Encoding failed: ") + PB_GET_ERROR(&output));
    }
    std::map<string, string> headers;
    headers.emplace("Board", uuid);
    std::string url = "/api/v1/send-sensors";
    buf.resize(output.bytes_written);
}

void addSensors(const unique_ptr<BackendToFirmwarePacket> &packet) {
    vector<std::tuple<std::string, TypeOfDevice>> newDevices;
    for (int i = 0; i < packet->type.add_sensor.add_sensor_infos_count; i++) {
        auto addSensorInfo = packet->type.add_sensor.add_sensor_infos[i];
        std::string address = addSensorInfo.sensor_info.address;
        TypeOfDevice deviceType;
        switch (addSensorInfo.device_type) {
            case DeviceType_DEVICE_TYPE_TI:
                deviceType = TypeOfDevice::TI;
                break;
            case DeviceType_DEVICE_TYPE_NORDIC:
                deviceType = TypeOfDevice::Nordic;
                break;
            case DeviceType_DEVICE_TYPE_CUSTOM:
                deviceType = TypeOfDevice::Custom;
                break;
            case DeviceType_DEVICE_TYPE_HUB:
                deviceType = TypeOfDevice::Hub;
                break;
            case DeviceType_DEVICE_TYPE_UNSPECIFIED:
                throw std::runtime_error("Assertion error: addSensorInfo.device_type is unspecified");
        }
        newDevices.emplace_back(address, deviceType);
    }
    getSensorData->setDevices(newDevices);
}

void recData(unique_ptr<BackendToFirmwarePacket> &packet) {
    TaskHandle_t Task3;

    // I *think* that this has to be a thread because it's called from a websocket callback. It seems that there's
    // some kind of deadlock going on here if I don't
    // TODO: verify if there is a requirement to have this as its own thread.

    auto ret = xTaskCreate([](void *arg) {
        ESP_ERROR_CHECK(esp_task_wdt_add(nullptr));
        // Ensure that all heap allocations are freed by the end of the function call
        // vTaskDelete prevents the destructors on unique_ptr from being run, thus causing memory leaking

        {

            unique_ptr<BackendToFirmwarePacket> packet1 = unique_ptr<BackendToFirmwarePacket>(
                    reinterpret_cast<BackendToFirmwarePacket *>(arg));
            switch (packet1->which_type) {
                case BackendToFirmwarePacket_get_sensors_list_tag: {
                    getSensorsList();
                    break;
                }
                case BackendToFirmwarePacket_clear_sensor_list_tag: {
                    getSensorData->clearDevices();
                    break;
                }
                case BackendToFirmwarePacket_add_sensor_tag: {
                    addSensors(packet1);
                    break;
                }
                default: {
                    throw runtime_error("Assertion error: packet1 has wrong type");
                }
            }
        }
        ESP_ERROR_CHECK(esp_task_wdt_delete(nullptr));

        vTaskDelete(nullptr);
    }, "recData", 16000, packet.release(), 1, &Task3);
    if (ret != pdPASS) {
        LOG("Error: %d\n", ret);
        throw runtime_error("Couldn't create thread");
    }
}

[[noreturn]]
void outerLoop1(void *) {
    for (;;) {
        loop1();
        delay(100);
    }

}

void setup() {
    // Blink all lights for 3 seconds
    gpio_config_t config = {
            .pin_bit_mask = (1ULL << GPIO_NUM_23) | (1ULL << GPIO_NUM_19) | (1ULL << GPIO_NUM_18) |
                            (1ULL << GPIO_NUM_17),
            .mode =  GPIO_MODE_OUTPUT,
            .pull_up_en = GPIO_PULLUP_DISABLE,
            .pull_down_en = GPIO_PULLDOWN_DISABLE,
            .intr_type = GPIO_INTR_DISABLE,

    };
    ESP_ERROR_CHECK(gpio_config(&config));
    for (int i = 0; i < 6; i++) {
        ESP_ERROR_CHECK(gpio_set_level(GPIO_NUM_23, 1));
        ESP_ERROR_CHECK(gpio_set_level(GPIO_NUM_19, 1));
        ESP_ERROR_CHECK(gpio_set_level(GPIO_NUM_18, 1));
        ESP_ERROR_CHECK(gpio_set_level(GPIO_NUM_17, 1));

        delay(250);
        ESP_ERROR_CHECK(gpio_set_level(GPIO_NUM_23, 0));
        ESP_ERROR_CHECK(gpio_set_level(GPIO_NUM_19, 0));
        ESP_ERROR_CHECK(gpio_set_level(GPIO_NUM_18, 0));
        ESP_ERROR_CHECK(gpio_set_level(GPIO_NUM_17, 0));
        delay(250);
    }

    // Turn on red LED when we're connected to the LED
    xTaskCreate([](void *arg) {
        for (;;) {
            // Is connected and time is set up
            if (WiFi.isConnected() && timeSet) {
                ESP_ERROR_CHECK(gpio_set_level(GPIO_NUM_23, 1));
            } else {
                ESP_ERROR_CHECK(gpio_set_level(GPIO_NUM_23, 0));
            }

            // Are we connected to the websocket?
            if (websocket::getInstance()->isConnected()) {
                ESP_ERROR_CHECK(gpio_set_level(GPIO_NUM_17, 1));
            } else {
                ESP_ERROR_CHECK(gpio_set_level(GPIO_NUM_17, 0));
            }

            delay(250);
        }
    }, "WiFi loop", 1024, nullptr, 1, nullptr);


    WiFiClass::mode(WIFI_STA);
    getSensorData = GetSensorData();
    ESP_ERROR_CHECK(esp_task_wdt_init(60, true));
    Serial.begin(115200);

    c_callback = new CharacteristicCallback();
    Preferences preferences;
    preferences.begin("permanent", false);
    uuid = preferences.getString("uuid").c_str();
    if (!preferences.isKey("uuid")) {
        char returnUUID[37];
        UUIDGen(returnUUID);
        preferences.putString("uuid", returnUUID);
        uuid = preferences.getString("uuid").c_str();
    }
    preferences.end();
    std::string name = "ESP-" + uuid;
    name.erase(15, std::string::npos);
    NimBLEDevice::init(name);
    NimBLEServer *pServer = BLEDevice::createServer();
    NimBLEService *pService = pServer->createService(SERVICE_UUID);

    pRead = pService->createCharacteristic(CHARACTERISTIC_SERVER_UUID,
                                           NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::INDICATE);
    pRead->setCallbacks(c_callback);
    pService->start();
    NimBLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
    pAdvertising->addServiceUUID(SERVICE_UUID);
    pAdvertising->setScanResponse(true);
    pAdvertising->setMinPreferred(0x06);  // functions that help with iPhone connections issue
    pAdvertising->setMinPreferred(0x12);
    NimBLEDevice::startAdvertising();
    NimBLEDevice::stopAdvertising();
    NimBLEDevice::startAdvertising();

    auto *pServerCallbacks = new ServerCallbacks();
    pServer->setCallbacks(pServerCallbacks);

    TaskHandle_t Task1;

    delay(100);
    TaskHandle_t Task2;

    xTaskCreate(outerLoop1, "Main Loop Task", 8000, nullptr, 1, &Task2);

    LOG("Finished Setup!");
    TaskHandle_t Task3;

    /*xTaskCreate([](void *arg) {
        for (;;) {

            delay(60'000 * 60);
            LOG("Restarting on the hour");
            BLEDevice::deinit(true);
            esp_restart();
        }
    }, "Cycle BLE", 8000, nullptr, 1, &Task3);
     */
}

void onWebsocketConnect(const WebsocketConnectionType type, int size, const char *data) {
    switch (type) {
        case WebsocketConnectionType::Any: {
            LOG("Connect type: Any\n");
            break;
        }
        case WebsocketConnectionType::Connected: {
            LOG("Connect type: Connected\n");
            break;
        }
        case WebsocketConnectionType::Disconnected: {
            LOG("Connect type: Disconnected\n");
            break;
        }

        case WebsocketConnectionType::Closed: {
            LOG("Connect type: Closed\n");
            break;
        }
        case WebsocketConnectionType::Error: {
            LOG("Connect type: Error\n");
            break;
        }
        case WebsocketConnectionType::Data: {
            if (size == 0) {
                break;
            }


            unique_ptr<BackendToFirmwarePacket> message = make_unique<BackendToFirmwarePacket>();
            *message = BackendToFirmwarePacket_init_zero;
            auto stream = make_unique<pb_istream_t>();
            *stream = pb_istream_from_buffer(reinterpret_cast<const pb_byte_t *>(data), size);
            bool status = pb_decode(stream.get(), BackendToFirmwarePacket_fields, message.get());
            if (!status) {
                throw std::runtime_error("Stream decode bug");
            }
            recData(message);

            break;
        }
        case WebsocketConnectionType::Max: {
            throw std::runtime_error("Connect type: Max. What's that?!?!\n");
        }
    }
}

bool hasConnected = false;

// This is the thread responsible for communicating with the backend.
// By making it a thread, I can control how large of a stack space it gets. I can also pause it
// until either it needs to ping or it needs to send data
// It returns the TaskID to which data should be sent
void clientConnectLoop() {

    xTaskCreate([](void *parameters) {
        ESP_ERROR_CHECK(esp_task_wdt_add(nullptr));
        string const url = std::string(REMOTE_HOST_WS) + "/api/v1/hub-connect?Board=" + uuid;

        for (;;) {
            ESP_ERROR_CHECK(esp_task_wdt_reset());

            auto ws = websocket::getInstance();
            if (!ws->isConnected()) {
                hasConnected = false;
                ws->connect(url, onWebsocketConnect);
            } else {
                hasConnected = true;
            }
            if (hasConnected) {
                delay(100);
            } else {
                delay(5'000);
            }
        }
    }, "reconnect websocket", 8000, (void *) nullptr, 1, nullptr);

    xTaskCreate([](void *parameters) {
        ESP_ERROR_CHECK(esp_task_wdt_add(nullptr));
        string const url = std::string(REMOTE_HOST_WS) + "/api/v1/hub-connect?Board=" + uuid;
        for (;;) {
            LOG("%d\n", __LINE__);
            ESP_ERROR_CHECK(esp_task_wdt_reset());
            LOG("%d\n", __LINE__);
            while (!websocket::getInstance()->isConnected()) {
                LOG("%d\n", __LINE__);
                ESP_ERROR_CHECK(esp_task_wdt_reset());
                delay(100);
                LOG("%d\n", __LINE__);
            }

            // Ping doesn't carry internal data
            FirmwareToBackendPacket packet = {0};
            LOG("%d\n", __LINE__);
            packet.which_type = FirmwareToBackendPacket_ping_tag;
            LOG("%d\n", __LINE__);
            packet.type.ping = Ping{0};
            LOG("%d\n", __LINE__);
            // A ping is 2 bytes [10 0]
            vector<uint8_t> buf(2);
            LOG("%d\n", __LINE__);
            pb_ostream_t output = pb_ostream_from_buffer(buf.data(), buf.size());
            LOG("%d\n", __LINE__);
            int status = pb_encode(&output, FirmwareToBackendPacket_fields, &packet);
            LOG("%d\n", __LINE__);
            if (!status) {
                LOG("%d\n", __LINE__);
                throw std::runtime_error(string("Encoding failed: ") + PB_GET_ERROR(&output));
            }
            LOG("%d\n", __LINE__);
            buf.resize(output.bytes_written);
            LOG("%d\n", __LINE__);
            char buff[20];
            LOG("%d\n", __LINE__);
            time_t now = time(NULL);
            LOG("%d\n", __LINE__);
            strftime(buff, 20, "%Y-%m-%d %H:%M:%S", localtime(&now));
            LOG("%d\n", __LINE__);
            LOG("Sending ping: %s\n", buff);
            WriteSocketError error = websocket::getInstance()->writeBytes(buf, 5'000);
            LOG("%d\n", __LINE__);
            LOG("Error:%d\n", error);
            LOG("%d\n", __LINE__);
            if (error == WriteSocketError::WriteError && websocket::getInstance()->isConnected()) {
                LOG("%d\n", __LINE__);
                throw std::runtime_error("Cannot send data to websocket");
            }
            LOG("%d\n", __LINE__);
            delay(10'000);
            LOG("%d\n", __LINE__);
        }
    }, "Send/rec from server", 8000, (void *) nullptr, 1, nullptr);
}


int tryingToConnect = 0;
bool isConnecting = false;

bool wifiWasConnected = false;

void loop1() {
    if (WiFi.isConnected()) {
        if (!wifiWasConnected) {
            wifiWasConnected = true;
            LOG("WiFi is connected");
        }

        if (!timeSet) {
            LOG("setting clock");
            setClock();
            clientConnectLoop();

            timeSet = true;
        }
    } else {
        if (wifiWasConnected) {
            wifiWasConnected = false;
        }
        Preferences preferences;

        while (!WiFi.isConnected() && !isConnecting) {
            std::string key = getSSID();
            std::string pass = getPassword();

            WiFi.begin(key.c_str(), pass.c_str());

            delay(10'000);

            if (tryingToConnect == 100) {
                throw std::runtime_error("Tried too long to connect. Restarting \n\n");
            }
            tryingToConnect++;
        }
        return;
    }
    if (getSensorData.has_value()) {
        LOG("About to loop\n");
        getSensorData->loop();
    } else {
        LOG("getSensorData has no value\n");
    }
    delay(100);
}

void loop() {
}
