#ifndef ESP32_STATETYPE_H
#define ESP32_STATETYPE_H
enum StateType {
  Initial,
  GetSize,
  GetCommand,
  SendWifiDataSize,
  SendWifiData,
  GetWifiUsernamePasswordSize,
  GetWifiUsernamePassword,
  ConnectingToWiFiStateSendSize,
  ConnectingToWiFiState
};

#endif //ESP32_STATETYPE_H
