
#include <vector>
#include <optional>
#include "Preferences.h"

void Preferences::begin(const std::string &category) {
    ESP_ERROR_CHECK(nvs_open(category.c_str(), NVS_READWRITE, &handle));
}

void Preferences::end() const {
    nvs_close(handle);
}

std::optional<std::string> Preferences::getString(const std::string &key) const {
    size_t required_size;
    // If out_value is nullptr, required_size is set to the final length value
    esp_err_t err = nvs_get_str(handle, key.c_str(), nullptr, &required_size);
    if (err == ESP_ERR_NVS_NOT_FOUND) {
        return std::nullopt;
    } else {
        std::vector<char> value(required_size);
        ESP_ERROR_CHECK(nvs_get_str(handle, key.c_str(), value.data(), &required_size));
        return value.data();
    }
}

void Preferences::putString(const std::string &key, const std::string &val) const {
    ESP_ERROR_CHECK(nvs_set_str(handle, key.c_str(), val.c_str()));
    nvs_commit(handle);
}

bool Preferences::isKey(const std::string &key) const {
    size_t required_size;
    // If out_value is nullptr, required_size is set to the final length value
    esp_err_t err = nvs_get_str(handle, key.c_str(), nullptr, &required_size);
    if (err == ESP_OK) {
        return true;
    }
    if (err == ESP_ERR_NVS_NOT_FOUND) {
        return false;
    }
    ESP_ERROR_CHECK(err);
    return "";
}

Preferences::Preferences() {
    ESP_ERROR_CHECK(nvs_flash_init());
}

Preferences::~Preferences() {
    ESP_ERROR_CHECK(nvs_flash_deinit());

}
