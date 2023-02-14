#pragma once

#include <optional>
#include <functional>
#include "esp_websocket_client.h"
#include "lib/mutex.h"

/// This represents the different type of connections
enum WebsocketConnectionType {
    Any, Error, Connected, Disconnected, Data, Closed, Max
};

enum WriteSocketError {
    Ok, WriteError, NotInitialized
};

/// websocket is a wrapper around the esp_websocket_* libraries.
/// We don't need to wrap it in a mutex since it's thread safe out of the box.
class websocket {
private:
    // Is behind a mutex since we can write to a socket from multiple threads
    safe_std::mutex<std::optional<esp_websocket_client_handle_t>> socket;

    websocket() = default;

public:
    /// Gets a singleton instance. The websocket pointer has a static lifetime.
    static websocket *getInstance();

    /// Connects to the server. On call is called when a websocket connection event occurs.
    /// It may be called after connect returns, so make sure to copy or move all captured references.
    bool connect(const std::string &url,
                 const std::function<void(const WebsocketConnectionType, int, const char *)> &onCall);

    /// Writes bytes as a binary value.
    WriteSocketError writeBytes(const std::vector<uint8_t> &bytes, int msToTimeut);

    [[nodiscard]] bool isConnected();
};

