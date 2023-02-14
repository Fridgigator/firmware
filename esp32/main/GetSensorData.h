#ifndef ESP32_SRC_GETSENSORDATA_H_
#define ESP32_SRC_GETSENSORDATA_H_

#include <deque>
#include <string>
#include <optional>
#include "TypeOfDevice.h"
#include "NimBLEDevice.h"
#include "lib/mutex.h"

/// GetSensorData is a singleton object that continuously polls the sensors and sends data to the server
/// A pointer to the object can be obtained using `getGetSensorData()`
class GetSensorData {
private:
    GetSensorData();

public:
    safe_std::mutex<std::unordered_map<std::string, NimBLEClient *>> pClient; 

    /// loop is where the majority of the work takes place. It collects data and sends it from here.
    void loop();

    /// clearDevices clears the list of devices that this device should connect to
    void clearDevices();

    /// setDevices sets the list of devices that this device should connect to.
    /// devices is a const reference to a vector which contains the string id of the device and a
    /// device type
    void setDevices(std::vector<std::tuple<std::string, TypeOfDevice>> &devices);

    friend GetSensorData *getGetSensorData();
};

#endif //ESP32_SRC_GETSENSORDATA_H_

/// This returns a static pointer to a GetSensorData singleton
/// The pointer has static lifetime and should not be deleted
GetSensorData *getGetSensorData();
