//#include <esp_websocket_client.h>
#include <memory>
#include <utility>
#include <esp_event_base.h>
#include <esp_websocket_client.h>
#include "websocket.h"
#include "lib/log.h"
#include "lib/mutex.h"

void websocket_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data);

websocket *websocket::getInstance() {
    static websocket w;
    return &w;
}

WriteSocketError websocket::writeBytes(const std::vector<uint8_t> &bytes, int msToTimeut) {
    static_assert(CHAR_BIT == 8);
    auto lockedSocket = socket.lock();
    if (lockedSocket->has_value()) {
        ESP_ERROR_CHECK(gpio_set_level(GPIO_NUM_18, 1));
        int const result = esp_websocket_client_send_bin(lockedSocket->value(),
                                                         reinterpret_cast<const char *>(bytes.data()),
                                                         bytes.size(),
                                                         pdMS_TO_TICKS(msToTimeut));
        ESP_ERROR_CHECK(gpio_set_level(GPIO_NUM_18, 0));
        if (result == ESP_FAIL) {
            return WriteSocketError::WriteError;
        }
        return WriteSocketError::Ok;
    } else {
        return WriteSocketError::NotInitialized;
    }

}

safe_std::mutex<std::function<void(const WebsocketConnectionType, int, const char *)>> onCallGlobal;

bool websocket::connect(const std::string &url,
                        std::function<void(const WebsocketConnectionType, int, const char *)> onCall) {
    auto lockedSocket = socket.lock();

    LOG("About to disconnect\n");
    if (lockedSocket->has_value()) {
        esp_websocket_client_close(lockedSocket->value(), pdMS_TO_TICKS(1'000));
        ESP_ERROR_CHECK(esp_websocket_client_destroy(lockedSocket->value()));

        lockedSocket->reset();
    }
    const esp_websocket_client_config_t ws_cfg = {.uri = url.c_str(),};
    esp_websocket_client_handle_t websocket_client = esp_websocket_client_init(&ws_cfg);
    if (websocket_client == nullptr) {
        return false;
    }
    onCallGlobal.lockAndSwap(onCall);
    ESP_ERROR_CHECK(
            esp_websocket_register_events(websocket_client, WEBSOCKET_EVENT_ANY, websocket_event_handler, nullptr));
    esp_err_t const error = esp_websocket_client_start(websocket_client);
    if (error != ERR_OK) {
        LOG("Websocket start error: %d\n", error);
        return false;
    }
    *lockedSocket = websocket_client;
    return true;
}

bool websocket::isConnected() {
    auto lockedSocket = socket.lock();
    return lockedSocket->has_value() && esp_websocket_client_is_connected(lockedSocket->value());
}


// This function will be called whenever the websocket receives an event.
void websocket_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data) {
    // Based on https://github.com/espressif/esp-idf/blob/v3.3.4/examples/protocols/websocket/main/websocket_example.c
    auto *data = reinterpret_cast<esp_websocket_event_data_t *>(event_data);
    auto call = onCallGlobal.lock();
    switch (event_id) {

        case WEBSOCKET_EVENT_ANY:
            (*call)(WebsocketConnectionType::Any, data->data_len, data->data_ptr);
            break;
        case WEBSOCKET_EVENT_ERROR:
            (*call)(WebsocketConnectionType::Error, data->data_len, data->data_ptr);
            break;
        case WEBSOCKET_EVENT_CONNECTED:
            (*call)(WebsocketConnectionType::Connected, data->data_len, data->data_ptr);
            break;
        case WEBSOCKET_EVENT_DISCONNECTED:
            (*call)(WebsocketConnectionType::Disconnected, data->data_len, data->data_ptr);
            break;
        case WEBSOCKET_EVENT_DATA:
            (*call)(WebsocketConnectionType::Data, data->data_len, data->data_ptr);
            break;
        case WEBSOCKET_EVENT_CLOSED:
            (*call)(WebsocketConnectionType::Closed, data->data_len, data->data_ptr);
            break;
        case WEBSOCKET_EVENT_MAX:
            (*call)(WebsocketConnectionType::Max, data->data_len, data->data_ptr);
            break;
        default:

            throw std::runtime_error(std::string("Assertion error: event_id = ") + std::to_string(event_id) +
                                     "which is not a valid id\n");
    }

}
