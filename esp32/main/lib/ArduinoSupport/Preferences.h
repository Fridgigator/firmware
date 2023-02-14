

#ifndef ESP32_PREFERENCES_H
#define ESP32_PREFERENCES_H

#include <string>
#include <mutex>
#include <nvs_flash.h>


/// Preferences class gets preferences data. We use it only in one place so it's not done as well as it could be.
/// It would be nice if it would write on RAII.
/// Issues:
/// Is not thread safe. It needs to be called from behind a mutex if it's called from multiple threads.
class Preferences {
    nvs_handle_t handle{};
public:
    /// Panics: If it can't initialize nvs_flash
    Preferences();
    /// Panics: If it can't initialize nvs_flash
    ~Preferences();
    /// Begin loads a category
    /// Panics: If unable to begin
    void begin(const std::string &category);

    /// End closes a category
    void end() const;

    /// getString returns a string from a given key
    /// Returns an empty optional if key is not found
    [[nodiscard]] std::optional<std::string> getString(const std::string &key) const;

    /// getString puts a key value into the preferences store
    /// Panics: If unable to put string
    void putString(const std::string &key, const std::string &val) const;

    /// isKey checks if the key exists
    /// Panics: If unable to get a string value unless it's because it doesn't exist
    [[nodiscard]] bool isKey(const std::string &key) const;
};

#endif //ESP32_PREFERENCES_H
