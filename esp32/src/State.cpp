#include "State.h"
#include "generated/packet.pb.h"
#include "pb_decode.h"
#include "DecodeException.h"
#include "StateException.h"
#include <iostream>
#include <memory>
#include <WiFi.h>

using namespace std;

void printVec(optional<vector<uint8_t>> val);

State::State() {
    currentState = nullptr;
    type = Initial;
}

void State::push(const NimBLEAttValue &ch) {
    deque<uint8_t> dequeValue;
    const uint8_t *rawChPointer = ch.data();
    const uint16_t rawChSize = ch.size();
    for (int i = 0; i < rawChSize; i++) {
        dequeValue.push_back(rawChPointer[i]);
    }

    while (!dequeValue.empty()) {
        switch (type) {
            case Initial: {
                type = GetSize;
                array<uint8_t, 4> sizeVector{{}};

                sizeVector.at(0) = dequeValue.front();
                dequeValue.pop_front();
                sizeVector.at(1) = dequeValue.front();
                dequeValue.pop_front();
                sizeVector.at(2) = dequeValue.front();
                dequeValue.pop_front();
                sizeVector.at(3) = dequeValue.front();
                dequeValue.pop_front();

                currentState = {GetSizeClass(sizeVector)};
                break;
            }
            case GetSize: {
                type = GetCommand;
                currentState = {GetCommandClass(std::get<GetSizeClass>(currentState).getSize())};
                break;
            }
            case GetCommand: {

                auto result = std::get<GetCommandClass>(currentState).read(dequeValue);
                if (result.has_value()) {
                    // Packet is huge and takes up too much stack space;
                    auto message = make_unique<Packet>();
                    *message = Packet_init_zero;


                    pb_istream_t stream = pb_istream_from_buffer(result.value().data(), result.value().size());

                    bool status = pb_decode(&stream, Packet_fields, message.get());

                    if (!status) {
                        cerr << "Decoding failed:" << PB_GET_ERROR(&stream) << endl;
                        throw DecodeException();
                    }
                    if (message->which_type == Packet_getWifi_tag) {
                        vector<WiFiStorage> wifis;
                        int n = WiFi.scanNetworks();
                        for (int i = 0; i < n; i++) {
                            wifis.emplace_back(WiFi.SSID(i).c_str(), WiFi.BSSID(i), WiFi.channel(i),
                                               WiFi.encryptionType(i) != WIFI_AUTH_OPEN);
                            delay(10);
                        }
                        Serial.write("About to send");

                        type = SendWifiDataSize;
                        currentState = {SendWifiDataClass(wifis)};
                    } else {
                        cerr << "Unknown packet type=" << message->which_type << endl;
                        throw DecodeException();
                    }
                }
                break;
            }
            default: {
                cerr << "Pushing Type=" << type << endl;
                throw StateException();
            }
        }
    }
}

vector<uint8_t> State::getPacket() {
    switch (type) {
        case SendWifiDataSize: {
            auto wifiData = get<SendWifiDataClass>(currentState);
            currentState = {wifiData};
            vector<uint8_t> size = wifiData.getSize();
            type = SendWifiData;
            return size;

        }
        case SendWifiData: {
            auto wifiData = get<SendWifiDataClass>(currentState);
            auto returnVal = wifiData.getData(126);
            currentState = {wifiData};

            if (returnVal.has_value()) {
                return returnVal.value();
            } else {
                type = Initial;
                currentState = nullptr;
                return {};
            }
        }
        default:
            cerr << "Getting Type=" << type << endl;
            throw StateException();
    }
}

void printVec(optional<vector<uint8_t>> val) {
    if (!val.has_value()) {
        Serial.print("Has no value\n");
    } else {
        vector<uint8_t> v = val.value();
        for (int i = 0; i < v.size(); i++) {
            Serial.print("v[");
            Serial.print(i);
            Serial.print("]=");
            Serial.print((int) v[i]);
            Serial.print("\n");
        }
    }

}
