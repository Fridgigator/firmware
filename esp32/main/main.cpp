#include "NimBLEDevice.h"
#include <vector>
#include <cstring>
#include <map>
#include <esp_websocket_client.h>
#include <esp_task_wdt.h>
#include <esp_netif.h>
#include <esp_wifi.h>
#include <freertos/event_groups.h>

#include "exceptions/StateException.h"
#include "uuid.h"
#include "Constants.h"
#include "GetSensorData.h"
#include "generated/firmware_backend.pb.h"
#include "../components/nanopb/pb_decode.h"
#include "ScanResults.h"
#include "setClock.h"
#include "lib/log.h"
#include "lib/websocket/websocket.h"
#include "secrets.h"
#include "../components/nanopb/pb_encode.h"
#include "driver/gpio.h"
#include "lib/ArduinoSupport/Preferences.h"

using namespace std;

NimBLECharacteristic *pRead = nullptr;

safe_std::mutex<bool> *isWiFiConnected() {
    static auto returnVal = safe_std::mutex(false);
    return &returnVal;
}

safe_std::mutex<bool> *timeSet() {
    static auto returnVal = safe_std::mutex(false);
    return &returnVal;
}

void loop();

void clientConnectLoop();

/// initialize_wifi starts the wifi system and connects
void initialize_wifi();

string *uuid() {
    static auto returnVal = string();
    return &returnVal;
};

/// app_main is the entry point of the program. It must be extern "C" for the linker to find it.
/// All setup should be done here.

extern "C" [[noreturn]] void app_main() {
    // Catch the reason the device restarted.
    // TODO: We should send this to the server for logging
    auto reason = esp_reset_reason();
    switch (reason) {
        case ESP_RST_UNKNOWN:
            printf("Restart reason: UNKNOWN\n");
            break;
        case ESP_RST_POWERON:
            printf("Restart reason: ESP_RST_POWERON\n");
            break;
        case ESP_RST_EXT:
            printf("Restart reason: EXT\n");
            break;
        case ESP_RST_SW:
            printf("Restart reason: ESP_RST_SW\n");
            break;
        case ESP_RST_PANIC:
            printf("Restart reason: ESP_RST_PANIC\n");
            break;
        case ESP_RST_INT_WDT:
            printf("Restart reason: ESP_INT_WDT\n");
            break;
        case ESP_RST_TASK_WDT:
            printf("Restart reason: ESP_TASK_WDT\n");
            break;
        case ESP_RST_WDT:
            printf("Restart reason: ESP_EST_WDT\n");
            break;
        case ESP_RST_DEEPSLEEP:
            printf("Restart reason: ESP_RST_DEEPSLEEP\n");
            break;
        case ESP_RST_BROWNOUT:
            printf("Restart reason: ESP_RST_BROWNOUT\n");
            break;
        case ESP_RST_SDIO:
            printf("Restart reason: ESP_RST_SDIO\n");
            break;
        default:
            printf("Restart reason: We don't know: %d\n", reason);
            break;
    }
    // Blink all lights for 3 seconds
    gpio_config_t config = {.pin_bit_mask = (1ULL << GPIO_NUM_23) | (1ULL << GPIO_NUM_19) | (1ULL << GPIO_NUM_18) |
                                            (1ULL
                                                    << GPIO_NUM_17), .mode =  GPIO_MODE_OUTPUT, .pull_up_en = GPIO_PULLUP_DISABLE, .pull_down_en = GPIO_PULLDOWN_DISABLE, .intr_type = GPIO_INTR_DISABLE,

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

    // Turn on red LED when we're connected to the LED.
    // This allocates on the heap but that should be OK since this only runs once and would panic
    // if it can't allocate.
    xTaskCreate([](void *arg) {
        for (;;) {
            // Is wifi connected
            bool _isWiFiConnected;
            {
                _isWiFiConnected = *isWiFiConnected()->lock();
            }
            // Is time set up
            bool _isTimeSet;
            {
                _isTimeSet = *timeSet()->lock();
            }
            // If wifi is connected, turn on red led. Otherwise, turn it off.
            if (_isWiFiConnected && _isTimeSet) {
                ESP_ERROR_CHECK(gpio_set_level(GPIO_NUM_23, 1));
            } else {
                ESP_ERROR_CHECK(gpio_set_level(GPIO_NUM_23, 0));
            }

            // Are we connected to the websocket? If yes, turn on white led. Otherwise, turn it off
            if (websocket::getInstance()->isConnected()) {
                ESP_ERROR_CHECK(gpio_set_level(GPIO_NUM_17, 1));
            } else {
                ESP_ERROR_CHECK(gpio_set_level(GPIO_NUM_17, 0));
            }
            // Keep checking every quarter of a second
            delay(250);
        }
    }, "WiFi loop", 1024, nullptr, 1, nullptr);

    // Ensure that our uuid is setup
    Preferences preferences;
    preferences.begin("permanent");
    // If it wasn't set up
    if (!preferences.isKey("uuid")) {
        // Generate the uuid and save it
        char returnUUID[37];
        UUIDGen(returnUUID);
        preferences.putString("uuid", returnUUID);
    }
    // If the uuid wasn't set up, it is now
    auto optionalUUID = preferences.getString("uuid");
    // If it wasn't for some reason, panic
    assert(optionalUUID.has_value());
    *uuid() = optionalUUID.value();
    preferences.end();

    // The device name is ESP-UUID name (trimmed to 15 chars). This makes it easier to find when looking for a device
    // (If you have two esp32s, which one is which)?
    std::string name = "ESP-" + *uuid();
    name.erase(15, std::string::npos);

    NimBLEDevice::init(name);
    NimBLEServer *pServer = BLEDevice::createServer();
    NimBLEService *pService = pServer->createService(SERVICE_UUID);

    // We will want to read from our BLE device when we re-enable our mesh network
    pRead = pService->createCharacteristic(CHARACTERISTIC_SERVER_UUID,
                                           NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::INDICATE);
    pService->start();
    NimBLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
    pAdvertising->addServiceUUID(SERVICE_UUID);
    pAdvertising->setScanResponse(true);
    NimBLEDevice::startAdvertising();

    delay(100);
    TaskHandle_t Task2;

    xTaskCreate([](void *) {
        for (;;) {
            loop();
            delay(100);
        }
    }, "Main Loop Task", 8000, nullptr, 1, &Task2);

    LOG("Finished Setup!");

    initialize_wifi();
    LOG("Finished WiFi\n");
    setClock();
    clientConnectLoop();
    for (;;) {
        yield();
        delay(100);
    }
}

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
    SensorsList sensorsList{.sensor_infos_count = static_cast<pb_size_t>(sensorInfo.size())};

    for (int i = 0;
         i < sensorInfo.size() && i < sizeof(sensorsList.sensor_infos) / sizeof(sensorsList.sensor_infos[0]); i++) {

        sensorsList.sensor_infos[i] = sensorInfo.at(i);

    }
    int encodeResult = pb_encode(&output, SensorsList_fields, &sensorsList);
    if (!encodeResult) {
        throw std::runtime_error(string("Encoding failed: ") + PB_GET_ERROR(&output));
    }
    std::map<string, string> headers;
    headers.emplace("Board", *uuid());
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
    getGetSensorData()->setDevices(newDevices);
}

void recData(unique_ptr<BackendToFirmwarePacket> &packet) {
    TaskHandle_t Task3;

    // I *think* that this has to be a thread because it's called from a websocket callback. It seems that there's
    // some kind of deadlock going on here if I don't
    // TODO: verify if there is a requirement to have this as its own thread.

    auto ret = xTaskCreate([](void *arg) {
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
                    getGetSensorData()->clearDevices();
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


        vTaskDelete(nullptr);
    }, "recData", 16000, packet.release(), 1, &Task3);
    if (ret != pdPASS) {
        LOG("Error: %d\n", ret);
        throw runtime_error("Couldn't create thread");
    }
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

/// clientConnectLoop starts the thread responsible for communicating with the backend.
/// By making it a thread, I can control how large of a stack space it gets. I can also pause it
/// until either it needs to ping or it needs to send data
/// It returns the TaskID to which data should be sent.

/// clientConnectLoop allocates is thread stack space on the heap, but it's only called once and panics if it
/// doesn't start.
void clientConnectLoop() {

    auto websocketConnect = xTaskCreate([](void *parameters) {
        string const url = std::string(REMOTE_HOST_WS) + "/api/v1/hub-connect?Board=" + *uuid();

        for (;;) {

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
                for (int i = 0; i < 10; i++) {
                    delay(6'000);
                }
            }
        }
    }, "reconnect websocket", 8000, (void *) nullptr, 1, nullptr);
    if (websocketConnect != pdPASS) {
        LOG("websocketConnect: %d\n", websocketConnect);
    }
    auto ping = xTaskCreate([](void *parameters) {
        string const url = std::string(REMOTE_HOST_WS) + "/api/v1/hub-connect?Board=" + *uuid();
        for (;;) {
            // Wait for a reconnect
            while (!websocket::getInstance()->isConnected()) {
                delay(100);
            }
            // Ping doesn't carry internal data
            FirmwareToBackendPacket packet = {0};
            packet.which_type = FirmwareToBackendPacket_ping_tag;
            packet.type.ping = Ping{0};
            // A ping is 2 bytes [10 0]
            vector<uint8_t> buf(2);
            pb_ostream_t output = pb_ostream_from_buffer(buf.data(), buf.size());
            int status = pb_encode(&output, FirmwareToBackendPacket_fields, &packet);
            if (!status) {
                throw std::runtime_error(string("Encoding failed: ") + PB_GET_ERROR(&output));
            }
            buf.resize(output.bytes_written);
            char buff[20];
            time_t now = time(nullptr);
            strftime(buff, 20, "%Y-%m-%d %H:%M:%S", localtime(&now));
            LOG("Sending ping: %s\n", buff);
            WriteSocketError error = websocket::getInstance()->writeBytes(buf, 5'000);
            LOG("Error:%d\n", error);
            if (error == WriteSocketError::WriteError && websocket::getInstance()->isConnected()) {
                LOG("%d\n", __LINE__);
                throw std::runtime_error("Cannot send data to websocket");
            }
            // Wait for 2 minutes
            for (int i = 0; i < 100; i++) {
                delay(1'200);
            }
        }
    }, "Send/rec from server", 8000, (void *) nullptr, 1, nullptr);
    if (ping != pdPASS) {
        LOG("ping: %d\n", ping);
    }
}

void loop() {
    getGetSensorData()->loop();
    delay(100);
}

int *s_retry_num() {
    static int returnVal;
    return &returnVal;
};

// Get an event from the WiFi subsystem. Most of this code is borrowed from the ESP website

static void event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data) {
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        isWiFiConnected()->lockAndSwap(false);
        if (*s_retry_num() < 100) {
            esp_wifi_connect();
            (*s_retry_num())++;
            LOG("retry to connect to the AP\n");
        } else {
            LOG("Can't connect, restart\n");
            esp_restart();
        }
        LOG("connect to the AP fail");
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        auto *event = (ip_event_got_ip_t *) event_data;
        LOG("got ip: %d.%d.%d.%d\n", IP2STR(&event->ip_info.ip));
        (*s_retry_num()) = 0;
        isWiFiConnected()->lockAndSwap(true);
        setClock();
        timeSet()->lockAndSwap(true);
    }
}

void initialize_wifi() {
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    esp_event_handler_instance_t instance_any_id;
    esp_event_handler_instance_t instance_got_ip;
    ESP_ERROR_CHECK(
            esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &event_handler, NULL, &instance_any_id));
    ESP_ERROR_CHECK(
            esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &event_handler, NULL, &instance_got_ip));

    wifi_config_t wifi_config = {0};

    // ssid cannot be more than 32 chars and the password cannot be more than 64
    strncpy(reinterpret_cast<char *>(wifi_config.sta.ssid), SSID, 32);
    strncpy(reinterpret_cast<char *>(wifi_config.sta.password), PASSWORD, 64);
    wifi_config.sta.threshold = {0};
    wifi_config.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    LOG("wifi_init_sta finished.\n");
}
